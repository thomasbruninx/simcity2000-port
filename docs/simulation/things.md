# Thing system (ships, aircraft, trains, monsters, …)

Port: `src/simulation/things.cpp` (+ `allocateMicrosim` / `populationLimit` in `special.cpp`). Original: `SIMCITY.EXE` build .022,
master update **0x450210**, called from the view's idle handler **0x407160** — i.e. **every animation frame (≈30–45 ms of wall-clock time), not once per
simulation day**. The monthly 25-phase tick (0x412FE0) never touches things; spawners are called from the zone pass (0x435220) and from the
disaster dispatcher (0x45C870). A faithful driver therefore interleaves `thingFrame()` with `SimulationTick()`; the number of frames per day in the
original depends on the machine speed (VERIFIED by reading 0x407160: GetTickCount gating). Drawing is omitted (0x451360 and everything below it are
presentation; with the view zoom the drawing code also consumes `rand()` — the port does not model that, see *Limits*).

## Thing record (12 bytes, `World::xthg`, slots 1..0x27; slot 0 = tool cursor) — VERIFIED
| byte | name | notes |
|---|---|---|
| 0 | type | 0 free · 1 plane · 2 helicopter · 3 ship · 4 tool cursor · 5 monster · 6 explosion/effect · 7,8,0xE tool objects (updates are no-ops) · 9 sailboat · 10 train engine · 11 car · 12 engine (tunnel/bridge) · 13 car · 0xF tornado · 0x10 visitor (disaster-site fly-in) |
| 1 | heading | 0..7 (8 compass directions; trains/sailboats use 0..3) |
| 2 | state | per-type state machine; trains: slot of the next car |
| 3, 4 | a, b | tile |
| 5 | altitude | |
| 6, 7 | sub-tile position (0..0x10 for most; ships 0..0xC; trains: *next tile*) | |
| 8, 9 | target tile (or sprite phase for monsters) | |
| 10 | saved XTXT value | restored when the thing leaves the tile |
| 11 | extra | monster variant, visitor target slot, explosion "ignites" flag |

The tile under a thing carries the XTXT value `slot + 0xC9` (0xC9..0xF0); values 0xF1..0xF9 are fire frames, 0xFA a road/rail marker, 0xFF a burning tile.

## Frame update (0x450210)
Counters (`ThingState`, none saved in a city file) are zeroed, then slots 1..0x27 are visited in order and dispatched by type:
1 `updatePlane` (0x4532E0) · 2 `updateHelicopter` (0x453A20) · 3 `updateShip` (0x4560B0) · 5 `updateMonster` (0x454220) · 6 `updateEffect` (0x454050) ·
9 `updateSailboat` (0x455C30) · 10, 12 `updateTrain` (0x454E60) · 0xF `updateTornado` (0x454800) · 0x10 `updateVisitor` (0x454980). Before the loop
two `rand()` calls are made *conditionally* (traffic > 0x23 and crime > 0x28 with police stations) for sound effects — both counters
(`ThingState::trafficAverage`/`crimeAverage`, written by the event half of demand) therefore influence the random sequence.

## Spawners
| Function | Port | Called by |
|---|---|---|
| 0x452500 | `spawnHelicopter(a,b)` | airport runway, 0xDD (40 % of the 1/30 events) |
| 0x452190 | `spawnPlane(a,b,dir)` | airport runway; either an incoming flight from a map edge or a departure |
| 0x452660 | `spawnShip(pierA,pierB)` | seaport pier 0xE0; enters at a map edge on terrain type 0x10 |
| 0x452CE0 / 0x452DC0 | `spawnSailboatsNearMarina`, sailboat | marina 0xF8 |
| 0x452EF0 / 0x452F60 | `spawnTrainNearStation`, `spawnTrain` | rail station 0xED (the first hooks were mis-named in the first zone port; renamed here) |
| 0x452900 | `spawnVisitor(kind)` | disaster dispatcher; flies in from 16 tiles away |
| 0x452BE0 | `spawnEffect(a,b,alt,state,extra)` | thing updaters (crashes) |
| 0x45E780 / 0x45EA00 | `spawnMonster`, `spawnTornado` | disaster dispatcher (kind 8 / 7) |

Spawn limits: planes < 2, helicopters < 1, monsters = 0 for aircraft, ships < 1, sailboats < `census[0xF8]/9` (and < 4 per marina), trains < `census[0xED]/4` (and ≤ 4).

## Helpers (all ported)
`freeThingSlot` 0x4501D0 · `removeThing` 0x450150 · `moveThing` 0x451070 (sub-tile carry, XTXT marker hand-over, drops the thing when it leaves the map) ·
`headingToward` 0x450F60 · `turnToward` 0x450E10 · `steerToward` 0x450ED0 · `coarseHeading` 0x450E60 · `wander` 0x450DC0 (CRT `rand`) ·
`avoidBlocked` 0x453E80/0x453F20 · train helpers 0x455720 / 0x4555C0 / 0x455A50 / 0x455840 / 0x455910 · ship helpers 0x456690 / 0x4565C0 / 0x456760 ·
sailboat helpers 0x455F50 / 0x455DC0 · `startFire` 0x460780 (+ 0x460CA0) · `floodTile` 0x446000 (recursive shore rebuild).
The three RNGs are used as follows: **CRT rand** for movement noise and spawn positions; **LFSR** (`lfsr.mod(n)`, 0x47FF40) for sailboat/ship/monster/effect decisions;
**LCG** (`lcg.mod(2)`, 0x47FEF0) only for train routing.

## Microsim allocation (needed when a monster builds)
`placeSpecialBuilding(…,200,1)` (a monster of variant 3 builds building 200) calls 0x44C8E0: it reserves an 8-byte XMIC record (slots 10..0x95
dynamically, or a fixed slot for the special classes in the table at `0x4E6C8A`), initialises it (0x44D740: capacities, year, `rand()` for 0xF3/0xFF)
and copies the 25-byte name template into XLAB. Name templates are game text loaded at run time → `ZoneHooks::labelTemplate`; the default (none) copies zeros.

## Validation
`sc2k-emu-diff things`: 61 cities × 4 variants (saved things; fresh spawns of trains, sailboats, planes, helicopter, ship; + monster or tornado, visitor
and effect) — spawners **and** 160 animation frames are run in the emulator and in the port and compared after every frame (world layers, XTHG, XMIC,
XLAB, counters, `rand`/LFSR/LCG state). See [validation/006](../validation/006-differential-testing-against-original-code.md).

## Limits and deviations (documented, not "fixed")
* Drawing is omitted. In the original every active thing is drawn twice per frame when the view zoom ≥ a per-type threshold, and some drawing routines call
  `rand()` (e.g. 0x452020, 0x451880). That makes the original's random sequence depend on the player's zoom level; the port (like the emulator
  test, which stubs 0x451360) does not consume those values.
* The original reads outside its row-pointer tables when a visitor or other thing stands on the map border (state 1 of the visitor looks up
  `XTXT[a+dx][a+dy]` — a typo that uses `a` twice, preserved). The port reads 0 there; the emulator faults, and such runs are reported as "skipped".
* Aircraft collide with buildings using the *sprite height* word of the building's sprite (`0x4CA1D0 + id*8 + 4`, game data from the sprite archives):
  `ThingState::spriteHeight`, default empty (= 0 → only ids > 0xFA crash planes).
* The decompiler mixes up two stack variables in the ship update (0x4560B0 state 0): the pier-head probe uses the ship's own `b`, not its target `b`
  (found by the emulator diff, verified in the machine code).
