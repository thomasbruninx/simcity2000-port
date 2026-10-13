# City-building tools

Literal ports of SIMCITY.EXE build .022 (`src/simulation/tools.{h,cpp}`); the interactive mouse loops of the originals are replaced by explicit
parameters and `ZoneHooks` callbacks, everything that changes the city is the original logic. **Verified bit-exact** by the emulator diff
(`sc2k-emu-diff tools`): 16 runs (8 cities + the empty `DEFAULT.SC2` template × 2 funding levels) × 500 scripted operations, every tile layer, counter,
RNG state and return value compared after every operation (docs/validation/006).

## Toolbar structure (dispatcher 0x43EBC0)
`DAT_004CA47C` = tool category, subtype globals `DAT_004CB968..CB984` one per category. Category → handler:

| cat | tool | handler(s) | cost row |
|---|---|---|---|
| 0 | bulldozer (sub 0), level/raise/lower (1–3, ported), de-zone (4, via 0x448F70) | 0x442C20 (+drag 0x442760) | 0x4DAFA0[0..4] |
| 2 | landscape (trees, water, terrain) | 0x44F4D0/0x44F6E0/0x44F8F0 | *not ported* |
| 3 | power: sub 0 line, 2–10 plants, 3 → 0x440DB0 | 0x449C90 / 0x43FB20 | row 36.. |
| 4 | water: sub 0 pipes, 1–4 pump/tower/treatment/desalination | 0x449F20 / 0x43FB20 | row 48.. |
| 5 | services/rewards (0x43FB20) | | row 60.. |
| 6 | transport: 0 road, 1 highway (0x461B80), 2 tunnel (0x44B820), 3 onramp (0x441160), 4 bus depot (0x43FB20) | 0x449530 | row 72.. |
| 7 | rail: 0 rail, 1 subway, 2 train station, 3 subway station, 4 → 0x441530 | 0x4498D0 / 0x44A0A0 / 0x43FB20 | row 84.. |
| 8–0xB | zoning (airport+seaport, R, C, I) | 0x448F70 | rows 96.. |
| 0xC–0xE | education, safety, recreation buildings | 0x43FB20 | rows 144.. |
| 0xF / 0x10 / 0x11 | microsim edit, query, centre | 0x44D140 / 0x471E80 | *not ported* |

Cost table `0x4DAFA0` (int32 per `[category*12 + subtype]`, −1 = no such button) and footprint table `0x4DAEC8` (bytes, 255 = none) are read from the exe
(`GameTables::toolCost`, `toolBuildingSize`). The (category, subtype) → building id table is the stack initialiser of 0x43FB20 (`toolBuildingId`).

## Ported pieces
* **Bulldozer** `bulldozeTile` (0x442C20): military zones are refused; non-underground mode refuses rubble-free empty land; 1 in 20 trees triggers the
  environmentalist message (funds are spent, nothing removed); buildings go through `destroyTile` (0x458690); water-edge terrain is flooded/reshaped
  (0x4469C0/0x446000); afterwards road/rail/power and subway/pipe neighbours are refitted (0x44C120/0x44C330). Underground mode removes pipes/subway.
* **Zoning** `paintZone` (0x448F70): rectangle; cost = unit × tiles that would change zone (count from 0x447D00: flat land, building < 0x1D, not 5/0xD, not military,
  not already that zone). De-zoning also clears rubble 1–4.
* **Network drags**: the path planner 0x448540 (start check, direction choice 0x44C000/0x44C070, bounds), 0x4488D0 (one step: crossings allowed over perpendicular
  roads/rails/lines, slope and water-table rules, altitude rules for rails), 0x4487A0 (reshape terrain under the route), preview/cost 0x448410
  (`length × unit + 25 × tiles needing levelling`). Entry points `buildNetwork` (road 0x449530, rail 0x4498D0, power 0x449C90, subway 0x44A0A0, pipes 0x449F20).
  Pieces are applied by 0x44A640/0x44AB20/0x44A220/0x44B4A0/0x44B110 and oriented by the neighbour fixers 0x44A850/0x44AD20/0x44A400/0x44B5C0/0x44B250, which use
  the connection tables at 0x4E6B08–0x4E6E10 (copied from the exe: `GameTables::connectData`).
* **Bridges** `buildBridge` (0x45B280): clicking a water-edge slope (XTER 0x20–0x3F, directions from a table) or dragging a road/rail/line to the shore. Length 1–11+ chooses
  among types (power line, rail, three road bridges, allowed by length) through `ZoneHooks::chooseBridge` (the original opens a dialog, 0x45B140). Cost per tile from 0x4E75A8.
* **Border connections**: ending a road/rail on the map edge asks (`ZoneHooks::confirm(0x6D / 0x6E)`) to connect to the neighbouring city for 1000 / 1500 and stores marker 0xFA in
  XTXT (adds an edge stub to the piece; increments MISC `cb4f0` / `c9408`, which feed the commercial / industrial demand limits).
* **Buildings** `placeBuilding` (0x43FB20): funds and size checks, power plants/treatment/prisons are refused with probability when many residential tiles are within 8 tiles
  (LFSR), placement by `placeSpecialBuilding` (0x440600), per-building follow-ups (budget unit counters for 0xD1/D2/D3/D6/D9, pump → pipe piece, subway station → XUND 0x23), and an immediate
  power/water rescan while the population is below 5000.

## Terrain, highways, tunnels and the single-click tools (session 13)
* **Terrain** (`raiseTerrain` 0x443610 + 0x443660/0x443B70/0x445500, `lowerTerrain` 0x443DF0, `levelTerrain` 0x442BB0): raise checks recursively (marking tiles with XBIT 8) that no military zone is touched, no altitude exceeds 30 and, unless the bulldoze subtype is 0, no water; each raised tile costs `toolCost[2]` and adjusts neighbours that would differ by more than one step; lowering walks a FIFO (the flood queue) lowering neighbours that would become too steep, then re-fits shorelines in the touched rectangle (`UnionRect` ignores empty rectangles). The shoreline fixer 0x4469C0 runs on 9 tiles around every change.
* **Highways** (`buildHighway`, `buildHighwayBridge`, `previewHighway`; src/simulation/highway.cpp): 2x2 pieces on even coordinates with their own planner (0x461370 / 0x461460), site classifier 0x4639D0 and height 0x4638F0, piece chooser 0x4622A0 (uses the neighbour pieces' classes from 0x4635D0), piece placer 0x462860, apply 0x462060, bridges 0x45BB30. Cost 25 per tile (100 per piece). **Original quirk (VERIFIED by diff): every "classifier == 0x8000" test compares a sign-extended `short` with +0x8000 and is never true**, so unusable sites are not rejected there; the port reproduces that. The highway bridge charges nothing (no subtraction in 0x45BB30).
* **Tunnel** 0x44B820 (starts on a hillside slope XTER 1–4, digs through a hill: length limits, no water table, no pipes/subway at depth 1, exit slope must match; the question 0x15A is `ZoneHooks::confirm(0x15A, cost)`), **onramp** 0x441160, **rail–subway link** 0x441530 (free), **hydroelectric dam** 0x440DB0 (on waterfall slopes XTER 0x2E/0x3E; plant id 0xC6 or 0xC7 by orientation; costs 400).
* The terrain, tunnel and highway code is verified with the extended script (`EXTENDED=1`): 20 runs x 800 operations on 10 cities identical.

## Landscape and emergency dispatch (session 14)
Category 1 (0x441DF0) is the **landscape** tool: tree (0x43F4E0, $3; shift removes) and water (0x43F7E0, $100); the original terrain editor adds forest (0x43F620, 20 random trees) and stream (0x445DF0, a random walk of up to 100 tiles that floods tiles). Category 2 is **emergency dispatch** (not "landscape" as first guessed): `startDispatch` (0x44F290, when a disaster starts) sizes the markers from the police / fire station counts (census/8) and the military base type; each click places a marker thing (types 7 / 8 / 0xE, overlay value = slot + 0xC9) and removes the oldest of that kind (0x44F4D0 / 0x44F6E0 / 0x44F8F0 via 0x450010). Verified with the LEVEL=2 script (20 runs x 800 operations identical). Findings: the highway border-link test reads the **start** tile's XTXT, not the end tile's; the keyboard query 0x42AD10 returns a full `EAX` (the stub must zero-extend).

## Not ported yet
The terrain editor's drag tools 0x443400 / 0x4432B0 / 0x4442A0 / 0x4423E0 (mouse-sample loops), query / microsim editing (0x44D140, 0x44CB30, 0x471E80), the stadium sport dialog (0xD7), power-plant selection by year, and the budget-window counters stored beside the budget records for the same buildings. Bonds are not ported (strings 288–297 describe up to 50 bonds of $10,000 with rates).

## Emulator-harness notes (dev only)
* The interactive loop 0x447F90 / 0x447910 is replaced by a 8-byte stub reading the drag target from memory (patching an immediate left stale translated blocks in Unicorn).
* 0x408600 (message box) is `thiscall ret 0xC`; 0x4B1223/0x4B1243 `ret 0xC`; 0x42B0C0 plain `ret`; 0x424AF0 `ret 4`. Wrong stack cleanup shows up as a jump to garbage (here 0xC0000 = a pair of shorts).
* `ToolScript` (src/simulation/tool_script.h) generates the operation sequence for both the emulator run and the unit test, which replays 500 operations on the port alone and compares the
  fingerprint with the value printed by the emulator run.
