// Typed view of the economy-related MISC fields (index map: docs/formats/misc-fields.md).
#pragma once
#include <array>
#include <cstdint>

#include "save/city.h"

namespace sc2k {

struct BudgetRecord {
    int32_t currentCosts = 0;      // zone categories: population*10; others: units to maintain
    int32_t fundingPercent = 0;    // zone categories: tax rate; others: funding %
    int32_t yearToDate = 0;
    std::array<int32_t, 12> countMonth{}, fundMonth{};
};

enum BudgetCategory { kRes, kCom, kInd, kOrdinance, kBond, kPolice, kFire, kHealth, kSchool, kCollege, kRoad, kHighway, kBridge, kRail, kSubway, kTunnel };

struct CityState {
    int32_t day = 0;
    int32_t startYear = 0;             // MISC 3 @0x4C960C

    int32_t treasury = 0;
    int32_t debt = 0;                  // MISC 6 @0x4CB500
    int32_t econIndex = 0;             // MISC 7
    int32_t popHistorySum = 0;         // MISC 16 @0x4C9608
    int32_t ratioMemory = 0;           // MISC 29 @0x4C91AC
    uint32_t ordinances = 0;           // MISC 1000
    int32_t budgetYearFlag = 0;        // MISC 911
    int32_t c9434 = 0;                 // MISC 1018 (int16)
    int32_t cb4ec = 0;                 // MISC 1024
    int32_t extraPop = 0;              // MISC 1032 @0x4C84DC
    uint32_t population10 = 0;         // MISC 1035 @0x4C9A8C
    int32_t c9200 = 0;                 // MISC 1036 (int16)
    std::array<int32_t, 8> zonePop{};  // MISC 380..387 @0x4C9440
    // economy / job-market model (0x4686C0)
    uint32_t nationalPop = 0, gnp = 0;       // MISC 20/21 @0x4C9940 / 0x4CA02C
    int32_t rateLevel = 1, era = 0;          // MISC 22/23 @0x4C9950 / 0x4C91D4 (int16)
    std::array<uint32_t, 20> jobsA{}, jobsB{}, jobsC{};   // MISC 31..90 interleaved @0x4C84CC / 84D4 / 9A88
    std::array<int16_t, 11> sec940c{}, sec3f8{};          // MISC 91..123 @0x4C940C / 0x4CA3F8
    std::array<int32_t, 11> sec408{};                     // @0x4CA408
    std::array<uint32_t, 4> nbr9d28{}, nbr981c{}, nbr9944{};   // MISC 438..453 @0x4C9D28 / 981C / 9944
    std::array<int16_t, 4> nbrName{};                    // @0x4CB4F4
    uint32_t populationDecline = 0, populationGrowth = 0;  // @0x4CA1C0 / 0x4C94E4: outputs of the demand pass (not saved)
    std::array<int32_t, 3> catPop{};                     // @0x4C9948: R / C / I population (not saved)
    uint32_t cb4cc = 0, ca428 = 0, ca478 = 0;            // MISC 19/18/17: education / health / employment scores
    int32_t pollutionTotal = 0;                          // MISC 13 @0x4CB928
    int32_t c8440 = 0;                                   // MISC 1037 @0x4C8440 (int16)
    int32_t c9610 = 0;                                   // MISC 1001 @0x4C9610
    // history graphs (XGRP): 16 series x 52 samples; [0] = latest. graphMax: per-series chart maxima (runtime)
    std::array<int32_t, 16 * 52> graph{};
    std::array<int32_t, 16> graphMax{};
    int32_t milestone = 0;                               // MISC 8 @0x4CA028 city-class milestone index
    int32_t weather = 0, c9df8 = 0;                      // MISC 27 @0x4CA424 / MISC 24 @0x4C9DF8 (bytes)
    std::array<int16_t, 50> bondRates{};                 // MISC 388..437 @0x4C8F08: interest rate of each outstanding bond (oldest first); `debt` is their number
    int32_t cityValue = 0;                               // @0x4C94E8 (runtime): asset value of the city (0x469BF0), limits how many bonds may be issued
    std::array<int16_t, 17> eventYears{};                // MISC 462..478 @0x4CA448 scheduled events (0 = none)
    int32_t c84d8 = 0, ca410 = 0;                        // MISC 913 / 914 flags
    int32_t landValueTotal = 0, crimeTotal = 0, trafficTotal = 0;   // MISC 10 / 11 / 12
    int32_t waterFlag = 0;                               // MISC 1043 @0x4CA3F4
    int32_t powerPct = 100, waterPct = 100;              // @0x4C9A68 / 0x4CB4D0 (runtime)
    int32_t powerCapacity = 0, waterSupply = 0;         // @0x4C83C8 / 0x4C83C4 (runtime)
    int32_t developedTiles = 0;                          // @0x4C83CC (runtime, set by the environment scan)
    int32_t c9448 = 0;                                   // MISC 1038 @0x4C9448 (int16): police capacity total of the last yearly update
    int32_t policeBase = 0;                              // MISC 1039 @0x4C91F4
    int32_t mode = 0;                  // MISC 1 @0x4C9444
    int32_t seaLevel = 0;              // MISC 912 @0x4C9830 (int16): altitude of the water surface (called 'water factor' in the water-scan docs)
    int32_t militaryMask = 0;          // MISC 1034 @0x4CB4F8 (int16)
    uint16_t c9408 = 0, cb4f0 = 0;     // @0x4C9408 / 0x4CB4F0: counters of text-overlay objects (not stored in MISC)
    int32_t centerA2 = 0, centerB2 = 0; // MISC 1030/1031 @0x4CA470/74: city centre, as computed by the environment scan
    int32_t rotation = 0;              // MISC 2 @0x4C8444 view rotation 0..3
    int32_t wind = 0;                  // MISC 25 (byte)
    int32_t cloudiness = 0;            // MISC 26 (byte)
    int32_t militaryType = 0;          // MISC 915 (byte)
    std::array<int16_t, 16> militaryCounts{};   // MISC 1002..1017
    std::array<int16_t, 8> demand{};   // MISC 454..461 @0x4CB910 (R, C, I, ...)
    std::array<int16_t, 256> census{}; // MISC 124..379: count of each XBLD id
    std::array<BudgetRecord, 16> budget{};

    int yearOffset() const { return day / 300; }     // 0x4CA038
    int month() const { return (day / 25) % 12; }     // game: ((day / 0x19) % 0xC), signed division
    int phase() const { return day % 25; }

    static CityState fromCity(const CityFile& c);
    void writeBack(CityFile& c) const;                // writes every field above (round-trips unchanged state)
};

}  // namespace sc2k
