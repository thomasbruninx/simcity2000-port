# Validation 006 — differential testing against the original machine code

**Method.** `tools/emu/` (dev-only, Unicorn x86-32 emulator, GPL-2.0 — never linked into the engine) maps the user's `SIMCITY.EXE` (build .022) into emulated memory, replaces the CRT `rand()`/`srand()` (0x48BCD8/0x48BCC8) by machine code operating on a state word we control, builds the game's row-pointer tables from a saved city, runs the original routine, and reads the results back. The C++ port is run on the same input with an identically seeded `CrtRand`. FPU control word is set to 0x27F (53-bit) as the MSVC runtime does. Unlike validation 002–005 this compares *code against code on identical input*, so stale saved state is no longer a confound. Reproduce: `tools/run_emu_diffs.sh <SC2K dir>`.

**Results (all 61 sample files; 0 differences everywhere).**

| Routine (VA) | Port | Runs | Compared | Result |
|---|---|---|---|---|
| Power scan 0x4671F0 | `runPowerScan` | 61 | XBIT (16,384 tiles/file), capacity, % used, final `rand()` state | **identical** (incl. wind/solar plants → random-driven path) |
| Water scan 0x463EE0 | `runWaterScan` | 61 | XBIT, supply, % served | **identical** (all four rotations, towers, desalination) |
| Environment scan 0x46A1C0 | `runEnvironmentScan` | 61 | XBIT marks, XPLT, XVAL, XCRM, XPOP, XROG, XPLC, XFIR, 6 totals incl. city centre | **identical — including the layers that saves could only match approximately** |
| Demand valves 0x467AE0 (up to 0x467FB7) | `runDemand` | 244 (61 × 4 variants: as saved + 3 perturbed incl. clamping/wrap cases) | zonePop[0], population×10, history sum, ratio memory, decline/growth, 3 demands, 3 budget costs | **identical** (x87 doubles reproduced exactly) |
| Monthly finance 0x473980 | `runMonthlyFinance` | 1,464 so far (61 × 6 + 4 × 24 incl. January settlement, December flag, random ordinance enactment) | all 16 budget records (cost, %, YTD, 12+12 monthly values), treasury, year flag, ordinances, `rand()` state | **identical** (3 random enactments exercised) |

| Zone/tile pass 0x435220 + traffic router 0x456F70 + tile primitives | `runZonePass` (zones 0–6) | 183 (61 cities × 3 economies; 16 sub-passes × 3 sweeps each) | all map layers, census, zone populations, military counters, `rand()`/LFSR end state, ridership counters | **identical** (92,739 tile changes exercised) |
| Zone types 7–9 and underground decay: 0x437C30, 0x438C20, 0x440600 | `placeZoneBuilding`, `clearSpecialBuilding`, `placeSpecialBuilding`, `militaryZone/airportZone/seaportZone` | 427 (61 cities × 7 variants: as saved, 2 perturbed economies, 4 with zone 7/8/9 blocks painted and militaryType 2..5, 12 sweeps) | same as above | **identical** (≈10,300 special-building tiles placed in the painted variants) |
| Destroy effects 0x458690 + footprint, bridge classifier, label/microsim cleanup, shoreline fixer | `destroyTile` & helpers | 122 (61 cities × animate on/off; every 5th built tile demolished in scan order, ≈ 200,000 demolitions; tiles within 3 of the border skipped) | all layers, census, labels, microsim, things, counters, `rand()`/LFSR end state | **identical** |

**Consequences for earlier reports.**
- Validation 003/004 mismatches between *saved* flags/layers and recomputed ones are therefore **properties of the saved state** (previous-month state, edits after the last scan, different game build), not port errors — the port equals the original code on the same input. The two phase-20 water saves (TOMG-C4, FOURCITI) and CENTERVL's all-zero coverage remain unexplained *as file anomalies*.
- Layer 0x4C8F70 = XTER, the `DAT_004cb790 = scratch rows 32..63` reading, the 0x08-mark-in-corner quirk, the queue overflow semantics, the "plant tiles cost one unit" quirk are all now confirmed by running the original.

**Limits.** The zone-pass test strips zone types 7–9 from the input and neutralises UI/presentation routines (0x440C90, 0x47BA90, 0x452xxx thing spawners, drawing/dirty-rect/sound/delay helpers, `UpdateWindow`); destroy is **not** stubbed any more (0x407220 returns a zeroed fake view). Only routines whose inputs we construct are testable; UI-touching callees are patched to `ret` (0x424A20 `ret 4`, 0x40CED0, 0x44DD10, 0x47BA90, 0x40C9A0, 0x40DC20). The demand routine is stopped at 0x467FB7 (before its random-event half). Inputs are real saved cities plus synthetic perturbations, not exhaustive. `c9408`/`cb4f0` are fed identical values to both sides.
