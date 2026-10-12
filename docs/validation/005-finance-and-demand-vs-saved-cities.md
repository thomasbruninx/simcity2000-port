# Validation 005 — finance and demand vs saved cities

| Check | Files | Result |
|---|---|---|
| Maintenance classes 3–15 recomputed from tile census == saved `currentCosts` | 6 files saved at phase 0 | **6/6 exact** (all 13 categories incl. ordinances) |
| `yearToDate == Σ count·fund` for months 0..current | 61 | 48 hold; exceptions: MAXISLND and 12 scenarios (hand-edited histories) |
| `CityState` read→write round trip of MISC | 61 | 61/61 byte-identical (after preserving upper halves of 16-bit fields) |
| Demand pass: `zonePop[0]`, `population10`, R/C/I `currentCosts` | 7 files saved at phase ≥ 21 | **7/7 exact** |
| Demand pass one more step: valve moves ≤ 400 | FLINT (the only file with 1995 `c91ac` semantics among them) | +36 / 0 / +11 |
| Demand numerics vs independent Python model | 2 synthetic vectors | exact double equality (guards typos, not misreadings) |

**Phase dependence.** Zone-population counters (`MISC 380..387`) are reset at phase 0 and filled by the zone passes (phases 3–18); only saves after phase 19 hold complete values, which is why demand is checked on phase ≥ 21 saves.

**MISC word 29 provenance.** 23 files store the residential population (this build), the rest store the previous month's residential graph value (10·pop + extraPop/2) — different game builds wrote these samples. Documented in docs/simulation/demand.md.

**Not verifiable from saves.** The valves themselves (stateful integrators), the random ordinance enactment, and the untouched second half of 0x467AE0.
