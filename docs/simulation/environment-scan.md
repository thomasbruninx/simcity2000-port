# Environment scan (phase 2, VA 0x46A1C0)

Status: **ported** (`src/simulation/environment.cpp`), reading of decompilation + statistical validation on saved cities ([validation 004](../validation/004-environment-scan-vs-saved-cities.md)). Inputs: XBLD, XZON, XBIT, XTER, XTRF, previous XPLT/XCRM/XPOP/XROG, tables (below), MISC words.

## Original implementation (stages, in code order)
Scratch array `S` = 128×128 `int16` (row-pointer table 0x4CB710; the "second accumulator" pointer table 0x4CB790 is simply rows 32..63 of it — VERIFIED from the table spacing: 128 pointers span 0x4CB710–0x4CB90F).

1. **Pollution accumulation** (64×64): `S[r][c] = traffic[r][c]/5 + pollution_old[r][c] + Σ over the 4 fine tiles of (P[id] if id>0x6F) + 200·(id==5)` (id 5 = radioactive waste).
2. **Pollution smoothing**: `base = waterFlag − c8440 + 4 (+1 if ordinance 0x80000)`, `div = max(base,1)`; `v = 2·S[r][c] + Σ in-range 4-neighbours`, `div += #neighbours`, `pollution[r][c] = min(255, v/div)`; total → 0x4CB928.
3. **City centre**: over all fine tiles with id > 0x6F: sums of row/col, count (starts at 1): `centre = sum / (2·count)` (64-grid), stored ×2 in MISC words 1030/1031 (VERIFIED equal to the saved values in 54/61 files). Also stamps `S[a][b] = 0x28` on those tiles (fine coordinates, overlapping the other uses of S — quirk kept) and clears their 0x08 mark.
4. **Terrain accumulators** over all fine tiles into `S[a/4][b/4]` (A) and `S[32+a/4][b/4]` (B): empty land +4 (water tile +12 to A and B); id 0xD5 +0x28; ids 1–5 −0x14; trees 6–0xD +0x14; watered flag (+4 A and B); layer@0x4C8F70 ∈ 1..15 +12. Tile is "developed" if `id>0x1C` or zone nibble ≠ 0 → **marks `XBIT[a/2][b/2] |= 0x08`** (so marks only ever exist in the top-left 64×64 corner — VERIFIED on saved cities) and counts toward `developedTiles`.
5. **Land value** (64×64, only marked cells; else 0): zone nibble taken from fine tile (2r,2c) or (2r+1,2c+1). Commercial (3,4): 5-point average of `S` + city-centre bonus `(64−|dc|−|dr|)` − pollution/4 − crime/3 + density/3. Industrial (5,6): average mixing B (self/horizontal) with S rows (vertical) [original quirk], +21 for type 6, bonus `/4`, − pollution/16 − crime/4. Other zones: S average, +21 if density < 0x40, bonus `/2`, − pollution/5 − crime/3. Then halve if `landValueClass[id(2r,2c)] == 2` (id>0x6F), clamp 0..255. NB: crime and density here are the **previous** values (they are recomputed later in the same scan).
6. **Reset** S[0..31][0..31], police and fire coverage; **density & coverage pass** over interior tiles (1..126): ids ≥0xC6 add +2 (0xFB–0xFE +12) to `S[a/4][b/4]`; police station (0xD2) / fire station (0xD3) anchors (XZON bit 0x80) spread `(policeBase+5)·funding/2` / `funding·5/2` (halved if unpowered) through the 0x46B420 footprint; ids 0x70–0xC5 add `population[id]`; ordinance 0x800 / 0x10 add +2 coverage per building tile (police / fire).
7. **Density & growth** (32×32): `density = min(255, 4·S)`, `rate = clamp((7·rate_old + (8·(density − density_old) + 128)) / 8)`.
8. **Crime** (64×64): for marked cells `t = density[r/2][c/2] − land[r][c]/4 − police[r/2][c/2]/2 (+16 with ordinance 0x4)`; unmarked 0; then 5-point average (in range), clamp 0..255; total → 0x4CA46C.

Spread footprint (0x46B420), strengths: centre s; 4 orthogonal 4s/5; 4 diagonal ¾ of that; 12 cells at knight-ish distance 2: ⅔ of that; 12 at distance 3 and 4 at (±2,±2): half of that; additions saturate at 0..255 and ignore out-of-range cells.

## Tables (read from the user's SIMCITY.EXE at runtime, see `GameTables`)
`pollution` @0x4E85A8, `population` @0x4E8638 (both indexed by XBLD id), `landValueClass` @0x4E6468 (same bytes the zone pass reads as `0x4E64D8[id−0x70]`).

## Native
`runEnvironmentScan(World&, GameTables, EnvInputs)`; tables loaded by `GameTables::fromExe` after a PE-timestamp check; no game data in the repo.

## Open questions
Identity of layer 0x4C8F70 (**XTER** fits far better than XUND: XVAL mean|d| 1.4 vs 8.8 on NYC — STRONG); CENTERVL's all-zero saved coverage (59 powered police stations); exactness of land value/pollution/crime needs the *previous* month's state (not stored in saves).
