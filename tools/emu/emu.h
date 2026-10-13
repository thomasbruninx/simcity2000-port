// Dev-only: run routines of the user's SIMCITY.EXE (build .022) inside the Unicorn CPU emulator so ports can be
// diff-tested against the original machine code. Not part of the shipped engine (Unicorn is GPL-2.0).
#pragma once
#include <unicorn/unicorn.h>

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "resources/pe_image.h"

namespace sc2k::emu {

class Emu {
public:
    explicit Emu(const std::string& exePath);
    ~Emu();
    Emu(const Emu&) = delete;
    Emu& operator=(const Emu&) = delete;

    // memory
    uint32_t alloc(size_t bytes);                       // zeroed, 16-byte aligned bump allocation
    void write(uint32_t va, const void* data, size_t n);
    void read(uint32_t va, void* out, size_t n) const;
    template <typename T> void put(uint32_t va, T v) { write(va, &v, sizeof v); }
    template <typename T> T get(uint32_t va) const { T v; read(va, &v, sizeof v); return v; }

    // run `fn` (cdecl/stdcall style: args pushed right-to-left, stack reset afterwards). Stops when `fn` returns or
    // when execution reaches `stopAt` (0 = none). Returns EAX. Throws on emulator errors with a diagnostic.
    uint32_t call(uint32_t fn, std::initializer_list<uint32_t> args = {}, uint32_t ecx = 0, uint32_t edx = 0, uint32_t stopAt = 0);

    // Replace CRT rand()/srand() (0x48BCD8 / 0x48BCC8) by machine code operating on a state word we control.
    void installCrtRand(uint32_t seed);
    uint32_t crtRandState() const { return get<uint32_t>(kRandState); }
    void patchRet(uint32_t va) { uint8_t c3 = 0xC3; write(va, &c3, 1); }   // turn a UI/CRT routine into "ret"
    void patchRetN(uint32_t va, uint16_t n) { uint8_t c[3] = {0xC2, uint8_t(n), uint8_t(n >> 8)}; write(va, c, 3); }   // "ret n" (callee-cleaned)
    uint64_t instructionCount() const { return insnCount_; }

    static constexpr uint32_t kRandState = 0x7E000000;

    uc_engine* engine() { return uc_; }
private:
    uc_engine* uc_ = nullptr;
    uint32_t heapNext_ = 0x10000000, heapEnd_ = 0x10000000 + 0x04000000;
    uint64_t insnCount_ = 0;
    uc_hook hook_ = 0;
};

// Layer helpers: the game stores maps as a table of row pointers (layer[a][b]).
struct LayerRef { uint32_t table; int rows, cols, elem; };   // elem = bytes per element
LayerRef makeLayer(Emu& e, uint32_t tableVA, int rows, int cols, int elemBytes);
void uploadBytes(Emu& e, const LayerRef& l, const uint8_t* data);        // row-major rows*cols bytes
void uploadShorts(Emu& e, const LayerRef& l, const uint16_t* data);
void downloadBytes(const Emu& e, const LayerRef& l, uint8_t* out);
void downloadShorts(const Emu& e, const LayerRef& l, uint16_t* out);

}  // namespace sc2k::emu
