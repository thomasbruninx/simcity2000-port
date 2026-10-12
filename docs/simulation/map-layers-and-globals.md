# Map layers and key globals (build .022)

Status: layer identity is **STRONG INFERENCE** from how the monthly-phase code uses each array (see [monthly-phases.md](monthly-phases.md)); the globals' *addresses* are VERIFIED (read from decompilation), the *names* are ours. Saved-city chunk names (XTER, XBLD, …) are from the file format; mapping runtime array → chunk is our inference unless marked.

## Storage convention (VERIFIED)
Each map layer is an array of **row pointers**: `layer = (BYTE**)ADDR`, access `layer[y][x]` (128 rows for 128×128 layers; 64 for half-res; 32 for quarter-res). 16-bit layers use `short` rows. Coordinates in the code are `short` with explicit bounds tests `0 <= v < 0x80`.

## 128×128 byte layers
| Row-pointer table VA | Our name | Evidence | Confidence |
|---|---|---|---|
| 0x4CB508 | `XBLD` building/tile id | ids >0x6F tested as buildings, 0xC6–0xCF power sources, 5 = radioactive waste (+200 pollution), 6–0xD trees (land-value bonus), 1–4 rubble; per-id tables (pollution, population) indexed by it | STRONG |
| 0x4C9208 | `XZON` zone | low nibble = zone type 0..9 (1,2 res; 3,4 com; 5,6 ind; 7 military; 8 airport; 9 seaport per switch layout), high bit tests `0x80`/`0xF0` | STRONG (type numbering: res/com/ind = 1–6 matches case structure; names of 7–9 HYPOTHESIS) |
| 0x4C9B28 | `XBIT` flags | `&0x08` mark (cleared by phases 1, 20 and scan), `0x10` watered, `0x40` powered, `0x04` water tile, `0x80` powerable, `0x02` flipped; matches saved-bit counts (NYC: 0x04 set on 7,378 tiles) | STRONG |
| 0x4C9618 | `XTXT` overlay | values 0x33–0xC8 index microsim records (`record = XMIC + (id−0x33)*8`), see 0x45C870 | STRONG |
| 0x4CA1E8 | `XUND` underground | ids 1..0x0F,0x1F,0x20,0x22,0x23 decay-tested (subway/pipe/rail pieces) | MODERATE |
| 0x4C8F70 | (unknown byte layer, values 1..15 add +12 to land-value accumulator) | – | UNKNOWN (candidates: `XTER`/water/slope) |

## 128×128 word layer
| 0x4CB710 | scratch `short` rows reused by phases as 64×64/32×32 accumulator | UNKNOWN reuse — treat as temporary |

## 64×64 byte layers (half resolution; index `[y/2][x/2]`)
| VA | Our name | Evidence | Chunk |
|---|---|---|---|
| 0x4C9958 | traffic | phase 19 decays it ×¾ per month-phase and sums into 0x4CB70C; pollution adds `traffic/5` | `XTRF` |
| 0x4C9840 | pollution | smoothed avg of 4 neighbours + own-cell building pollution (+`traffic/5`, +200 for id 5); total → 0x4CB928; later subtracted in land value | `XPLT` |
| 0x4CA0C0 | land value | zone-type-dependent smoothing, − pollution/4, − crime/3, + population density/3; total → 0x4C9458 | `XVAL` |
| 0x4C94F0 | crime | smoothed (neighbour avg) of `popDensity − landValue/4 − police/2 (+16 w/ ordinance)`; total → 0x4CA46C | `XCRM` |

## 32×32 byte layers (quarter resolution; index `[y/4][x/4]`)
| VA | Our name | Evidence | Chunk |
|---|---|---|---|
| 0x4C9460 | population density | `min(255, 4 × accumulated building population)` | `XPOP` |
| 0x4CA040 | rate of growth | `(7·old + 8·(Δdensity)+128)/8` clamped 0–255 (exponential smoothing of density change) | `XROG` |
| 0x4C8448 | police coverage | stations (id 0xD2) spread via `0x46B420`, strength from budget funding; +2 per tile with ordinance bit 0x800 | `XPLC` |
| 0x4C9A90 | fire coverage | stations (id 0xD3), +2 per tile with ordinance bit 0x10 | `XFIR` |

## Scalars / records
| VA | Meaning | Evidence |
|---|---|---|
| 0x4C9E1C | day counter (int32) | `SimulationTick` |
| 0x4CA034 / 0x4CA038 / 0x4CA400 | month / year-offset / season | derived each tick |
| 0x4C960C | start year (base for `year + start`, 0x76C = 1900 used as offset in tables) | 0x4686C0 |
| 0x4C945C | treasury (int32; game over if < −100000) | phase 22, budget |
| 0x4C9A58 | ordinance bitmask (20 bits) | demand adjusts (`&2,&1,&0x1000,&0x8000,&0x40000,&0x2000,&0x80000,&4,&0x800,&0x10`) |
| 0x4C94E0 | `short[256]` tile census (count of each XBLD id) | summed over id ranges to get maintenance costs |
| 0x4CB4E4 | budget array: 16 × 0x70-byte records (see below) | decompiled phases 0 and 21 |
| 0x4CB910/12/14 | residential / commercial / industrial demand (`short`, clamped ±2000) | 0x467AE0 |
| 0x4C9440 / 0x4C9948 | zone population accumulators (`int[8]`, reset at phase 0), summed per R/C/I | 0x467AE0 |
| 0x4C9A68 / 0x4CB4D0 | % powered / % watered | 0x4671F0 / 0x463EE0 |
| 0x4C6318 | game speed | see clock doc |
| 0x4CB488… (16 pointers) | graph history, 16 series × 52 `int32` | 0x413910 |
| 0x4C944C | thing/sprite array base (12-byte records, ~40) | 0x45C870 |
| 0x4CA404 | microsim array base (8-byte records) | 0x45C870 |

### Budget record (0x70 bytes) — matches sc2kfix `budget_t` (VERIFIED by field offsets used)
`+0x00 int[12] countMonth; +0x30 int[12] fundMonth; +0x60 currentCosts; +0x64 fundingPercent; +0x68 yearToDateCost; +0x6C estimatedCost`.
Category order (VERIFIED by `0x1b0, 0x220, 0x290, 0x300, 0x370, 0x3e0, 0x450, 0x4c0, 0x530, 0x5a0, 0x610, 0x680, 0x6f0` offsets): 0 residential, 1 commercial, 2 industrial, 3 ordinances, 4 bonds, 5 police, 6 fire, 7 health, 8 schools, 9 colleges, 10 roads, 11 highways, 12 bridges, 13 rail, 14 subways, 15 tunnels. Monthly: `countMonth[month] = currentCosts; fundMonth[month] = fundingPercent; yearToDate += currentCosts × fundingPercent` (0x473980). In January (after a flag set in December) the year's cost is added to treasury: `treasury += yearToDate / (table4E8EA0[i] × 12)` (exact meaning of the divisor table UNKNOWN).
Tile-census → maintenance classes (0x473980): XBLD ids 0x1D–0x2B,0x3F–0x46,0x4B,0x4C,0x5D–0x60 → roads; 0x2C–0x3E,0x45–0x48,0x4D,0x4E,0x6C–0x6F → rail; 0x51–0x5C,0x6A,0x6B → bridges; 0x49–0x50,0x61–0x6B → highways; 0x3F–0x42 → tunnels (class assignments from the code's range tests; real tile *names* UNKNOWN).

## Open questions
Which exact array is `XTER`; meaning of 0x4C8F70; table contents (building pollution 0x4E8638/0x4E85A8, population 0x4E8638?, `0x4E8318` tax→demand, `0x4E8528` doubles, `0x4E8448` year interpolation); exact rule for `0x46B420` coverage spreading.
