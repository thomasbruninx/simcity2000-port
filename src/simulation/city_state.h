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
    int32_t rotation = 0;              // MISC 2 @0x4C8444 view rotation 0..3
    int32_t wind = 0;                  // MISC 25 (byte)
    int32_t cloudiness = 0;            // MISC 26 (byte)
    int32_t militaryType = 0;          // MISC 915 (byte)
    std::array<int16_t, 16> militaryCounts{};   // MISC 1002..1017
    std::array<int16_t, 8> demand{};   // MISC 454..461 @0x4CB910 (R, C, I, ...)
    std::array<int16_t, 256> census{}; // MISC 124..379: count of each XBLD id
    std::array<BudgetRecord, 16> budget{};

    int month() const { return (day / 25) % 12; }     // game: ((day / 0x19) % 0xC), signed division
    int phase() const { return day % 25; }

    static CityState fromCity(const CityFile& c);
    void writeBack(CityFile& c) const;                // writes every field above (round-trips unchanged state)
};

}  // namespace sc2k
