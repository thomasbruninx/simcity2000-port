// Word indices into the MISC chunk (1200 big-endian 32-bit words) as read by the game's city loader
// FUN_00430CB0 (VA 0x430CB0, build .022). Names are ours; "global" is the runtime variable the loader fills.
// Confidence: index->global VERIFIED from decompilation; meaning as noted in docs/formats/misc-fields.md.
#pragma once
#include <cstddef>

namespace sc2k::misc {

constexpr size_t kVersion = 0;           // must be 0x122 or the game refuses the file ("Old File Version")
constexpr size_t kMode = 1;              // @0x4C9444 (observed 1 in every sample)
constexpr size_t kRotation = 2;          // @0x4C8444 view rotation 0..3 (selects map iteration order in the water scan)
constexpr size_t kStartYear = 3;         // @0x4C960C  displayed year = startYear + day/300
constexpr size_t kDay = 4;               // @0x4C9E1C  day counter (int32)
constexpr size_t kTreasury = 5;          // @0x4C945C  (int32)
constexpr size_t kDebt = 6;              // @0x4CB500  subtracted from treasury in goal checks (HYPOTHESIS: bond debt)
constexpr size_t kEconIndex = 7;         // @0x4CA41C  index into the 8-byte-stride double table @0x4E8528
constexpr size_t kMilestoneIndex = 8;    // @0x4CA028  city-class milestone (0..5)
constexpr size_t kLandValueTotal = 10;   // @0x4C9458
constexpr size_t kCrimeTotal = 11;       // @0x4CA46C
constexpr size_t kTrafficTotal = 12;     // @0x4CB70C
constexpr size_t kPollutionTotal = 13;   // @0x4CB928
constexpr size_t kWind = 25;             // @0x4C9E24  (byte)
constexpr size_t kCloudiness = 26;       // @0x4CA3EC  (byte)
constexpr size_t kWeather = 27;          // @0x4CA424  (byte) weather-table state
constexpr size_t kPendingDisaster = 28;  // @0x4C9438
constexpr size_t kTileCensus = 124;      // 256 x int16 stored one per word (counts per XBLD id) @0x4C94E0
constexpr size_t kZonePop = 380;         // 8 x dword @0x4C9440
constexpr size_t kDemand = 454;          // 8 x int16 (R, C, I, ...) @0x4CB910
constexpr size_t kBudget = 479;          // 16 x 27 words: currentCosts, fundingPercent, yearToDate, then 12 x (countMonth, fundMonth)
constexpr size_t kBudgetRecordWords = 27;
constexpr size_t kBudgetYearFlag = 911;  // @0x4C9D44
constexpr size_t kWaterFactor = 912;     // @0x4C9830  = sea level (altitude of the water surface); pump output = cloudiness/2 + this*5 + 10 per adjacent fresh-water tile
constexpr size_t kMilitaryMask = 1034;   // @0x4CB4F8 (int16) bit mask of military base kinds present
constexpr size_t kMilitaryType = 915;     // @0x4CB4FC (byte) military base type selector (2..5 used by the zone pass)
constexpr size_t kMilitaryCounts = 1002;  // 16 x int16 @0x4C994C: military buildings by type
constexpr size_t kOrdinances = 1000;     // @0x4C9A58  ordinance bitmask (20 bits)
constexpr size_t kSpeed = 1019;          // game speed 1..5 (app+0x308)
constexpr size_t kCityCenterX2 = 1030;   // @0x4CA470
constexpr size_t kCityCenterY2 = 1031;   // @0x4CA474
constexpr size_t kBudgetYearFlag2 = kBudgetYearFlag;
constexpr size_t kPopulation10 = 1035;   // @0x4C9A8C  population*10 of the previous demand pass
constexpr size_t kC9200 = 1036;          // @0x4C9200  (int16) demand term
constexpr size_t kExtraPop = 1032;       // @0x4C84DC  extra residents added to budget currentCosts (R/6, C/12, I/12)
constexpr size_t kC9434 = 1018;          // @0x4C9434  (int16) added to the subway maintenance class
constexpr size_t kCb4ec = 1024;          // @0x4CB4EC  flag: random ordinance enactment disabled when non-zero
constexpr size_t kPopHistorySum = 16;    // @0x4C9608  running sum of population*10
constexpr size_t kRatioMemory = 29;      // @0x4C91AC  previous residential population (demand ratio)
constexpr size_t kC8440 = 1037;          // @0x4C8440  pollution divisor term
constexpr size_t kPoliceBase = 1039;     // @0x4C91F4  police station strength base
constexpr size_t kWaterFlag = 1043;      // @0x4CA3F4  set by the water scan
constexpr size_t kFundingPercent(size_t category) { return 479 + 27 * category + 1; }   // budget[category].fundingPercent

}  // namespace sc2k::misc
