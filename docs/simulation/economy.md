# Economy / job-market model (0x4686C0, phase 21)

Port: `src/simulation/economy.cpp` (`runEconomy`). State lives in `CityState` (MISC words noted below). Validation: `sc2k-emu-diff economy`,
61 cities × 6 variants × 40 consecutive months, every field compared after every month — identical (see validation/006).
Arithmetic: the original uses x87 doubles at 53-bit precision (Win32 default control word 0x27F); IEEE doubles in the same operation order are exact.
`__ftol` truncates toward zero (low 32 bits used).

## What it computes (STRONG: structure verified by the oracle; the *names* are interpretations)
1. **National population** `nationalPop` (MISC 20) and **GNP** `gnp` (MISC 21) drift by `era-factor × value / 1200` per month (era = MISC 23, table 0x4E8548), pulled
   toward soft caps 5,000,000 / 3,500,000 (above the cap the drift is subtracted).
2. With probability 1/10 the *rate level* (MISC 22, "federal rate") moves up/down depending on `100 × gnp / (nationalPop + 1)` (messages 9/10), and the era index
   (0–3, message 7) is re-derived from the same ratio (thresholds 45 / 60 / 75).
3. **Four neighbouring cities** (MISC 438..453: population-like `nbr9d28`, GNP-like `nbr981c`) evolve with the same recipe; every 64th month on average one of them is cut by 25 % / 50 %.
4. **Eleven sectors** (MISC 91..123: `sec940c` activity, `sec3f8` demand offset, `sec408` workers): activity moves toward a year-interpolated target
   (tables 0x4E8448 rows 50 years apart, 0x4E84F8 after the last row) with noise from four LFSR(&0x7F) draws; ordinance 0x80000, population growth, and the education
   score `cb4cc` scale individual sectors by 0.9 / 1.1 / 1.2 / 0.8 (note: the multiplications are applied to fixed slots on **every** loop iteration and therefore compound — preserved).
   Worker counts are then scaled toward the real industrial population (`catPop[2]`) with a probabilistic rounding step.
   Outputs `c8440` (pollution divisor term) and `c9200` (demand term) are derived from the sector shares.
5. **Job market** (MISC 31..90, 20 interleaved triples `jobsA/B/C` = headcount, wage-like sum, education-like sum for 20 education/age classes): classes are promoted up the
   ladder (school/college/health funding and ordinances 0x20/0x40/0x100/0x200/0x400 shape the throughput), immigration and unemployment from the demand pass
   (`populationGrowth`/`populationDecline`) are distributed over the classes, the aggregate **education score** `cb4cc`, **health score** `ca428` and **employment** `ca478` are produced.

## Details worth knowing
* A `rateLevel` of 0 makes the original divide by zero (it faults in the emulator); real games keep it ≥ 1. The port skips the test instead of crashing.
* All divisions follow the original's signedness (unsigned `DIV` for job counts, signed `IDIV` for percentages).
* Stage markers (`g_econStopStage`) exist only for the differential test tool.
