# Reverse-engineering journal

## 2026-10-12 — Session 1: reconnaissance

**Examined:** all 168 files of `SC2K/`; `SIMCITY.EXE`, `WINSCURK.EXE`, `SCURKUSA.DLL`, `D2HTOOLS.DLL` headers/imports/resources/strings; the sc2kfix source tree (README, `dllmain.cpp`, `include/sc2k/sc2k_1995.h`, parts of `sc2k_1996.h`, `hook_sc2k1995_miscellaneous.cpp`, `hook_animation.cpp`, `registry_config.cpp`, `save_sc2x.cpp` headers).
**Addresses examined:** VA 0x40103C, 0x4014F1, 0x4029C8, 0x402405, 0x401749 (thunk jmps); 0x4A1700; IAT 0x4EE79C–0x4EE7AC; entry 0x48DF90 (bytes only). No code interpreted.

**Discoveries**
- Supplied game = 1995 CD Collection, build .022, PE timestamp `0x302FEA8A` (matches sc2kfix's 1995 target). MFC statically linked, GDI + MCI + `sndPlaySound`, no DirectX.
- sc2kfix's 1995 addresses verified against our import table (5/5 IAT slots) → their 1995 header is usable as leads.
- Save format container and RLE **verified**; decoded chunk sizes identical across all 61 files; struct sizes for microsim (8) and thing (12) match sc2kfix exactly.
- `LARGE/SMALLMED/SPECIAL.DAT` index structure decoded; pixel encoding unknown.
- `TILES.DB` is only used by SCURK.
- Ghidra 12.1.3 headless import + auto-analysis of SIMCITY.EXE succeeds.

**Hypotheses**: two RNGs (LCG + LFSR); sim tick driven by `timeSetEvent`; MIDI via MCI sequencer; palette cycling ranges 171–219 & 224–239. All unverified.
**Disproven**: initial assumption that SC2 chunks are word-padded (they are not).
**Remaining questions**: sprite pixel format; `MISC` layout; sim timer period; RNG algorithms; exact music playlist; `SCURKUSA.DLL` timestamp anomaly; type-240 resource record layout.
**Next**: (1) Ghidra project with sc2kfix leads applied as *labels flagged "unverified"*; (2) trace `large.dat` string xref → sprite decoder; (3) find `timeSetEvent` call → sim tick; (4) find RNG via sc2kfix names; (5) `MISC` field map from load routine.

## 2026-10-12 — Session 2: sprite format, timer

**Setup:** MIT LICENSE added. Persistent Ghidra project in `reverse/ghidra` (gitignored). New scripts `tools/ghidra/DecompileAt.java`, `XrefsTo.java`. Python `capstone` install failed (Python 3.14 pip cannot parse macOS 26 version string) — not needed, Ghidra disassembles/decompiles; Dear ImGui deferred to M4 (FetchContent).
**Examined:** xrefs for `large.dat`, `\%d.MID`, `sequencer`, timer message; functions 0x4732A0, 0x434C6F, 0x4875D0, 0x435100, 0x485780, 0x4857F0, 0x485CA0, 0x40BF80.
**Discoveries:** sprite stream = `[arg][op]` records (0 nop, 1 new row, 3 skip, 4 literal copy padded even, else end); header order is height,width. Decoder validated on all 1,455 sprites, rendered with PAL_MSTR palette — correct. Sim timer start routine located; period is a parameter (caller not yet traced).
**Disproven:** earlier doc had width/height swapped; "pixel format unknown" resolved.
**Remaining:** callers of 0x40BF80 (actual period / speed table); mirror and mask blit variants; tile id → sprite id mapping; RNG; MCI music player at 0x480CEB (xref to `sequencer`) and `\%d.MID` at 0x480BCE.
**Next:** (1) trace 0x40BF80 callers → speeds; (2) decompile 0x480BCE/0x480CEB → MIDI playlist; (3) find isometric projection in the map draw code; (4) RNG.

### Session 2 addendum — sim clock
- mm-timer callback (0x40CA90) only does `PostMessage(hwnd, WM_TIMER, 1, 0)`; so ticks arrive as window messages (VERIFIED). Matches sc2kfix's flag model: `SimulationTicking` is set by the timer path and cleared after each tick except at the fastest speed ("African Swallow"), speeds enum 1=paused,2=turtle,3=llama,4=cheetah,5=swallow (sc2kfix, UNVERIFIED in our build; in 1996 code).
- Only direct `StartSimTimer` call uses 200 ms / 200 ms (VERIFIED call). The mapping speed → effective ticks/sec is therefore NOT a timer period but must be in the WM_TIMER handler (still to find). **HYPOTHESIS:** base tick 200 ms, speed gates how often the sim runs per tick.
- App state machine 0x405A70 documents intro timing (presents 5 s, title ≈5 s).
- Ghidra auto-analysis misses some code (e.g. 0x40CA90, 0x409DC0 not in functions; timeKillEvent xrefs empty). Use `tools/ghidra/Disasm.java` for forced disassembly, and consider a re-analysis pass with code-scan options.
- **Next:** find WM_TIMER handler (message map entry for 0x113 in CSimcityApp/MainFrame), speed variable at app+? and its consumers; MIDI player 0x480BCE/0x480CEB; RNG.

## 2026-10-12 — Session 3: RNG and clock
**Found/verified:** both RNGs (LCG@0x4CCB98, 16-bit LFSR@0x4CCB94, poly 0x1BF5, seeded from `GetTickCount()|1` at 0x424CE5/0x424CF6; RNG state not in saves per decoded chunks); LFSR period from seed 1 is 57,337 (computed from the model; not maximal; low bits correlated — preserved). Located `WM_TIMER` handlers via MFC message-map scan (`{0x113,0,0,0,sig 11,fn}` ×7 in .rdata); sim pace handler 0x40BBD0; speed gating table (turtle 800 ms, llama 400 ms, cheetah 200 ms, swallow free-run); `SimulationTick` 0x412FE0: 25 ticks/month, 300/year, 25-phase dispatch; disaster dispatcher 0x45C870.
**Code:** first native code — `src/simulation/{rng,clock}`, CMake, 14 tests (known-answer vectors from `tools/rng_model.py`, an independent model of the disassembly). Tests prove self-consistency with *our reading*, not yet with the live game.
**Hypotheses:** mode 2 = disaster (half rate); phases 3–18 = 16 map sector passes; phase 22 = scenario/bankruptcy checks.
**Disproven:** I first took 0x45C870 for the main sim orchestrator — it is the disaster dispatcher.
**Tooling:** `DecompileAt.java` now creates functions on demand; `DecompileSmall.java` decompiles all small functions for constant searches (found RNG by multiplier constants).
**Next:** census of RNG call sites by caller; decompile phases (0x4671F0, 0x46A1C0, 0x435220, 0x458040, 0x463EE0, 0x467AE0, 0x4686C0) to map each to map layers; validation experiment (Windows): speed vs. date progression per wall-clock to confirm 800/400/200 ms and 25 days/month.

## 2026-10-12 — Session 4: decompilation of the per-tick phases
**Read:** 0x4671F0, 0x46A1C0, 0x435220, 0x458040, 0x463EE0, 0x467AE0, 0x4686C0 (first 260 lines), 0x413910, 0x40CED0, 0x4736C0, 0x473980, 0x4720A0 (head). Full outputs in `reverse/out/ph.txt` (not in git).
**Discoveries:** layer storage = row-pointer tables; mapped runtime arrays to XBLD/XZON/XBIT/XTXT/XUND, traffic/pollution/value/crime, density/growth/police/fire (docs/simulation/map-layers-and-globals.md); phase-to-function table (monthly-phases.md); budget record layout and category order verified by offsets (matches sc2kfix `budget_t`); XGRP = 16×52 int32 BE graph history (verified against 3 saves); ALTM u16 BE bitfields verified; **MSVC `rand()` is the main RNG (335 callers, shared state, srand from 8 UI/gen sites)** — corrected rng.md and added `CrtRand` (tests pass).
**Hypotheses:** zone numbers 7–9; coverage-ordinance bits; unknown layer 0x4C8F70; divisor table 0x4E8EA0.
**Disproven:** "CRT rand unused by game logic".
**Remaining:** read 0x435220 callees (0x436680, 0x456F70, 0x436800, 0x4376C0, 0x436F40, 0x4418B0), the flood fills (0x467340, 0x464370, 0x46B420), tables (0x4E8318, 0x4E8528, 0x4E8638, 0x4E85A8, 0x4E6468, 0x4E6428, 0x4E64D8, 0x4E6580), constants 0x4D9CE8–0x4D9D10; second half of 0x4686C0 and 0x4720A0; `MISC` field map (budget/ordinance/date fields live in MISC).
**Next:** implement `libsc2` (container + layers) and a verification harness that recomputes power/water/pollution/land value from a saved city's XBLD/XZON/XBIT and compares to the saved layers.

## 2026-10-12 — Session 5: libsc2 and first equivalence tests
**Built:** `src/save` (RLE, CityFile, misc indices), `sc2k-save-info`, `src/world`, `src/simulation/{tile_queue,power,water}`, `tools/sc2k_verify.cpp`, tests (32 pass; data-dependent ones need `SC2K_DATA_PATH`).
**Found:** the city loader 0x430CB0 → full MISC field map; queue discipline (FIFO ring, 512); power and water flood algorithms and the outer scans (water: 4 rotation orders).
**Validated against the 61 saved files:** power 5 mismatching tiles of ~284k; water 4,327 of 284,009 with 41/61 files exact (remaining diffs all pipe tiles, likely stale-after-edit saves — HYPOTHESIS). New quirks confirmed in tests: plant tiles are charged one supply unit in pass 2; in shortage, flood order decides who is served.
**Encoder note:** canonical RLE encoder reproduces 55% of original chunks bytewise (min run 2); original encoder not yet reverse-engineered — `serializeOriginal` keeps untouched chunks verbatim.
**Next:** environment scan (0x46A1C0) port + verification against XPLT/XVAL/XCRM/XPOP/XROG/XPLC/XFIR; needs tables 0x4E85A8, 0x4E8638, 0x4E6468, coverage spreader 0x46B3B0, city centre; then demand/finance. Also: the Windows check of TOMG-C4 water (docs/validation/003).

## 2026-10-12 — Session 6: environment scan, demand, finance
**Ported + validated:** environment scan (all 8 stages; XPOP/XPLC/XFIR exact on phase-2 saves, city centre exact in 54/61, marks exact in 39/43 eligible saves; layer 0x4C8F70 = XTER), PE table loader (`GameTables::fromExe`, no game data in repo), finance (maintenance exact on 6/6 phase-0 saves), demand valves (population/cost fields exact on 7/7 phase≥21 saves; numerics vs independent model).
**Key discoveries:** (1) **save phase matters** — `MISC day % 25` tells which scans a save contains; this refines validations 003/004; (2) `DAT_004cb790` is rows 32–63 of the 0x4CB710 scratch array (no unbounded accumulation); (3) marks 0x08 only exist in the top-left 64×64 quadrant of XBIT (quirk); (4) two conventions for MISC word 29 across builds; (5) the sample files were saved by more than one game build.
**Anomalies:** CENTERVL saved coverage all zero; TOMG-C4 / FOURCITI water at phase 20; XVAL/XPLT/XCRM only approximately reproducible (need previous month state).
**Tooling:** `tools/demand_model.py` (independent model), `sc2k-verify env|power|water`.
**Next:** zone-growth pass (0x435220 and callees 0x436680, 0x456F70, 0x436800, 0x4376C0, 0x436F40, 0x4418B0) — the largest remaining piece and the RNG consumer; economy model 0x4686C0; second half of 0x467AE0 (events); then the window/UI-free "headless sim driver" that runs SimulationTick (all 25 phases) on a loaded city.

## 2026-10-12 — Session 7: emulator oracle; zone pass reconnaissance
**Approved by owner:** install Unicorn (Homebrew `unicorn` 2.1.4, GPL-2.0, dev-only). Python packaging is broken on this macOS (pip/truststore cannot parse the OS version) so the harness uses the C API (`tools/emu/`, CMake option `SC2K_BUILD_EMU`).
**Result:** differential tests (docs/validation/006) show power, water, environment, demand and finance ports are **bit-identical to the original machine code** on all 61 sample cities (+ perturbed variants). This also settles why saved flags only approximately matched.
**Zone pass recon (0x435220):** 0x456F70 is the **traffic router** (DFS over roads/rails/highways/subways; adds traffic to XTRF along the found path; called as the commute test by the growth code) — a major subsystem on its own; 0x4418B0 = `SetTile` (census update; military tiles use per-type counters MISC 1002..1017); 0x456E80 = road-adjacency test (24-offset table @0x4C82D8 filled at runtime); 0x436680 = powered-or-adjacent-powered; 0x436800/0x4376C0/0x436F40 = zone/building placement (tables @0x4E652E/0x4E6556 base ids and ranges); 0x441D20 = underground-layer setter with subway counter MISC 1018 (so XUND = subway layer). Callees still unread: 0x458690 (761 lines), 0x437C30 (428), 0x437AE0, 0x437910, 0x452xxx special-building handlers, 0x47BA90 (message).
**Next:** port the zone pass in layers using the emulator as oracle: SetTile/census → growth decision (zones 1–6) → traffic router → infrastructure decay → special buildings; each layer diff-tested against 0x435220 run in the emulator.
