#include <cstdlib>
#include <cstdio>
#include "emu/emu.h"

#include <cstring>

namespace sc2k::emu {
static uint32_t g_ring[48]; static int g_ringPos = 0;
namespace {
constexpr uint32_t kStackBase = 0x7FF00000, kStackSize = 0x00100000, kStackTop = 0x7FFF0000;
constexpr uint32_t kSentinel = 0x7FFFF800;
constexpr uint32_t kFakeBase = 0x7E000000, kFakeSize = 0x00200000;   // rand state + fake import targets

void check(uc_err e, const char* what) {
    if (e != UC_ERR_OK) throw std::runtime_error(std::string("unicorn: ") + what + ": " + uc_strerror(e));
}
}  // namespace

Emu::Emu(const std::string& exePath) {
    const PeImage pe = PeImage::load(exePath);
    if (pe.timestamp() != kTimestampWin95Cd1995) throw std::runtime_error("unsupported SIMCITY.EXE build");
    check(uc_open(UC_ARCH_X86, UC_MODE_32, &uc_), "uc_open");
    const uint32_t base = pe.imageBase();
    const uint32_t imageSize = (pe.sizeOfImage() + 0xFFF) & ~0xFFFu;
    check(uc_mem_map(uc_, base, imageSize, UC_PROT_ALL), "map image");
    for (const auto& s : pe.sections()) {
        const uint32_t n = std::min(s.rawSize, s.vsize ? s.vsize : s.rawSize);
        if (n && s.rawPtr + n <= pe.file().size()) check(uc_mem_write(uc_, base + s.va, pe.file().data() + s.rawPtr, n), "write section");
    }
    if (std::getenv("EMUTRACE"))
        uc_hook_add(uc_, &hook_, UC_HOOK_CODE, reinterpret_cast<void*>(+[](uc_engine*, uint64_t addr, uint32_t, void*) { g_ring[g_ringPos++ % 48] = uint32_t(addr); }), nullptr, 1, 0);
    check(uc_mem_map(uc_, kStackBase, kStackSize, UC_PROT_ALL), "map stack");
    check(uc_mem_map(uc_, kFakeBase, kFakeSize, UC_PROT_ALL), "map fake region");
    check(uc_mem_map(uc_, heapNext_, heapEnd_ - heapNext_, UC_PROT_ALL), "map heap");
    // Imports: point every IAT slot at a fake address filled with HLT so an unexpected API call stops the run loudly.
    {
        const uint8_t hlt = 0xF4;
        for (uint32_t a = kFakeBase + 0x1000; a < kFakeBase + 0x1000 + 0x1000; ++a) uc_mem_write(uc_, a, &hlt, 1);
    }
}

Emu::~Emu() { if (uc_) uc_close(uc_); }

uint32_t Emu::alloc(size_t bytes) {
    const uint32_t a = heapNext_;
    heapNext_ += static_cast<uint32_t>((bytes + 15) & ~size_t(15));
    if (heapNext_ > heapEnd_) throw std::runtime_error("emulator heap exhausted");
    return a;
}

void Emu::write(uint32_t va, const void* data, size_t n) { check(uc_mem_write(uc_, va, data, n), "mem write"); }
void Emu::read(uint32_t va, void* out, size_t n) const { check(uc_mem_read(uc_, va, out, n), "mem read"); }

uint32_t Emu::call(uint32_t fn, std::initializer_list<uint32_t> args, uint32_t ecx, uint32_t edx, uint32_t stopAt) {
    uint32_t esp = kStackTop;
    std::vector<uint32_t> a(args);
    for (size_t i = a.size(); i-- > 0;) { esp -= 4; put<uint32_t>(esp, a[i]); }
    esp -= 4; put<uint32_t>(esp, kSentinel);
    uc_reg_write(uc_, UC_X86_REG_ESP, &esp);
    uc_reg_write(uc_, UC_X86_REG_EBP, &esp);
    uc_reg_write(uc_, UC_X86_REG_ECX, &ecx);
    uc_reg_write(uc_, UC_X86_REG_EDX, &edx);
    // x87 control word 0x27F: 53-bit precision, round to nearest (what the MSVC runtime sets at start-up)
    {
        const uint32_t eflags = 0x202; uc_reg_write(uc_, UC_X86_REG_EFLAGS, &eflags);
        const uint16_t fpcw = 0x27F; uc_reg_write(uc_, UC_X86_REG_FPCW, &fpcw);
    }
    uc_err err = uc_emu_start(uc_, fn, stopAt ? stopAt : kSentinel, 0, 0);
    if (err != UC_ERR_OK) {
        uint32_t eip = 0; uc_reg_read(uc_, UC_X86_REG_EIP, &eip);
        char msg[160]; std::snprintf(msg, sizeof msg, "emulation failed in %#x at EIP=%#x: %s", fn, eip, uc_strerror(err));
        if (std::getenv("EMUTRACE")) { std::fprintf(stderr, "last EIPs:"); for (int i = 0; i < 48; ++i) std::fprintf(stderr, " %x", g_ring[(g_ringPos + i) % 48]); std::fprintf(stderr, "\n"); }
        throw std::runtime_error(msg);
    }
    uint32_t eax = 0; uc_reg_read(uc_, UC_X86_REG_EAX, &eax);
    return eax;
}

void Emu::installCrtRand(uint32_t seed) {
    put<uint32_t>(kRandState, seed);
    const uint32_t st = kRandState;
    auto le32 = [](std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back(uint8_t(x >> (8 * i))); };
    {   // srand(seed): mov eax,[esp+4]; mov [st],eax; ret
        std::vector<uint8_t> c{0x8B, 0x44, 0x24, 0x04, 0xA3}; le32(c, st); c.push_back(0xC3);
        write(0x48BCC8, c.data(), c.size());
    }
    {   // rand(): mov eax,[st]; imul eax,eax,0x343FD; add eax,0x269EC3; mov [st],eax; shr eax,16; and eax,0x7FFF; ret
        std::vector<uint8_t> c{0xA1}; le32(c, st);
        c.insert(c.end(), {0x69, 0xC0}); le32(c, 0x343FD);
        c.push_back(0x05); le32(c, 0x269EC3);
        c.push_back(0xA3); le32(c, st);
        c.insert(c.end(), {0xC1, 0xE8, 0x10});
        c.push_back(0x25); le32(c, 0x7FFF);
        c.push_back(0xC3);
        write(0x48BCD8, c.data(), c.size());
    }
}

LayerRef makeLayer(Emu& e, uint32_t tableVA, int rows, int cols, int elemBytes) {
    const uint32_t data = e.alloc(static_cast<size_t>(rows) * cols * elemBytes);
    for (int r = 0; r < rows; ++r) e.put<uint32_t>(tableVA + 4u * r, data + static_cast<uint32_t>(r) * cols * elemBytes);
    return {tableVA, rows, cols, elemBytes};
}
void uploadBytes(Emu& e, const LayerRef& l, const uint8_t* d) {
    for (int r = 0; r < l.rows; ++r) e.write(e.get<uint32_t>(l.table + 4u * r), d + static_cast<size_t>(r) * l.cols, l.cols);
}
void uploadShorts(Emu& e, const LayerRef& l, const uint16_t* d) {
    for (int r = 0; r < l.rows; ++r) e.write(e.get<uint32_t>(l.table + 4u * r), d + static_cast<size_t>(r) * l.cols, 2u * l.cols);
}
void downloadBytes(const Emu& e, const LayerRef& l, uint8_t* out) {
    for (int r = 0; r < l.rows; ++r) e.read(e.get<uint32_t>(l.table + 4u * r), out + static_cast<size_t>(r) * l.cols, l.cols);
}
void downloadShorts(const Emu& e, const LayerRef& l, uint16_t* out) {
    for (int r = 0; r < l.rows; ++r) e.read(e.get<uint32_t>(l.table + 4u * r), out + static_cast<size_t>(r) * l.cols, 2u * l.cols);
}

}  // namespace sc2k::emu
