# New-city setup (0x42DDE0, reset 0x434250)

Port: `src/simulation/new_city.{h,cpp}` (`resetCityGlobals`, `setupNewCity`), `Simulation::startNewCity`, `GameUi::newCityDialog` (Ctrl+N; headless script `newcity <era> <difficulty>`).

| Step | Original | Status |
|---|---|---|
| Reset: calendar 0, milestone 0, rotation 0, centre (0x40,0x40), ratio memory 0, ALTM &= 0x3FF on every tile; pending disaster, speed option, window text and many UI globals | 0x434250 | only the simulation-visible parts ported (STRONG INFERENCE for the rest being UI) |
| Start year = (era + 0x26) * 50, era = dialog 101 choice of 1900 / 1950 / 2000 / 2050 | 0x42DDE0 | VERIFIED from the decompilation |
| National population 10 000 / 25 000 / 60 000 / 150 000 by era; rate level 3; GNP = nat.pop * 3 / 10 | | VERIFIED (decompilation) |
| Dated events: `eventYears[i] = table@0x4E89D8[i] + rand % 20`, set to 0 if that is before the start year; then tool availability 0x469A10 | | VERIFIED (decompilation); uses the shared RNG |
| Funds: easy $20 000, medium $10 000, hard $10 000 plus one $10 000 bond at the rate level (3) | | VERIFIED (decompilation); the dialog lists "$10K Bond" for hard |
| Maps flagged as neighbour-free (`c84d8`): neighbour names / connections cleared | | STRONG INFERENCE |

Not differential-tested: the routine reads its parameters from a dialog object, so only a unit test checks the values. The map is the caller's (DEFAULT.SC2 in the app); the original's random terrain generator ("Edit New Map") is not ported.
