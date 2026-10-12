# Simulation clock, speed and calendar (build .022)

Status: control flow **VERIFIED** by reading decompilations (VAs); names are ours, semantics of the individual phase calls UNKNOWN unless stated.

## Original implementation

1. **Timer source** — `StartSimTimer(200,200)` (VA 0x40BF80; only call at 0x409DFC): `timeBeginPeriod`, `timeSetEvent(period=200 ms, resolution=200, callback 0x40CA90, user=HWND, TIME_PERIODIC)`; fallback `SetTimer(hwnd,1,200,proc 0x40CAC0)`. Callback only does `PostMessage(hwnd, WM_TIMER, 1, 0)`.
2. **Tick pacing** — frame `OnTimer(id==1)` (VA 0x40BBB0 → 0x40BBD0):
   - `phase = ++g_tickPhase` (int32 @0x4E53A8); when `|phase| & 7 == 0` → `phase = 0` and `animOn = 1` (@0x4C6324); `animOff = 1` (@0x4C6328) each tick. If colour cycling active (@0x4C632C) these trigger palette toggles (`0x456A60(palette, 0/1)`).
   - Speed variable `g_speed` (@0x4C6318 = app+0x308; values 1..5 = paused, turtle, llama, cheetah, african-swallow; enum from sc2kfix, consistent with this code) gates the **sim-step flag** `g_simTicking` (@0x4C631C = app+0x30C):
     | speed | flag set when | resulting interval |
     |---|---|---|
     | 1 paused | `phase == 0` | (irrelevant; idle code refuses to simulate while paused) |
     | 2 turtle | `phase & 3 == 0` | every 4 timer ticks = **800 ms** |
     | 3 llama | `phase & 1 == 0` | every 2 = **400 ms** |
     | 4 cheetah | every tick | **200 ms** |
     | 5 swallow | every tick; flag is *not* cleared after a step | as fast as the idle loop runs |
   - If `g_mode (@0x4C9444) == 2` and speed ≠ 5, every second would-be step is dropped (parity counter @0x4E53AC) → half rate (probably the disaster mode; HYPOTHESIS).
3. **Consumption** — the app step machine's in-game branch (0x405A70, `default:` case): if speed ≠ paused and `g_simTicking`: run `SimulationTick` (0x412FE0, via thunk 0x401xxx) then `DisasterTick` (0x45C870) and clear `g_simTicking` unless speed == swallow; matches sc2kfix's description of the 1996 code.
4. **Calendar / phases** — `SimulationTick` (VA 0x412FE0):
   - `day = ++g_dayCounter` (int32 @0x4C9E1C); `month = (day/25) % 12` (@0x4CA034); `season = ((month+1)%12)/3` (@0x4CA400; Dec–Feb = 0 …); `year = day/300` (@0x4CA038, years since start).
   - **25 sim ticks per month, 300 per year.** Autosave check at month 0, tick 0 when `year % g_autosaveYears == 0` (@0x4C60E8; calls 0x42FA50 — role HYPOTHESIS).
   - Per-tick work chosen by `day % 25`:

| phase | call(s) | likely role |
|---|---|---|
| 0 | 0x40CED0, 0x4736C0, 0x473980; reset 8 counters at `*(0x4C9440)`; (seasonal event in months 3/7 when 0x4CA3E8≠0) | start-of-month bookkeeping |
| 1 | 0x4671F0 | scan (UNKNOWN) |
| 2 | 0x46A1C0 | scan (UNKNOWN) |
| 3–18 | 0x435220(a,b) for (a,b) = (0,0)…(3,3) | 16 sector/quadrant passes over the map (HYPOTHESIS) |
| 19 | 0x458040 | UNKNOWN |
| 20 | 0x463EE0 | UNKNOWN |
| 21 | 0x467AE0, 0x4686C0, 0x413910 | UNKNOWN |
| 22 | scenario/goal & game-over checks (bankruptcy when funds `@0x4C945C < -100000` → 0x414470(2)) | STRONG INFERENCE (funds threshold, goal-compare chain) |
| 23 | 0x4143F0, 0x414410, 0x414430 | UNKNOWN |
| 24 | 0x40BEA0, 0x4143D0, 0x414450, 0x4720A0 | month-end (UNKNOWN) |

So one timer tick of the original is one **in-game day**, and the simulation is a fixed 25-phase cycle per month; the whole map is processed incrementally across phases.

## Native implementation
`src/simulation/clock.{h,cpp}`: `Clock` with `speed`, `advance(elapsed_ms)` using a 200 ms base tick and the phase gating above (swallow = run all pending steps each frame), `Calendar` with `day/month/year/season` (int32 day counter, signed division semantics preserved), `phaseOfMonth = day % 25`. Rendering/sim separation: sim steps are driven only by this accumulator.

## Open questions
Exact meaning of `g_mode == 2`; initial `g_dayCounter` for new cities and its link to `MISC` (start year); what each phase does (next investigation: decompile 0x4671F0, 0x46A1C0, 0x435220, 0x458040, 0x463EE0 and census their global reads/writes → map layers).
