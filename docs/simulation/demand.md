# RCI demand valves (phase 21, first half of VA 0x467AE0)

Status: **ported** (`src/simulation/demand.cpp`) from the **disassembly** (the decompiler output for the x87 code is unreliable). Cross-checked against an independent Python model (`tools/demand_model.py`, exact double equality) and against saved cities for the integer parts. The second half of 0x467AE0 (random events, advisor messages, weather) is **not** ported.

## Original implementation
All doubles are x87 values at 53-bit precision (MSVC start-up CW 0x27F) → reproduced with IEEE doubles in the same operation order (`-ffp-contract=off`).

1. `zonePop[0] = Σ zonePop[1..6]` (32-bit); `pop10 = zonePop[0]*10`; if `pop10 < previous` → decline = diff else growth = diff (stored @0x4CA1C0 / 0x4C94E4); `population10 := pop10`; `popHistorySum += pop10`.
2. `cat[0..2] = (zp1+zp2, zp3+zp4, zp5+zp6)` (res, com, ind). `comInd = (zp3+zp4) + (zp5+zp6)` as doubles (accumulated in that order).
3. `ratio = (double)ratioMemory / (1.0 + comInd)`; then `ratioMemory := cat[0]` (MISC word 29, see note).
4. `R = (double)(cat0/50) + comInd`; `X = ((pop10+50000)·K1 · ratio) · indPop`; `Y = (c9200·K2 + econFactor[econIndex]) · ratio`; `Z = indPop · Y`, `Z = 15.0` if `Z < K3`.
5. Caps (each applied as `if (cap < value) value = cap`):
   `R ≤ (c[0xD5]/3 + c[0xD7] + c[0xDA] + c[0xF8] + 10)·150`; `R ≤ cat1·4 + 500`; `X ≤ ((c[0xDD]+c[0xDE]+cb4f0)/5 · 4 + 4)·375`; `Z ≤ ((c[0xE0] + c9408 + 1)·4)·375` (`c[]` = tile census; cb4f0/c9408 are counters whose writers are in the zone code, not yet ported).
6. Valves `E[i] = D[i] / (double)(cat[i]+1) − 1.0` (unsigned pop+1). Budget categories 0..2 get `currentCosts = cat[i]*10`, then `+ extraPop/6`, `/12`, `/12`.
7. For each of R, C, I: tax = `budget[i].fundingPercent` (the tax rate) adjusted by ordinances — R: `+1` (bit 0x2), `−1` (0x4000); C: `+1` (0x1), `−1` (0x1000, 0x8000, 0x40000); I: `−1` (0x2000), `+1` (0x80000) — floored at 0; `delta = trunc(E[i]·600 + taxDemand[tax])`; `demand[i] = clamp16(demand[i] + (int16)delta, −2000, 2000)` with 16-bit wrapping add before the clamp.

Constants (read from the exe): `1.0, 1/150000, 0.01, 15.0, 600.0` @0x4D9CE8.. ; `econFactor` = `[0, 1.2, 1.1, 0.95]` @0x4E8528; `taxDemand` @0x4E8318 = `200,160,120,100,75,50,25,0,−25,−50,−100,−150,−200,−250,−300,−350,−400,−450,−500,−550,−600,−650,−700` (index = tax %). Reading is VERIFIED; the game-design meaning (tax 7% = neutral) is an inference.

## Validation
- Saves taken after phase 21 reproduce **exactly** `zonePop[0]`, `population10` and all three `budget[R/C/I].currentCosts` (including the `extraPop/6` and `/12` terms) in 7/7 files (CAPE, FLINT, EM, HAPPYLND, HI-LOH2O, NEWMONRO, ROME).
- The demand *valves* themselves are not independently verifiable from a save (they integrate previous state). In FLINT (1995 semantics) one more step moves the saved demand by +36/0/+11 — plausible.
- **Provenance note (MISC word 29):** in 23 sample files (mostly Maxis scenarios) word 29 equals the residential population, as this build writes it; in the others it equals last month's *residential graph value* (`XGRP` series 1, entry 1 = 10·pop + extraPop/2 for the previous month) — evidently saved by a different build of the game. Our port follows build .022 (word 29 = residential population); loading a city saved by the other build gives a ~10× larger `ratio` for the first month.

## Native
`runDemand(CityState&, GameTables, DemandExtra)`; constants come from `GameTables::fromExe`.
