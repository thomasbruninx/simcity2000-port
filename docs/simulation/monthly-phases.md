# Per-tick phases of `SimulationTick` (VA 0x412FE0)

Status: each function below was **decompiled and read**; descriptions of *what is computed* are STRONG INFERENCE unless noted (algorithm details not yet reconstructed to code). One tick = one in-game day; phase = `day % 25` (see [clock-and-calendar.md](clock-and-calendar.md)). Layer names refer to [map-layers-and-globals.md](map-layers-and-globals.md).

| Phase | Function | What it does | Confidence | Port status |
|---|---|---|---|---|
| 0 | 0x40CED0 | Updates title-bar/date text and status (month names table at 0x4C6288) | STRONG | UI only |
| 0 | 0x4736C0 | On the budget-year boundary shows the annual budget dialog ("%s %4d Budget …") | MODERATE | UI |
| 0 | 0x473980 | **Finance** *(ported: finance.md)*: each December sets year flag; in January credits `yearToDate/…` to treasury, rolls monthly records, recomputes per-class maintenance from the tile census, applies ordinance costs, may randomly enact a computer-chosen ordinance (when funds exceed 50000+rand) | STRONG | not ported |
| 1 | 0x4671F0 | **Power supply scan** *(ported, validated)*: clears flags `0x08|0x40` in XBIT, floods power (0x467340) from every tile whose XBLD id ∈ 0xC6..0xCF that isn't already powered; computes % powered (→0x4C9A68, 100 when no consumers) | STRONG | not ported |
| 2 | 0x46A1C0 | **Environment scan** *(ported: environment-scan.md)* (sc2kfix: `SimulationPollutionTerrainAndLandValueScan`): pollution → land value → population density → rate of growth → crime, plus police/fire coverage (via 0x46B420); sets city centre (mean of building coordinates) | STRONG | not ported |
| 3–18 | 0x435220(a,b) | **Zone/tile update pass** *(zones 0–6 ported and bit-verified: zone-growth.md)*: visits tiles `(x≡a mod 4, y≡b mod 4)` (a = ph…); i.e. every tile once per 16 ticks. Zone 0: infrastructure decay/disaster-like maintenance (road, rail, bridge… with funding% vs `rand()%100`), zones 1–6: growth/decline via `0x436680`/`0x456F70` and construction (0x436800/0x4376C0/0x436F40, probabilities from demand, `4000−…` tables), zone 7 (military base type switch), zone 8/9 (airport/seaport), special buildings (ids 0xED, 0xF8, 0xFB–0xFE) | STRONG structure, algorithms UNKNOWN | not ported |
| 19 | 0x458040 | Traffic decay: `v -= v>>2` on 64×64 traffic, total → 0x4CB70C | VERIFIED | trivial |
| 20 | 0x463EE0 | **Water supply scan** *(ported, validated)*: clears XBIT `0x08` and `0x10`; iterates map in one of **4 orders chosen by view rotation** (0x4C8444) flooding water (0x464370) from tiles id 0xDC/0xFA that are powered; % watered → 0x4CB4D0 | STRONG | not ported |
| 21 | 0x467AE0 | **RCI demand** *(valves ported: demand.md; events not)* (R/C/I valves into 0x4CB910/12/14, ±2000, adjusted by tax-rate table 0x4E8318 and ordinance bits), population totals, uses x87 doubles + constants at 0x4D9CE8–0x4D9D00 and table 0x4E8528; then **random events/messages** (advisor news, fire/rubble spreading, weather via table 0x4DAD18) | STRONG | not ported |
| 21 | 0x4686C0 | **Economy/national model**: national population/GNP style variables (0x4C9940, 0x4CA02C, 0x4C9950, year-interpolated tables 0x4E8448/0x4E84F8) — uses `LFSR&0x7F` ×4 per category | MODERATE | not ported |
| 21 | 0x413910 | **Graph history**: shifts 16 series × 52 `int32` windows (monthly ×12, 6-monthly ×20 at months 0/6, 5-yearly ×20) and stores current stats | VERIFIED vs XGRP size/endianness | simple |
| 22 | inline in 0x412FE0 | Scenario goal checks (compares ~9 stats vs goal thresholds at 0x4CB936–0x4CB962 → win 0x414470(1)); bankruptcy `treasury < −100000` → 0x414470(2) | STRONG | not ported |
| 23 | 0x4143F0, 0x414410, 0x414430 | Post a window command (0x80, 0xA0, 0x98) to the main frame (UI refresh) | MODERATE | UI |
| 24 | 0x40BEA0, 0x4143D0, 0x414450, 0x4720A0 | Month end: UI refresh commands and **advisor-needs** selection (0x4720A0 compares stats/budgets to pick an advisor message) | MODERATE | UI/messages |

## RNG usage in these phases (VERIFIED by reading)
- Zone pass, demand, events, economy: **MSVC `rand()` (0x48BCD8)** dominates (`rand() % 100 < percent` style tests; `rand() & 3`, `% 6`, `& 0x7F` for random map coordinates).
- LFSR mask-7F (0x480100 = `&0x7F`) is used in the economy phase and zone checks; LFSR `&0x3` (0x47FFC0) gates special buildings; LCG appears in military/other code (not in these phases).
- **Consequence:** reproducing the original city evolution requires all three generators plus the exact call order — and `rand()`'s state is shared with UI code (newspaper/terrain `srand` calls).

## Native plan
Port in dependency order, each with tests built from saved-city layers where possible: (1) power scan, (2) water scan, (3) traffic decay (trivial), (4) environment scan, (5) demand, (6) finance, (7) zone pass (largest). Validate (4) against the decoded `XPLT/XVAL/XCRM/XPOP/XROG/XPLC/XFIR` layers of the sample cities: a saved city is the output of the game's last scan, so recomputing from its `XBLD/XZON/XBIT` should reproduce them nearly exactly (a strong test that needs no Windows run).
