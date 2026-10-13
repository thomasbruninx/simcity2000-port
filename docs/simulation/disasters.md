# Disasters (0x45C870 and callees)

Port: `src/simulation/disaster.cpp` (`startDisaster`, `disasterFrame`, `disasterTick`), run after every `simulationTick` by `Simulation::tick` like the app's step machine does in the original. Differential test: `sc2k-emu-diff disaster` (`OPS=n`) runs each handler and four frames of the driver on 8 cities x 2 variants with scattered markers, flood state and emergency units: **identical** in tile layers, counters, thing table, RNG and LFSR states (VERIFIED).

## Markers and state
* XTXT overlay codes used by disasters: `0xFF` flames, `0xFB` spreading fire, `0xFC` flood water, `0xFD` / `0xFE` a burning vehicle on a road (the pair walks toward +x/+y respectively -x/-y). Thing slots (`slot + 0xC9`) of types 5 (monster), 0xF (tornado), 1 in state 7 (crashing plane / UFO) and 6 (effects) keep a disaster alive; emergency dispatch units are types 7 (police), 8 (fire), 0xE (military).
* `CityState::disasterKind` (MISC 1041 @0x4C9954) and `disasterActive` (MISC 1042 @0x4C8F00); `ThingState::floodTimer` (@0x4C83C0, counts down from 60), `floodBudget` (@0x4E76A0), `fireMessageKind` (@0x4E76A4, burnt-building class with the highest message priority).
* The pending request lives in `ThingState::disasterRequest` (set by `chooseDisaster`, a thing, or a scenario file) with the focus tile.

## Handlers (kind -> routine)
| Kind | Routine | What it does (names are the port's reading) |
|---|---|---|
| 1 | 0x45E290 | fire: spiral search from the city centre (+-20 random) for a building to ignite, then 200 random LFSR tiles |
| 2 | 0x45D460 | flood: nearest tile with terrain type 0x20..0x2F; water markers on its dry neighbours; else a random dry tile |
| 3 | 0x45DF50 (x3) | burning vehicle on the nearest free road tile |
| 4 | 0x45DEF0 | flames at the focus tile |
| 5, 0x11 | -- | nothing happens (flag only) |
| 6 | 0x45E490 | earthquake: within +-32 tiles about 1/64 of the tiles with buildings catch fire or collapse |
| 7 / 8 | 0x45EA00 / 0x45E780 | tornado / monster (thing spawners, ported earlier) |
| 9 | 0x45EC20 | explosion of the tile 0xCB (a power plant): demolish + flames + rubble around it |
| 10 | 0x45EF30 | 39-step random walk of fire starting at the tile 0xCD |
| 0xB | 0x45DC00 | impact barrage: the treasury is set to $25,000 and used up by terrain-raise attempts ($1000 for each refused), then restored |
| 0xC | 0x45D990 | firestorm: 65 fires spiralling out of the focus |
| 0xD / 0xE / 0xF | 0x45DAE0 / 0x45D330 / 0x45D210 | several vehicle fires / floods / flame markers around the focus (count = population/1000 + 5) |
| 0x10 | 0x45CE40 | flood from the map edge opposite the view rotation (demolishes buildings, lays water markers) |
| 0x12 | 0x45E130 | a plane-type thing in state 7 over the middle of the map |

The scenario files use 6 (San Francisco), 8 (Hollywood), 0x10 (Charleston, Homestead), 0xF (Chicago), 0xE (Davenport), 0xC (Malibu, Oakland, Paris), 9 (Barcelona, Manhattan), 0xD (Washington), 0xB (Portland), 10 (Silicon Valley), 7 ... -- the names above come from the behaviour; the scenario descriptions in the TEXT chunks would settle some (HYPOTHESIS for kinds 4, 9, 10, 0xB).

## Driver 0x45F0D0 (every tick while a disaster is active)
Scans the whole map once: flames (0xFF) spread by 1/4 chance per tile to a random neighbour or according to the fire-station coverage (`XFIR + 8`), demolish buildings and may spawn effects; spreading fire (0xFB) collapses the building and moves to the lowest neighbour; flood (0xFC) spreads to lower tiles while the timer runs; vehicle fires move along roads; emergency units (types 7 / 8 / 0xE) extinguish neighbouring tiles. After the scan: sounds, random flood damage while `floodBudget` lasts. Returns whether any marker remains.

## DisasterTick 0x45C870
Consumes the request (`disasterRequest = 0`), runs the handler, sets kind/active, plays the alarm (sound 0x208), maybe sends a military visitor (military base type 1, LFSR&3 == 0), then runs the driver. When nothing is left (no markers, no monster / tornado / effect / crashing plane) the disaster ends: `postMessage(kind -> news id)`, emergency markers are removed. The camera moves to the focus through `ZoneHooks::scrollTo`.

## Not ported
Drawing and screen shaking of the earthquake, the 5 ms delays between flood steps, the message-box texts, and the visual side of 0x451360 (which also consumes `rand()` in the original -- the harness stubs it, as for the thing system).
