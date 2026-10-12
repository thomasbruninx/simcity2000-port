# Validation 003 — water scan reproduced from saved cities

**Method.** As validation 002: for each of the 61 sample files build a `World`, recompute power (as the game does before water), then run our port of `0x463EE0`/`0x464370` (`src/simulation/water.cpp`) with inputs read from `MISC` (rotation word 2, cloudiness word 26, water factor word 912) and compare the *watered* flag (`XBIT & 0x10`) with the saved one. `sc2k-verify water <files>` (set `SC2K_DEBUG=1` for diff details).

**Result.** 284,009 saved watered tiles; **4,327 mismatching** (1.5%). 48 of 61 files match with ≤ 4 tiles — exactly 0 in 41 of them — covering all four view rotations (0–3), shortage and surplus cases, towers, desalination, and wind/solar-powered networks. The rest:

| File | Diff (ours − saved) | Notes |
|---|---|---|
| TOMG-C4.SC2 | +2020 | all differing tiles are piped; ids 0x00–0x0F (pipes under empty land/trees) dominate |
| HOLLYWOO.SCN | +498 | |
| CHICAGO.SCN | +390 | |
| LASVEGAS.SCN | −319 | |
| VIRTUALV.SC2 | −294 | |
| SILICONV.SCN | −248 | |
| WASHINGT.SCN | +152, ATLANTA.SCN +145, CHARLEST.SCN +144, FOURCITI.SC2 +76, MALIBU.SCN −19, PARIS (both) +4/+12, others ≤3 | |

**Interpretation.**
- Exact agreement in 41 files, across all rotations, is strong evidence that the flood algorithm, FIFO order, pump/tower/desalination formulas, `MISC` words 2/26/912 and the XBIT bit meanings are right (VERIFIED statistically).
- In all mismatching files the differing tiles are **piped** tiles, scattered across the map rather than clustered in one network. The most plausible cause (HYPOTHESIS, not tested) is that those saves contain infrastructure edited after the last monthly scan, so the stored flags are stale (the scan only runs at phase 20 of each month, so a save made the same month as a pipe edit is legitimately out of date). The scenario files and hand-built test cities (TOMG-*) are the likeliest to be saved mid-edit. Positive diffs (ours has more) = pipes laid after the scan; negative diffs = pipes/consumers removed.
- Update: the emulator differential test (validation 006) shows the water port is bit-identical to the original on these same inputs, so the saved-flag differences above are properties of the saved states.

**Next test to settle it.** Reproduce one mismatching city in the Windows version: load TOMG-C4 and let one month pass (or save/reload) and compare the watered-tile count with our scan (2,560 saved vs 4,580 ours predicted after a rescan).
