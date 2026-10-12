#include "emu/sim_image.h"

#include <cstring>

namespace sc2k::emu {

SimImage::SimImage(Emu& emu) : e(emu) {
    xbld = makeLayer(e, 0x4CB508, 128, 128, 1); xzon = makeLayer(e, 0x4C9208, 128, 128, 1); xbit = makeLayer(e, 0x4C9B28, 128, 128, 1);
    xtxt = makeLayer(e, 0x4C9618, 128, 128, 1); xund = makeLayer(e, 0x4CA1E8, 128, 128, 1); xter = makeLayer(e, 0x4C8F70, 128, 128, 1);
    altm = makeLayer(e, 0x4C9E28, 128, 128, 2);
    xtrf = makeLayer(e, 0x4C9958, 64, 64, 1); xplt = makeLayer(e, 0x4C9840, 64, 64, 1); xval = makeLayer(e, 0x4CA0C0, 64, 64, 1); xcrm = makeLayer(e, 0x4C94F0, 64, 64, 1);
    xpop = makeLayer(e, 0x4C9460, 32, 32, 1); xrog = makeLayer(e, 0x4CA040, 32, 32, 1); xplc = makeLayer(e, 0x4C8448, 32, 32, 1); xfir = makeLayer(e, 0x4C9A90, 32, 32, 1);
    tmp = makeLayer(e, 0x4CB710, 128, 128, 2);
    census = e.alloc(512); zonePop = e.alloc(32); c9948 = e.alloc(16); budget = e.alloc(16 * 0x70); milCounts = e.alloc(32); micro = e.alloc(1200); things = e.alloc(64 * 12); labels = e.alloc(6400); fakeView = e.alloc(0x400);
    e.put<uint32_t>(0x4C94E0, census); e.put<uint32_t>(0x4C9440, zonePop); e.put<uint32_t>(0x4C9948, c9948); e.put<uint32_t>(0x4CB4E4, budget);
    e.put<uint32_t>(0x4C994C, milCounts);
    e.put<uint32_t>(0x4CA404, micro);        // points at record 0 (micro id 0x33); the code adds id*8 - 0x198
    e.put<uint32_t>(0x4C944C, things);
    e.put<uint32_t>(0x4C91B0, labels);
    e.put<uint32_t>(0x4CA1D0, e.alloc(12000));   // sprite table (heights only feed drawing coordinates): zeroed
    e.call(0x456CE0);   // fills the 24-entry neighbourhood offset table (static init in the game)
}

void SimImage::load(const World& w, const CityState& s) {
    uploadBytes(e, xbld, w.xbld.v.data()); uploadBytes(e, xzon, w.xzon.v.data()); uploadBytes(e, xbit, w.xbit.v.data());
    uploadBytes(e, xtxt, w.xtxt.v.data()); uploadBytes(e, xund, w.xund.v.data()); uploadBytes(e, xter, w.xter.v.data());
    uploadShorts(e, altm, w.altm.v.data());
    uploadBytes(e, xtrf, w.xtrf.v.data()); uploadBytes(e, xplt, w.xplt.v.data()); uploadBytes(e, xval, w.xval.v.data()); uploadBytes(e, xcrm, w.xcrm.v.data());
    uploadBytes(e, xpop, w.xpop.v.data()); uploadBytes(e, xrog, w.xrog.v.data()); uploadBytes(e, xplc, w.xplc.v.data()); uploadBytes(e, xfir, w.xfir.v.data());
    e.write(micro, w.xmic.data(), 1200); e.write(labels, w.xlab.data(), 6400); e.write(things, w.xthg.data(), 480);
    for (int i = 0; i < 256; ++i) e.put<int16_t>(census + 2u * i, s.census[static_cast<size_t>(i)]);
    for (int i = 0; i < 8; ++i) e.put<int32_t>(zonePop + 4u * i, s.zonePop[static_cast<size_t>(i)]);
    for (int i = 0; i < 16; ++i) e.put<int16_t>(milCounts + 2u * i, s.militaryCounts[static_cast<size_t>(i)]);
    for (int k = 0; k < 16; ++k) {
        const BudgetRecord& r = s.budget[static_cast<size_t>(k)]; const uint32_t b = budget + 0x70u * k;
        for (int m = 0; m < 12; ++m) { e.put<int32_t>(b + 4u * m, r.countMonth[static_cast<size_t>(m)]); e.put<int32_t>(b + 0x30 + 4u * m, r.fundMonth[static_cast<size_t>(m)]); }
        e.put<int32_t>(b + 0x60, r.currentCosts); e.put<int32_t>(b + 0x64, r.fundingPercent); e.put<int32_t>(b + 0x68, r.yearToDate);
    }
    e.put<uint32_t>(0x4C9A58, s.ordinances); e.put<int32_t>(0x4C945C, s.treasury); e.put<int32_t>(0x4CB500, s.debt); e.put<int16_t>(0x4C9434, int16_t(s.c9434));
    e.put<uint32_t>(0x4C9A8C, s.population10); e.put<int16_t>(0x4C8444, int16_t(s.rotation)); e.put<uint8_t>(0x4C9E24, uint8_t(s.wind)); e.put<uint8_t>(0x4CA3EC, uint8_t(s.cloudiness));
    e.put<uint8_t>(0x4CB4FC, uint8_t(s.militaryType));
    e.put<int32_t>(0x4C9444, s.mode); e.put<int16_t>(0x4C9830, int16_t(s.seaLevel)); e.put<int16_t>(0x4CB4F8, int16_t(s.militaryMask));
    e.put<uint16_t>(0x4C9408, s.c9408); e.put<uint16_t>(0x4CB4F0, s.cb4f0);
    for (int i = 0; i < 3; ++i) e.put<int16_t>(0x4CB910 + 2u * i, s.demand[static_cast<size_t>(i)]);
    e.put<int32_t>(0x4C9E1C, s.day); e.put<int16_t>(0x4CA034, int16_t(s.month()));
}

void SimImage::store(World& w, CityState& s) const {
    downloadBytes(e, xbld, w.xbld.v.data()); downloadBytes(e, xzon, w.xzon.v.data()); downloadBytes(e, xbit, w.xbit.v.data());
    downloadBytes(e, xtxt, w.xtxt.v.data()); downloadBytes(e, xund, w.xund.v.data()); downloadBytes(e, xter, w.xter.v.data());
    downloadShorts(e, altm, w.altm.v.data());
    downloadBytes(e, xtrf, w.xtrf.v.data()); downloadBytes(e, xplt, w.xplt.v.data()); downloadBytes(e, xval, w.xval.v.data()); downloadBytes(e, xcrm, w.xcrm.v.data());
    downloadBytes(e, xpop, w.xpop.v.data()); downloadBytes(e, xrog, w.xrog.v.data()); downloadBytes(e, xplc, w.xplc.v.data()); downloadBytes(e, xfir, w.xfir.v.data());
    e.read(micro, w.xmic.data(), 1200); e.read(labels, w.xlab.data(), 6400); e.read(things, w.xthg.data(), 480);
    s.c9408 = e.get<uint16_t>(0x4C9408); s.cb4f0 = e.get<uint16_t>(0x4CB4F0); s.militaryMask = e.get<int16_t>(0x4CB4F8);
    for (int i = 0; i < 256; ++i) s.census[static_cast<size_t>(i)] = e.get<int16_t>(census + 2u * i);
    for (int i = 0; i < 8; ++i) s.zonePop[static_cast<size_t>(i)] = e.get<int32_t>(zonePop + 4u * i);
    for (int i = 0; i < 16; ++i) s.militaryCounts[static_cast<size_t>(i)] = e.get<int16_t>(milCounts + 2u * i);
    for (int k = 0; k < 16; ++k) {
        BudgetRecord& r = s.budget[static_cast<size_t>(k)]; const uint32_t b = budget + 0x70u * k;
        for (int m = 0; m < 12; ++m) { r.countMonth[static_cast<size_t>(m)] = e.get<int32_t>(b + 4u * m); r.fundMonth[static_cast<size_t>(m)] = e.get<int32_t>(b + 0x30 + 4u * m); }
        r.currentCosts = e.get<int32_t>(b + 0x60); r.fundingPercent = e.get<int32_t>(b + 0x64); r.yearToDate = e.get<int32_t>(b + 0x68);
    }
    s.treasury = e.get<int32_t>(0x4C945C); s.c9434 = e.get<int16_t>(0x4C9434); s.population10 = e.get<uint32_t>(0x4C9A8C);
    for (int i = 0; i < 3; ++i) s.demand[static_cast<size_t>(i)] = e.get<int16_t>(0x4CB910 + 2u * i);
}

void SimImage::stubUi() {
    // presentation / UI routines (plain `ret`)
    for (uint32_t f : {0x440C90u, 0x47BA90u, 0x452500u, 0x452190u, 0x452EF0u, 0x452CE0u, 0x452660u,           // scroll, messages, thing spawners
                       0x435100u, 0x4857F0u, 0x45AA50u, 0x4351F0u, 0x45AEC0u, 0x439EB0u, 0x439710u, 0x441800u, 0x451360u})   // drawing, dirty rects, delays
        e.patchRet(f);
    e.patchRetN(0x40ED80, 8);     // InvalidateRect wrapper (thiscall, 2 args)
    e.patchRetN(0x424AA0, 4);     // play sound (thiscall, 1 arg)
    const uint8_t zeroEax[3] = {0x31, 0xC0, 0xC3};
    e.write(0x475D70, zeroEax, 3);                       // acquire window DC -> 0
    const uint8_t getView[6] = {0xB8, uint8_t(fakeView), uint8_t(fakeView >> 8), uint8_t(fakeView >> 16), uint8_t(fakeView >> 24), 0xC3};
    e.write(0x407220, getView, 6);                       // current view -> zeroed fake view object
    // UpdateWindow (USER32 import @0x4EEE10) -> stdcall stub `ret 4`
    const uint32_t stub = Emu::kRandState + 0x3000;
    const uint8_t ret4[3] = {0xC2, 0x04, 0x00};
    e.write(stub, ret4, 3);
    e.put<uint32_t>(0x4EEE10, stub);
}

namespace {
template <typename G>
long diffGrid(const char* name, const G& a, const G& b, std::string& detail) {
    long d = 0; int first = -1;
    for (size_t i = 0; i < a.v.size(); ++i) if (a.v[i] != b.v[i]) { ++d; if (first < 0) first = int(i); }
    if (d) { detail += std::string(" ") + name + ":" + std::to_string(d) + "@" + std::to_string(first / G::kSize) + "," + std::to_string(first % G::kSize); }
    return d;
}
}  // namespace

long diffWorld(const World& a, const World& b, std::string& detail) {
    long d = 0;
    d += diffGrid("XBLD", a.xbld, b.xbld, detail); d += diffGrid("XZON", a.xzon, b.xzon, detail); d += diffGrid("XBIT", a.xbit, b.xbit, detail);
    d += diffGrid("XTXT", a.xtxt, b.xtxt, detail); d += diffGrid("XUND", a.xund, b.xund, detail); d += diffGrid("XTER", a.xter, b.xter, detail);
    d += diffGrid("ALTM", a.altm, b.altm, detail);
    d += diffGrid("XTRF", a.xtrf, b.xtrf, detail); d += diffGrid("XPLT", a.xplt, b.xplt, detail); d += diffGrid("XVAL", a.xval, b.xval, detail); d += diffGrid("XCRM", a.xcrm, b.xcrm, detail);
    d += diffGrid("XPOP", a.xpop, b.xpop, detail); d += diffGrid("XROG", a.xrog, b.xrog, detail); d += diffGrid("XPLC", a.xplc, b.xplc, detail); d += diffGrid("XFIR", a.xfir, b.xfir, detail);
    if (a.xmic != b.xmic) { ++d; detail += " XMIC"; }
    if (a.xlab != b.xlab) { ++d; detail += " XLAB"; }
    if (a.xthg != b.xthg) { ++d; detail += " XTHG"; }
    return d;
}

long diffState(const CityState& a, const CityState& b, std::string& detail) {
    long d = 0;
    auto note = [&](bool differs, const char* what) { if (differs) { ++d; detail += std::string(" ") + what; } };
    note(a.census != b.census, "census"); if (a.zonePop != b.zonePop) { char zb[200]; std::snprintf(zb, sizeof zb, " zonePop[%d %d %d %d %d %d %d %d vs %d %d %d %d %d %d %d %d]", a.zonePop[0], a.zonePop[1], a.zonePop[2], a.zonePop[3], a.zonePop[4], a.zonePop[5], a.zonePop[6], a.zonePop[7], b.zonePop[0], b.zonePop[1], b.zonePop[2], b.zonePop[3], b.zonePop[4], b.zonePop[5], b.zonePop[6], b.zonePop[7]); detail += zb; }
    note(a.zonePop != b.zonePop, "zonePop"); note(a.militaryCounts != b.militaryCounts, "militaryCounts");
    note(a.treasury != b.treasury, "treasury"); note(a.militaryMask != b.militaryMask, "militaryMask"); note(a.c9408 != b.c9408 || a.cb4f0 != b.cb4f0, "counters"); note(a.c9434 != b.c9434, "c9434"); note(a.population10 != b.population10, "pop10"); note(a.demand != b.demand, "demand");
    for (size_t k = 0; k < 16; ++k) {
        note(a.budget[k].currentCosts != b.budget[k].currentCosts || a.budget[k].fundingPercent != b.budget[k].fundingPercent || a.budget[k].yearToDate != b.budget[k].yearToDate, "budget");
    }
    return d;
}

}  // namespace sc2k::emu
