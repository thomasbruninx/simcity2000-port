# Zone types 7–9: military, airport, seaport

Port: `src/simulation/special.cpp` (placement) and `militaryZone` / `airportZone` / `seaportZone` in `src/simulation/zone_pass.cpp`.
Original: `SIMCITY.EXE` build .022, branches of the zone pass `0x435220` and the helpers below.
Validation: differential test against the original machine code, `sc2k-emu-diff zone` (variants 3–6 paint zone 7/8/9 blocks and
cycle the military-base type); see [validation/006](../validation/006-differential-testing.md).

## Zone nibble meanings (XZON & 0x0F) — VERIFIED from the pass
| nibble | meaning | branch |
|---|---|---|
| 1..6 | residential/commercial/industrial low-medium-high density | growth logic (see zone-growth.md) |
| 7 | military base | `militaryZone` |
| 8 | airport | `airportZone` |
| 9 | seaport | `seaportZone` |
| ≥ 10 | underground decay of subway pieces | see below |

## Functions ported
| Address | Port | Notes |
|---|---|---|
| 0x437C30 `(a,b,id,zone)` | `placeZoneBuilding` | returns false when the spot is rejected; **zone ≠ 7 requires power nearby** (0x436680) |
| 0x438C20 `(a,b)` | `clearSpecialBuilding` | clears a 2×2 block (even-aligned) or a single tile, XBIT &= 0x3F, XZON &= 0x0F |
| 0x440600 `(a,b,id,size)` | `placeSpecialBuilding` | validates the footprint, stamps tiles with flags 0xE0 (0x20 for ids 0xD5/0x0D) and corner bits |
| 0x44C8E0 / 0x44D740 | – | microsim/label allocation; **returns 0 for every id placed here** (class 0 in the table at 0x4E6C8A), so no overlay is allocated |

Tables read from the exe: pier direction vectors `0x4E6590` (dx) / `0x4E6598` (dy) = (0,1,0,−1)/(1,0,−1,0); corner bits `0x4DB330`.

## Building choices (all VERIFIED against the decompilation and the emulator)
`mil[i]` = military tile counters (MISC 1002+i); `n[id]` = tile census; `q4(x)` = signed x/4.

* **Military** (`militaryType` MISC 915): every case does `if ((rand() & 3) == 0)` except case 5.
  * 2: `id = mil[13]/12 < q4(mil[3]) ? 0xE8 : 0xEF`, fallback 0xE8.
  * 3: `q = (mil[2]+mil[1])/5`; if `q4(mil[3]) < q`: first of 0xE2 (`mil[11]*2<q`), 0xEA (`mil[5]*2<q`), 0xE7 (`mil[12]<q`), 0xE4 (`mil[7]/2<q`), 0xE5 (`mil[8]/2<q`), else 0xF6 (0xEF if `q <= q4(mil[14])`); otherwise 0xDD (runway).
  * 4: `q = mil[10]`; if `q4(mil[4]) < q`: 0xF1 if `q4(mil[9]) < q` else 0xE3 (0xF2 if `q <= mil[6]/3`); otherwise 0xE0 (pier); fallback 0xE3.
  * 5: a 3×3 command centre 0xF9 unless the tile already is one.
* **Seaport**: `q = n[0xE0]`; `q4(n[0xF2]) < q` → 0xF0 if `q4(n[0xF0]) < q` else 0xE3 (0xF2 if `q <= n[0xE3]/3`); else 0xE0; fallback 0xE3. If the `rand()&3` test fails and the tile is a pier (0xE0), another `rand()&3` decides whether to call the ship spawner 0x452660 (hook `spawnSeaTraffic`).
* **Airport**: `q = (n[0xDE]+n[0xDD])/5`; `q4(n[0xEE]) < q` → 0xE1 (`n[0xE1]*2<q`), 0xEA (`n[0xEA]*2<q`), 0xE6 (`n[0xE6]<q`), 0xE4 (`n[0xE4]/2<q`), 0xE5 (`n[0xE5]/2<q`), else 0xF6 (0xEE if `q <= q4(n[0xF6])`); otherwise a runway 0xDD. Else, on a powered runway tile with `rand()%30 == 0`: `rand()%10 < 4` calls 0x452500 (`spawnAircraft`), otherwise 0x452190 (`spawnPlane`) with direction 0 or 2 chosen by view rotation parity and the tile's flip bit.
* **Underground decay (zone ≥ 10)**: with probability 1/128 (LFSR `&0x7F`) on a subway piece, if the subway funding ≠ 100 and `funding <= rand()%100`: tile 0x23 is destroyed (0x458690, no animation), other pieces are removed (0x441D20 with 0).

## Placement details (0x437C30)
* **Runway 0xDD**: a line of 5 non-runway tiles of the right zone; direction depends on the *parity of the runway tile count* (`census[0xDD] & 1`) and the parity of (a, b); existing runway tiles (0xDD/0xDE) do not count towards the 5. Tiles alternate 0xDD/0xDE logic: existing 0xDD whose flip bit differs from the wanted orientation become 0xDE. Orientation = view-rotation parity XOR line direction. Non-military runways get XBIT |= 0xC0 (powerable/powered).
* **Pier 0xE0**: needs water in one of 4 directions next to it, 5 water tiles free of buildings in that direction, and the end tile's altitude word must satisfy `((alt & 0x3E0) >> 5) >= (alt & 0x1F) + 2`; the pier is the tile itself (0xE0) plus 4 tiles 0xDF.
* **1×1** (0xE1–0xE8, 0xEA): only placed over rubble/clear (`XBLD < 0xD`).
* **2×2** (0xEE–0xF2, 0xF6): anchor is the even-aligned corner; rejects any tile with id > 0xEA (first tile only), runway/pier pieces, or a different zone; existing buildings are cleared first.
* **3×3** (0xF9): the anchor slides up to two tiles toward −a and −b while the neighbour has the same zone nibble, then the 3×3 block is stamped over *whatever is there* and each tile gets underground piece 0x22.

## Known deviations / undefined behaviour (documented, not "fixed")
* The original reads outside its row-pointer tables at the map border; the port reads 0 there.
* The 0xF9 command centre is stamped over neighbouring non-military tiles without any check. If it overwrites tiles of a growth zone (zone 1–6), the original later computes `buildingSize[id-0x70]` with id ≥ 0xF0, i.e. reads **past the end of the table** (a value of 148 was observed for 0xF9). The port's tables have only the documented entries, so that state is **not reproduced**; the differential test keeps a margin of unzoned tiles around painted blocks to avoid it. (UNDEFINED in the original; effect on a real game is a mis-sized growth update on those tiles.)
* The thing spawners (0x452500, 0x452190, 0x452660, 0x452EF0, 0x452CE0) are hooks; the emulator replaces them with a plain `ret`.
