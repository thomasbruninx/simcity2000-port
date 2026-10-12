# Random events, history graphs, disaster chooser and the 25-phase tick

Port: `src/simulation/tick.cpp`. Originals: second half of `0x467AE0` (events), `0x413910` (history), `0x458040` (traffic decay), `0x4720A0` (advisor + disaster
chooser), `0x412FE0` (tick). `simulationTick()` runs one simulation day (phase = `day % 25` after the increment):

| Phase | Work | Port |
|---|---|---|
| 0 | monthly finance (0x473980); zone-population accumulators cleared | `runMonthlyFinance` |
| 1 | power scan | `runPowerScan` → `powerPct`, `powerCapacity` |
| 2 | environment scan | `runEnvironmentScan` → totals, city centre, developed tiles |
| 3–18 | zone pass `runZonePass(ph/4…)` | `runZonePass((ph-3)/4, (ph-3)%4)` |
| 19 | traffic decay | `trafficDecay` |
| 20 | water scan (+ water-shortage flag) | `runWaterScan` |
| 21 | demand valves, **events**, **economy**, **history** | `runDemand`, `runEvents`, `runEconomy`, `recordHistory` |
| 22 | city-class milestones, scenario goal test (hook), bankruptcy (hook) | inline |
| 23 | UI refresh only | – |
| 24 | advisor topic (message hook) and the random disaster request | `chooseDisaster` |

Things (ships, trains, aircraft, …) are **not** part of the tick: the original updates them from the view's idle handler every ~30–45 ms of wall-clock
time (docs/simulation/things.md). A driver chooses how many `thingFrame()` calls to interleave per day.

## Events (0x467AE0 after the valves)
One random tile per month: rubble (id 5) may clear; burning tiles (ids 6..0xD) advance one frame (6→…→0xC) and ignite one random 4-neighbour; then a long list of
**advisor/news triggers** which are plain `rand()` comparisons against the graph values (traffic, pollution, crime, unemployment, education, health) — the port keeps every
`rand()` call and routes the message ids to `ZoneHooks::postMessage`. Dated events (MISC 462..478) fire when `startYear + year` reaches them (hook `scheduledEvent`).
Finally the **weather**: next state from table 0x4DAD18 indexed by `(state + 12·season)·8 + (rand & 7)`; cloudiness, wind and haze are averaged with per-state targets.

## History graphs (0x413910)
16 series × 52 samples (XGRP): monthly values shift every month; every 6 months (January/July) the 12..31 window shifts; every 5 years the 32..51 window shifts.
Series: 0 city size, 1–3 R/C/I size, 4 traffic, 5 pollution, 6 land value, 7 crime, 8/9 power/water shortage %, 10 health, 11 education, 12 unemployment-like (`c9610`),
13 GNP, 14 national population, 15 rate. Series 4, 5, 7 are also read by the thing frame (sound gates) and by the event triggers.

## Disaster chooser (0x4720A0)
Sets `ThingState::disasterRequest` (and the focus tile) with probability depending on difficulty grace months (table 0x4E88F8 by MISC 7) and on fire/flood/tornado/… conditions
(weather state, haze, pollution density, unemployment, power plants, …). The disaster *handlers* (0x45C870 and callees: fire, flood, earthquake, monster, …) are not ported yet.

## Not modelled (UI / scenario data)
Budget and newspaper dialogs, autosave (0x42FA50), military-base granting at the 4th milestone (0x413D80 shows a dialog), scenario goals, game-over screens: hooks in `ZoneHooks`.
Message queue (0x47BA90 / 0x479F40, MISC 946..999) is UI state and only reachable through `postMessage`.
