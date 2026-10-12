# Validation 002 — power scan reproduced from saved cities

**Method.** A saved city stores the power flags the game computed at its last scan. `sc2k-verify power` loads each of the 61 sample cities/scenarios, builds a `World` from `XBLD`/`XBIT`/`ALTM`, clears the mark/powered flags, runs our port of `0x4671F0`/`0x467340` (`src/simulation/power.cpp`) and compares the resulting *powered* flag (`XBIT & 0x40`) with the saved one. Weather inputs unknown (not yet located in `MISC`) are set to 0, so plants with random output (ids 0xC8 wind, 0xCC solar) are only approximate.

**Result (build of 2026-10-12).**
- 61 files, 249,402 saved powered tiles in the 41 cities **without** random-output plants: **1 mismatching tile** (EM.SC2: one tile powered by us but not in the file).
- Cities **with** wind/solar plants: 5 of 20 differ by a handful of tiles (EGYPTFAL −1, STCHRISC −1, VIRTUALV −5, SANFRANC −4, THEBAHAM −39 of 3,850) — expected: supply shortage boundaries depend on the random terms; others match exactly.
- Cities without plants (HERMAN, TOMG-B1..4) trivially 0 = 0.

**Interpretation.** Strong evidence (VERIFIED statistically, not for bit-exactness of RNG paths) that:
1. XBIT bits 0x08 (mark), 0x40 (powered), 0x80 (powerable) are as documented;
2. layer indexing `layer[a][b]` with `a` = first (outer, file-major) index and neighbour order are right;
3. plant ids 0xC6–0xCF and the FIFO ring-queue traversal order (which decides who is powered during a shortage) are right;
4. the "plant tiles cost one unit in pass 2" quirk is real (otherwise exact-capacity shortage cases would differ).

**Discrepancies not explained yet.** the one EM.SC2 tile (likely the city was edited after the last scan); wind/solar cases (need `wind`@0x4C9E24 and `cloudiness`@0x4CA3EC from `MISC`, and the exact RNG stream — not reproducible from a save).

**Reproduce.** `cmake --build build && ./build/sc2k-verify power SC2K/CITIES/*.SC2 SC2K/SCENARIO/*.SCN`; regression test `power_scan_reproduces_saved_powered_bits_in_original_cities`.
