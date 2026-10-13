# Scenarios: SCEN chunk, goals, time limit

Port: `src/simulation/scenario.{h,cpp}` (`ScenarioInfo::fromCity`, `scenarioGoalsMet`), `CityState::scenario` / `scenarioMonthsLeft`, tick phase 22 in `tick.cpp`, `Simulation` constructor (`startScenario`).

## Loader 0x433F30
The `SCEN` chunk of a .SCN file is copied to 0x4CB92C (the first 4 bytes are skipped) and its integers byte-swapped. Layout of the rest (VERIFIED against all 18 scenarios: the numbers read sensibly, e.g. San Francisco = disaster 6 at (41, 61), 60 months, population goal 68000):

| Offset | Size | Meaning |
|---|---|---|
| +0 | u16 | disaster kind started when the scenario begins (stored in 0x4C9438), 0 = none |
| +2 / +3 | u8 | focus tile of the disaster |
| +4 | u16 | time limit in months (0x4CB934) |
| +6 | u32 | population goal, compared with MISC 1035 (e.g. Charleston: 45000, and its description says "city population level of 45,000") |
| +10 / +14 / +18 | u32 | residential / commercial / industrial budget-record populations |
| +22 | u32 | treasury minus number of bonds |
| +26 | u32 | total land value (MISC 9) |
| +30 / +32 | u16 | the two city scores (MISC 18 @0x4CA428, MISC 19 @0x4CB4CC) |
| +34 / +38 / +42 | u32 | upper limits for pollution, crime and traffic totals (only when > 0) |
| +46 / +47 | u8 | tile ids whose census must reach ... |
| +48 / +50 | u16 | ... these counts (only in the 56-byte chunks: Barcelona, Las Vegas, Paris -- all zero) |

The disaster is requested at load time and the scenario flag (0x4C9D5C) is set, so the first tick after loading starts the disaster.

## Phase 22 (0x412FE0 case 0x16, once per month)
After the city-class milestone: if the scenario flag is set and all goals hold -> win (`gameOver(1)`); otherwise the month counter (`dec word [0x4CB934]`) falls and at 0 the scenario is lost (`gameOver(0)`). Then bankruptcy (< -$100,000, `gameOver(2)`). `gameOver` (0x414470) plays a sound, shows string 285 (won), 286 (time ran out) or 287 (bankrupt) and clears the scenario flag.

Status: loader layout and the comparison chain are read from the decompilation and unit-tested on SANFRANC.SCN (disaster starts, goals not met, goals met at the population goal); not run against the emulator (the loader reads files through MFC).
