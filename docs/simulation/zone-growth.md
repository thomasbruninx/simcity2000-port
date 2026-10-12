# Zone growth pass, tile primitives and traffic router (phases 3–18)

Status: **ported and bit-verified against the original machine code** (docs/validation/006) for zone types 0..6. Not ported yet: zone types 7–9 (military, airport, seaport — they call 0x437C30), the underground-decay branch (zone nibble ≥ 10), and the effect routines reached through hooks (0x458690 destroy, thing spawners 0x452xxx, UI scroll/message calls). Code: `src/simulation/{zone_pass,tile_ops,traffic}.cpp`, `sim_context.h`.

## Structure of `0x435220(a0, b0)`
`SimulationTick` calls it 16 times, phases 3..18, with `(a0, b0) = (0..3, 0..3)`; each call visits tiles `(a0 + 4i, b0 + 4j)` — every tile once per month. Per tile (zone nibble `z` of XZON, building id `id` of XBLD):

| Case | Behaviour |
|---|---|
| `z == 0`, `id ≤ 0x1C` | nothing |
| `z == 0`, `id > 0x1C` | with probability 127/128 (`LFSR & 0x7F ≠ 0`) go straight to the special-building section; otherwise **infrastructure upkeep**: a road / rail tile is replaced by rubble (`(rand&3)+1`, powerable bit cleared) when `fund != 100 && fund <= rand()%100` (funding % of budget categories road 10, rail 13); a highway 2×2 block (only at even coordinates) turns to rubble/water with `rand()%100 >= fund(11)`; bridges (`(rand%50 + wind) >= fund(12)`) and tunnels (`rand%100 >= fund(15)`) call the destroy effect; then the special section |
| `z ≥ 7` | military / airport / seaport / underground decay (not ported) |
| `1 ≤ z ≤ 6`, empty lot (`id < 0x1D`) next to a road/station within the 24-neighbourhood | may start a building |
| `1 ≤ z ≤ 6`, building anchor tile (`XZON` corner bit == view-rotation mask) | may decline, finish construction, be rebuilt, or upgrade |

**Common growth logic** (empty lot: size 0; building: size = `buildingSize[id−0x70]` ∈ 1..4):
1. If no powered tile in the 5-point neighbourhood (`poweredNearby`, with the original `1 < a` quirk) *or* the traffic router finds no destination (`routeTraffic(a,b,zone,size,100)`): `growth = 0, decline = 4000`; else `growth = demand[(z−1)/2] + 2000`, `decline = 4000 − growth` (16-bit).
2. Class `k = tileClass[0x70 + sizeIdx]` (0 = ordinary building / empty lot, 1 = construction site, 2 = abandoned).
3. Ordinary building (`size > 0, k == 0`): `zonePop[z] += sizePop[size]`; with `rand() < decline/size` the building **collapses** (`collapseBuilding`).
4. `k == 1`: with `rand() < 0x4000/size` the construction **completes** (`stampBuilding(size, level=(z−1)/2)`, or an arcology lot clear when the city has a big population and `(size & 2) && z < 3`).
5. `k == 2`: `zonePop[7] += sizePop[size]`; with `rand() < growth*15/size` an abandoned building is **rebuilt**.
6. Otherwise (not size 4; light-density zones 1/3/5 only on empty lots): land-value gate (size 1: ≥0x20, 2: ≥0x60, 3: ≥0xC0, none for industrial) and `rand() < growth*3/(size+1)` → `growBuilding` (new construction site or merge of neighbouring small lots).

`rand()` is the CRT generator, `LFSR&0x7F` the 16-bit LFSR (docs/simulation/rng.md). The pass stores `-1000` in `0x4C9D64` at the end (unmodelled).

**Special-building section** (ids > 0xEC, reached from the upkeep branch): 0xED / 0xF8 spawn things when powered with `LFSR & 3 == 0` and below a limit (hooks); ids 0xFB–0xFE with the corner flag 0x80 update a microsim record: `min(12, max(0, (value>>5) − (crime>>5) − (pollution>>5) + 12))`, halved if unpowered, halved again if unwatered, stored at byte +1 of the record `XTXT[a][b] − 0x33` (XTXT values 0x33–0xC8).

## Tile primitives
- `setTile (0x4418B0)`: updates the tile census (`census[old]--, census[new]++`) — except in military zones, which use the 16 military counters (MISC 1002–1017) by building id; ids are bytes.
- `stampBuilding (0x436800)`: size 1 → one tile (`level 0`: id = 0x70 + 4·bucket + (rand&3) with bucket from land value; other levels `base[1+4·level] + rand % range`), sets zone corner bits 0xF0 and XBIT 0xE0 (powerable | powered | piped); size ≥ 2 → a `(size/2+1)²` block with a single random pick for all its tiles, XZON low nibble kept zero, XBIT |= 0xE0 and the four corner flags from `rotationCorner[rotation·8 + {0,2,4,6}]`. Level 3 = construction site, level 4 = rubble.
- `collapseBuilding (0x4376C0)`, `growBuilding (0x436F40)` (+`canBuildAt 0x437AE0`, demolition of small buildings `0x437910`), `clearLotForArcology (0x436C90)`: see code.
- `hasRoadNearby (0x456E80)`, `poweredNearby (0x436680)`, `setUnderground (0x441D20)`: see code.

## Traffic router (`0x456F70`)
Depth-first search with an explicit stack (the 512-entry queue ring used as a stack): start from the first road / station tile in the 24-neighbourhood of the origin, move in the four directions (random start direction and rotation parity from two `rand()` calls), switching between **14 transport modes** (road, highway, tunnel, bridge, their "after transit" variants 4–7, bus stop 8/9, rail station 10 → rail 12, subway station 11 → subway 13) with per-mode step costs (1–4); success = stepping onto a tile whose zone type is in `zoneDestMask[origin zone]` (or off-map onto an XTXT `0xFA` tile, or an id > 0xFA). On success the path is unwound: tiles of road-like modes get `kind` added to the 64×64 traffic layer (saturating at 255) and ridership counters (`bus`, `rail`, `subway`) are increased by `kind`. Step budget: `maxSteps` (100), reduced by 25 % for `kind == 1`.

## Validation
`sc2k-emu-diff zone`: original `0x435220` (all 16 sub-passes × 3 sweeps) in the emulator vs. the port, 61 cities × 3 perturbed economies (growth/decline weights, funding, big-city flag), with zone types 7–9 stripped from the input and the same UI/spawner stubs on both sides: **183/183 runs identical** in every layer (XBLD, XZON, XBIT, XTXT, XUND, XTRF, XMIC, …), census, zone populations, military counters, `rand()` state, LFSR state and ridership counters (92,739 tiles changed in total).

## Open
Zone 7–9 branches, the destroy effect (0x458690), thing spawners, and what writes `DAT_004E89C0/C8`.
