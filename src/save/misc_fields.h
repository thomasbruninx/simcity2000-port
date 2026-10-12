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
constexpr size_t kWaterFactor = 912;     // @0x4C9830  (pump output = cloudiness/2 + this*5 + 10 per adjacent fresh-water tile)
constexpr size_t kOrdinances = 1000;     // @0x4C9A58  ordinance bitmask (20 bits)
constexpr size_t kSpeed = 1019;          // game speed 1..5 (app+0x308)
constexpr size_t kCityCenterX2 = 1030;   // @0x4CA470
constexpr size_t kCityCenterY2 = 1031;   // @0x4CA474

}  // namespace sc2k::misc
