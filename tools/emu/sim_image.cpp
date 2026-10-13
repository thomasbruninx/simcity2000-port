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
    jobsAp = e.alloc(80); jobsBp = e.alloc(80); jobsCp = e.alloc(80); s940cp = e.alloc(32); s3f8p = e.alloc(32); a408p = e.alloc(48);
    n9d28p = e.alloc(16); n981cp = e.alloc(16); n9944p = e.alloc(16);
    e.put<uint32_t>(0x4C84CC, jobsAp); e.put<uint32_t>(0x4C84D4, jobsBp); e.put<uint32_t>(0x4C9A88, jobsCp);
    e.put<uint32_t>(0x4C940C, s940cp); e.put<uint32_t>(0x4CA3F8, s3f8p); e.put<uint32_t>(0x4CA408, a408p);
    e.put<uint32_t>(0x4C9D28, n9d28p); e.put<uint32_t>(0x4C981C, n981cp); e.put<uint32_t>(0x4C9944, n9944p);
    graphBase = e.alloc(16 * 52 * 4); graphMaxp = e.alloc(64);
    for (int i = 0; i < 16; ++i) e.put<uint32_t>(0x4CB488 + 4u * i, graphBase + 208u * i);
    e.put<uint32_t>(0x4CB708, graphMaxp);
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
    e.put<int16_t>(0x4CA038, int16_t(s.yearOffset())); e.put<int16_t>(0x4C960C, int16_t(s.startYear));
    e.put<uint32_t>(0x4C9940, s.nationalPop); e.put<uint32_t>(0x4CA02C, s.gnp);
    e.put<int16_t>(0x4C9950, int16_t(s.rateLevel)); e.put<int16_t>(0x4C91D4, int16_t(s.era));
    e.put<uint32_t>(0x4C94E4, s.populationGrowth); e.put<uint32_t>(0x4CA1C0, s.populationDecline);
    e.put<uint32_t>(0x4CB4CC, s.cb4cc); e.put<uint32_t>(0x4CA428, s.ca428); e.put<uint32_t>(0x4CA478, s.ca478);
    e.put<int16_t>(0x4C8440, int16_t(s.c8440)); e.put<int16_t>(0x4C9200, int16_t(s.c9200)); e.put<int32_t>(0x4CB928, s.pollutionTotal);
    for (int i = 0; i < 3; ++i) e.put<int32_t>(c9948 + 4u * i, s.catPop[static_cast<size_t>(i)]);
    for (int i = 0; i < 20; ++i) { e.put<uint32_t>(jobsAp + 4u * i, s.jobsA[static_cast<size_t>(i)]); e.put<uint32_t>(jobsBp + 4u * i, s.jobsB[static_cast<size_t>(i)]); e.put<uint32_t>(jobsCp + 4u * i, s.jobsC[static_cast<size_t>(i)]); }
    for (int i = 0; i < 11; ++i) { e.put<int16_t>(s940cp + 2u * i, s.sec940c[static_cast<size_t>(i)]); e.put<int16_t>(s3f8p + 2u * i, s.sec3f8[static_cast<size_t>(i)]); e.put<int32_t>(a408p + 4u * i, s.sec408[static_cast<size_t>(i)]); }
    for (int i = 0; i < 4; ++i) { e.put<uint32_t>(n9d28p + 4u * i, s.nbr9d28[static_cast<size_t>(i)]); e.put<uint32_t>(n981cp + 4u * i, s.nbr981c[static_cast<size_t>(i)]); e.put<uint32_t>(n9944p + 4u * i, s.nbr9944[static_cast<size_t>(i)]); }
    for (int i = 0; i < 16 * 52; ++i) e.put<int32_t>(graphBase + 4u * i, s.graph[static_cast<size_t>(i)]);
    for (int i = 0; i < 16; ++i) e.put<int32_t>(graphMaxp + 4u * i, s.graphMax[static_cast<size_t>(i)]);
    e.put<int16_t>(0x4CA028, int16_t(s.milestone)); e.put<uint8_t>(0x4CA424, uint8_t(s.weather)); e.put<uint8_t>(0x4C9DF8, uint8_t(s.c9df8));
    for (int i = 0; i < 17; ++i) e.put<int16_t>(0x4CA448 + 2u * i, s.eventYears[static_cast<size_t>(i)]);
    e.put<int32_t>(0x4C84D8, s.c84d8); e.put<int32_t>(0x4CA410, s.ca410);
    e.put<int32_t>(0x4C9458, s.landValueTotal); e.put<int32_t>(0x4CA46C, s.crimeTotal); e.put<int32_t>(0x4CB70C, s.trafficTotal);
    e.put<int16_t>(0x4CA3F4, int16_t(s.waterFlag)); e.put<int32_t>(0x4C9A68, s.powerPct); e.put<int32_t>(0x4CB4D0, s.waterPct);
    e.put<int32_t>(0x4C83C8, s.powerCapacity); e.put<int32_t>(0x4C83C4, s.waterSupply); e.put<int32_t>(0x4C83CC, s.developedTiles);
    e.put<int16_t>(0x4C91F4, int16_t(s.policeBase)); e.put<int16_t>(0x4CA034, int16_t(s.month()));
    e.put<int32_t>(0x4C9E1C, s.day);
    e.put<int16_t>(0x4C9610, int16_t(s.c9610)); e.put<int16_t>(0x4C9448, int16_t(s.c9448));
    e.put<int32_t>(0x4C84DC, s.extraPop); e.put<int16_t>(0x4CA41C, int16_t(s.econIndex)); e.put<int32_t>(0x4C9608, s.popHistorySum);
    e.put<int32_t>(0x4C94E8, s.cityValue); e.put<int32_t>(0x4C91AC, s.ratioMemory); e.put<int32_t>(0x4C9D44, s.budgetYearFlag);
    e.put<int32_t>(0x4CA470, s.centerA2); e.put<int32_t>(0x4CA474, s.centerB2); e.put<int16_t>(0x4CA034, int16_t(s.month()));
}

void SimImage::loadThings(const ThingState& t, const CityState& s) {
    e.put<int32_t>(0x4E70CC, t.animFrame);
    const int32_t counters[9] = {t.planes, t.helis, t.ships, t.visitors, t.sailboats, t.monsters, t.trains, t.tornados, t.soundGate};
    for (int i = 0; i < 9; ++i) e.put<int32_t>(0x4E89B0 + 4u * i, counters[i]);
    e.put<int32_t>(0x4C9A70, t.focusA); e.put<int32_t>(0x4C9A74, t.focusB);
    e.put<uint16_t>(0x4C9438, uint16_t(t.disasterRequest)); e.put<int16_t>(0x4C9834, int16_t(t.lastSpawned));
    e.put<int32_t>(0x4E70BC, t.shipTargetA); e.put<int32_t>(0x4E70C0, t.shipTargetB);
    e.put<int32_t>(0x4C9D5C, t.scenarioFlag); e.put<int16_t>(0x4E76A4, int16_t(t.fireMessageKind));
    e.put<int32_t>(0x4C83C0, t.floodTimer); e.put<int16_t>(0x4E76A0, int16_t(t.floodBudget)); e.put<int16_t>(0x4CB968, int16_t(t.dozerSub));
    e.put<int32_t>(0x4CA470, s.centerA2); e.put<int32_t>(0x4CA474, s.centerB2); e.put<int32_t>(0x4CB4EC, s.cb4ec);
    if (!t.spriteHeight.empty()) {
        const uint32_t tbl = e.get<uint32_t>(0x4CA1D0);
        for (size_t i = 0; i < t.spriteHeight.size() && i < 1400; ++i) e.put<uint16_t>(tbl + 8u * uint32_t(i) + 4, t.spriteHeight[i]);
    }
}

void SimImage::storeThings(ThingState& t) const {
    t.animFrame = e.get<int32_t>(0x4E70CC);
    int32_t* counters[9] = {&t.planes, &t.helis, &t.ships, &t.visitors, &t.sailboats, &t.monsters, &t.trains, &t.tornados, &t.soundGate};
    for (int i = 0; i < 9; ++i) *counters[i] = e.get<int32_t>(0x4E89B0 + 4u * i);
    t.focusA = e.get<int32_t>(0x4C9A70); t.focusB = e.get<int32_t>(0x4C9A74);
    t.disasterRequest = e.get<uint16_t>(0x4C9438); t.lastSpawned = e.get<int16_t>(0x4C9834);
    t.shipTargetA = e.get<int32_t>(0x4E70BC); t.shipTargetB = e.get<int32_t>(0x4E70C0);
    t.scenarioFlag = e.get<int32_t>(0x4C9D5C); t.fireMessageKind = e.get<int16_t>(0x4E76A4);
    t.floodTimer = e.get<int32_t>(0x4C83C0); t.floodBudget = e.get<int16_t>(0x4E76A0); t.dozerSub = e.get<int16_t>(0x4CB968);
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
    s.nationalPop = e.get<uint32_t>(0x4C9940); s.gnp = e.get<uint32_t>(0x4CA02C);
    s.rateLevel = e.get<int16_t>(0x4C9950); s.era = e.get<int16_t>(0x4C91D4);
    s.populationGrowth = e.get<uint32_t>(0x4C94E4); s.populationDecline = e.get<uint32_t>(0x4CA1C0);
    s.cb4cc = e.get<uint32_t>(0x4CB4CC); s.ca428 = e.get<uint32_t>(0x4CA428); s.ca478 = e.get<uint32_t>(0x4CA478);
    s.c8440 = e.get<int16_t>(0x4C8440); s.c9200 = e.get<int16_t>(0x4C9200);
    for (int i = 0; i < 3; ++i) s.catPop[static_cast<size_t>(i)] = e.get<int32_t>(c9948 + 4u * i);
    for (int i = 0; i < 20; ++i) { s.jobsA[static_cast<size_t>(i)] = e.get<uint32_t>(jobsAp + 4u * i); s.jobsB[static_cast<size_t>(i)] = e.get<uint32_t>(jobsBp + 4u * i); s.jobsC[static_cast<size_t>(i)] = e.get<uint32_t>(jobsCp + 4u * i); }
    for (int i = 0; i < 11; ++i) { s.sec940c[static_cast<size_t>(i)] = e.get<int16_t>(s940cp + 2u * i); s.sec3f8[static_cast<size_t>(i)] = e.get<int16_t>(s3f8p + 2u * i); s.sec408[static_cast<size_t>(i)] = e.get<int32_t>(a408p + 4u * i); }
    for (int i = 0; i < 4; ++i) { s.nbr9d28[static_cast<size_t>(i)] = e.get<uint32_t>(n9d28p + 4u * i); s.nbr981c[static_cast<size_t>(i)] = e.get<uint32_t>(n981cp + 4u * i); s.nbr9944[static_cast<size_t>(i)] = e.get<uint32_t>(n9944p + 4u * i); }
    for (int i = 0; i < 16 * 52; ++i) s.graph[static_cast<size_t>(i)] = e.get<int32_t>(graphBase + 4u * i);
    for (int i = 0; i < 16; ++i) s.graphMax[static_cast<size_t>(i)] = e.get<int32_t>(graphMaxp + 4u * i);
    s.milestone = e.get<int16_t>(0x4CA028); s.weather = e.get<uint8_t>(0x4CA424); s.c9df8 = e.get<uint8_t>(0x4C9DF8);
    for (int i = 0; i < 17; ++i) s.eventYears[static_cast<size_t>(i)] = e.get<int16_t>(0x4CA448 + 2u * i);
    s.landValueTotal = e.get<int32_t>(0x4C9458); s.crimeTotal = e.get<int32_t>(0x4CA46C); s.trafficTotal = e.get<int32_t>(0x4CB70C);
    s.waterFlag = e.get<int16_t>(0x4CA3F4); s.powerPct = e.get<int32_t>(0x4C9A68); s.waterPct = e.get<int32_t>(0x4CB4D0);
    s.pollutionTotal = e.get<int32_t>(0x4CB928);
    s.c9610 = e.get<int16_t>(0x4C9610); s.c9448 = e.get<int16_t>(0x4C9448);
    s.day = e.get<int32_t>(0x4C9E1C); s.extraPop = e.get<int32_t>(0x4C84DC); s.popHistorySum = e.get<int32_t>(0x4C9608);
    s.ratioMemory = e.get<int32_t>(0x4C91AC); s.budgetYearFlag = e.get<int32_t>(0x4C9D44); s.ordinances = e.get<uint32_t>(0x4C9A58);
    s.wind = e.get<uint8_t>(0x4C9E24); s.cloudiness = e.get<uint8_t>(0x4CA3EC);
    s.centerA2 = e.get<int16_t>(0x4CA470); s.centerB2 = e.get<int16_t>(0x4CA474);
    s.developedTiles = e.get<int32_t>(0x4C83CC); s.powerCapacity = e.get<int32_t>(0x4C83C8); s.waterSupply = e.get<int32_t>(0x4C83C4);
    s.treasury = e.get<int32_t>(0x4C945C); s.c9434 = e.get<int16_t>(0x4C9434); s.population10 = e.get<uint32_t>(0x4C9A8C);
    for (int i = 0; i < 3; ++i) s.demand[static_cast<size_t>(i)] = e.get<int16_t>(0x4CB910 + 2u * i);
}

void SimImage::stubTick() {
    for (uint32_t f : {0x40CED0u, 0x479F40u, 0x4B1243u, 0x4736C0u, 0x42FA50u, 0x406A40u, 0x413D80u, 0x469A10u, 0x414470u, 0x4143F0u, 0x414410u, 0x414430u, 0x4143D0u, 0x414450u}) e.patchRet(f);
    // the tick sets up an SEH frame through FS:[0]; there is no TIB here, so neutralise those instructions (xor eax,eax / nops)
    {
        std::vector<uint8_t> code(0x700);
        e.read(0x412FE0, code.data(), code.size());
        for (size_t i = 0; i + 7 <= code.size(); ++i) {
            if (code[i] == 0x64 && code[i + 1] == 0xA1 && code[i + 2] == 0 && code[i + 3] == 0 && code[i + 4] == 0 && code[i + 5] == 0) {   // mov eax, fs:[0]
                const uint8_t p[6] = {0x31, 0xC0, 0x90, 0x90, 0x90, 0x90}; e.write(0x412FE0 + i, p, 6);
            } else if (code[i] == 0x64 && code[i + 1] == 0xA3 && code[i + 2] == 0 && code[i + 3] == 0 && code[i + 4] == 0 && code[i + 5] == 0) {   // mov fs:[0], eax
                const uint8_t p[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(0x412FE0 + i, p, 6);
            } else if (code[i] == 0x64 && code[i + 1] == 0x89 && code[i + 3] == 0 && code[i + 4] == 0 && code[i + 5] == 0 && code[i + 6] == 0) {   // mov fs:[0], reg
                const uint8_t p[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(0x412FE0 + i, p, 7);
            }
        }
    }
    e.patchRetN(0x424A20, 4); e.patchRetN(0x4A2347, 4); e.patchRet(0x4A1B89); e.patchRetN(0x406CA0, 4);
    e.patchRetN(0x47FDE0, 8); e.patchRetN(0x40BEA0, 4); e.patchRetN(0x4ACFB0, 12);
}

void SimImage::stubUi() {
    // presentation / UI routines (plain `ret`)
    for (uint32_t f : {0x440C90u, 0x47BA90u, 0x47FAC0u, 0x47F6C0u,                                             // scroll, messages, begin/end paint
                       0x435100u, 0x4857F0u, 0x45AA50u, 0x4351F0u, 0x45AEC0u, 0x439EB0u, 0x439710u, 0x441800u, 0x451360u})   // drawing, dirty rects, delays
        e.patchRet(f);
    e.patchRetN(0x40ED80, 8);     // InvalidateRect wrapper (thiscall, 2 args)
    e.patchRetN(0x412050, 12);    // recentre the view (thiscall, 3 args)
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
    e.put<uint32_t>(0x4EED7C, stub);                    // CharLowerA(name) -> ret 4 (the city name buffer is a zeroed string)
    e.put<uint32_t>(0x4C6148, e.alloc(64));
    const uint8_t zero32[3] = {0x31, 0xC0, 0xC3};      // GetTickCount() -> 0 (xor eax,eax; ret)
    e.write(stub + 0x10, zero32, 3);
    e.put<uint32_t>(0x4EEAD8, stub + 0x10);
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
    if (a.xmic != b.xmic) { ++d; detail += " XMIC"; for (size_t i = 0; i < a.xmic.size(); ++i) if (a.xmic[i] != b.xmic[i]) { detail += "@rec" + std::to_string(i / 8) + "+" + std::to_string(i % 8) + "(" + std::to_string(a.xmic[i]) + "/" + std::to_string(b.xmic[i]) + ")"; break; } }
    if (a.xlab != b.xlab) { ++d; detail += " XLAB"; }
    if (a.xthg != b.xthg) { ++d; detail += " XTHG"; }
    return d;
}

long diffThings(const ThingState& a, const ThingState& b, std::string& detail) {
    long d = 0;
    auto chk = [&](const char* n, long x, long y) { if (x != y) { ++d; detail += std::string(" ") + n + "(" + std::to_string(x) + "/" + std::to_string(y) + ")"; } };
    chk("animFrame", a.animFrame, b.animFrame); chk("planes", a.planes, b.planes); chk("helis", a.helis, b.helis); chk("ships", a.ships, b.ships);
    chk("visitors", a.visitors, b.visitors); chk("sailboats", a.sailboats, b.sailboats); chk("monsters", a.monsters, b.monsters);
    chk("trains", a.trains, b.trains); chk("tornados", a.tornados, b.tornados); chk("soundGate", a.soundGate, b.soundGate);
    chk("focusA", a.focusA, b.focusA); chk("focusB", a.focusB, b.focusB); chk("disasterRequest", a.disasterRequest, b.disasterRequest);
    chk("lastSpawned", a.lastSpawned, b.lastSpawned); chk("shipTargetA", a.shipTargetA, b.shipTargetA); chk("shipTargetB", a.shipTargetB, b.shipTargetB);
    chk("fireMessageKind", a.fireMessageKind, b.fireMessageKind);
    chk("floodTimer", a.floodTimer, b.floodTimer); chk("floodBudget", a.floodBudget, b.floodBudget);
    return d;
}

long diffState(const CityState& a, const CityState& b, std::string& detail) {
    long d = 0;
    auto note = [&](bool differs, const char* what) { if (differs) { ++d; detail += std::string(" ") + what; } };
    note(a.census != b.census, "census"); if (a.zonePop != b.zonePop) { char zb[200]; std::snprintf(zb, sizeof zb, " zonePop[%d %d %d %d %d %d %d %d vs %d %d %d %d %d %d %d %d]", a.zonePop[0], a.zonePop[1], a.zonePop[2], a.zonePop[3], a.zonePop[4], a.zonePop[5], a.zonePop[6], a.zonePop[7], b.zonePop[0], b.zonePop[1], b.zonePop[2], b.zonePop[3], b.zonePop[4], b.zonePop[5], b.zonePop[6], b.zonePop[7]); detail += zb; }
    note(a.zonePop != b.zonePop, "zonePop"); note(a.militaryCounts != b.militaryCounts, "militaryCounts");
    note(a.treasury != b.treasury, ("treasury(" + std::to_string(a.treasury) + "/" + std::to_string(b.treasury) + ")").c_str()); note(a.militaryMask != b.militaryMask, "militaryMask"); note(a.c9408 != b.c9408 || a.cb4f0 != b.cb4f0, "counters"); note(a.c9434 != b.c9434, "c9434"); note(a.population10 != b.population10, "pop10"); note(a.demand != b.demand, "demand");
    note(a.c9448 != b.c9448, "c9448");
    note(a.day != b.day, "day"); note(a.wind != b.wind || a.cloudiness != b.cloudiness, "wind/cloud"); note(a.weather != b.weather || a.c9df8 != b.c9df8, "weather");
    note(a.centerA2 != b.centerA2 || a.centerB2 != b.centerB2, "centre"); note(a.graph != b.graph, "graph"); note(a.graphMax != b.graphMax, "graphMax");
    note(a.milestone != b.milestone || a.eventYears != b.eventYears, "milestone/events"); note(a.c9610 != b.c9610, "c9610");
    note(a.landValueTotal != b.landValueTotal || a.crimeTotal != b.crimeTotal || a.trafficTotal != b.trafficTotal || a.pollutionTotal != b.pollutionTotal, "totals");
    note(a.powerPct != b.powerPct || a.waterPct != b.waterPct || a.waterFlag != b.waterFlag, "power/water");
    note(a.developedTiles != b.developedTiles, "developed");
    note(a.nationalPop != b.nationalPop || a.gnp != b.gnp, "national"); note(a.rateLevel != b.rateLevel || a.era != b.era, "rate/era");
    note(a.jobsA != b.jobsA, "jobsA"); note(a.jobsB != b.jobsB, "jobsB"); note(a.jobsC != b.jobsC, "jobsC");
    note(a.sec940c != b.sec940c, "sec940c"); note(a.sec408 != b.sec408, "sec408");
    note(a.nbr9d28 != b.nbr9d28 || a.nbr981c != b.nbr981c, "neighbours");
    note(a.populationGrowth != b.populationGrowth || a.populationDecline != b.populationDecline, "growth/decline");
    note(a.cb4cc != b.cb4cc || a.ca428 != b.ca428 || a.ca478 != b.ca478, "scores"); note(a.c8440 != b.c8440 || a.c9200 != b.c9200, "c8440/c9200");
    for (size_t k = 0; k < 16; ++k) {
        note(a.budget[k].currentCosts != b.budget[k].currentCosts, ("cost" + std::to_string(k) + "(" + std::to_string(a.budget[k].currentCosts) + "/" + std::to_string(b.budget[k].currentCosts) + ")").c_str());
        note(a.budget[k].fundingPercent != b.budget[k].fundingPercent, ("pct" + std::to_string(k)).c_str());
        note(a.budget[k].yearToDate != b.budget[k].yearToDate, ("ytd" + std::to_string(k) + "(" + std::to_string(a.budget[k].yearToDate) + "/" + std::to_string(b.budget[k].yearToDate) + ")").c_str());
        note(a.budget[k].countMonth != b.budget[k].countMonth || a.budget[k].fundMonth != b.budget[k].fundMonth, ("month" + std::to_string(k)).c_str());
    }
    return d;
}

}  // namespace sc2k::emu
