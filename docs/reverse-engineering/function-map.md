# Function map

Columns per project spec. **No function has been identified by us yet** (no disassembly analysed in session 1). Rows below are *leads imported from sc2kfix* for the 1995 build `.022`, VA = RVA + 0x400000; they are **UNVERIFIED** until we examine the code. The 1995 header only names a handful.

| VA | Proposed name | Purpose | Evidence | Confidence | Native equivalent |
|---|---|---|---|---|---|
| 0x0048DF90 | `entry` (CRT startup) | Process entry | PE header; SEH-prologue bytes `64 A1 00 00 00 00 55 8B` | STRONG INFERENCE | `main()` |
| 0x0040103C | `MainFrame_ToggleToolBars` (thunk) | Show/hide toolbars | sc2kfix 1995 header; location is a `jmp` thunk (verified bytes) | HYPOTHESIS | ui |
| 0x004014F1 | `SimcityApp_ExitRequester` (thunk) | Exit confirmation | sc2kfix | HYPOTHESIS | app |
| 0x00401587 | `SimcityApp_SaveCity` (thunk) | Save city | sc2kfix | HYPOTHESIS | save |
| 0x004022C0 | `UpdateSectionsAndResetWindowMenu` (thunk) | – | sc2kfix | HYPOTHESIS | – |
| 0x004026D0 | `SimcityApp_PointerToCSimcityViewClass` (thunk) | Get view | sc2kfix | HYPOTHESIS | – |
| 0x004029C8 | `SwapCycle` (thunk) | Palette cycle table swap | sc2kfix; used in its animation hook | HYPOTHESIS | renderer palette cycling |
| 0x00402405 | `ToggleColorCycling` (thunk) | Palette-animation toggle | sc2kfix hook site | HYPOTHESIS | renderer |
| 0x004A1700 | `CmdTarget_OnCmdMsg` | MFC | sc2kfix | HYPOTHESIS | – (MFC) |
| 0x004BF624 | `AfxGetThreadState` | MFC | sc2kfix | HYPOTHESIS | – (MFC) |

Globals (1995): `pCSimcityAppThis` 0x4C6010 (.bss), `pPalAnimMain` 0x4C77F0, `pPalOnCycle` 0x4C7BF0, `pPalOffCycle` 0x4C80C0, `bCSimcityDocSC2InUse` 0x4E8734, `bCSimcityDocSCNInUse` 0x4E8738, `bLoColor` 0x4E903C — all sc2kfix, HYPOTHESIS.

**1996-SE leads worth locating by signature in our binary** (names only; do not trust addresses): `RandomWordLCGMod`, `RandomWordLFSRMod`, `SeedRandomLCG`, `SeedRandomLFSR`, `RandomWordLFSRMod4`, `RandomWordLFSRMod128`, `Engine_SimulationProcessTick`, `SimulationPollutionTerrainAndLandValueScan`, `SimulationProvisionMicrosim`, `SimulationEQ_LE_Processing`, `LoadTilesFromMemory`, `LoadNamedEntryFromRsrcOffset`, `SimcityDoc_PrepareMap`. sc2kfix naming suggests **two RNGs (an LCG and an LFSR)** — HYPOTHESIS, to be examined for Phase 16.

## Thunk targets resolved by Ghidra (session 1)

Ghidra 12.1.3 headless auto-analysis found **3,678 functions** (878 with non-default names — mostly imports/thunks; ≈2,800 `FUN_*`). The `jmp` thunks named by sc2kfix resolve to these real implementations (target addresses VERIFIED from the `jmp rel32`; the *names* remain sc2kfix's, HYPOTHESIS):

| Thunk VA | sc2kfix name | Implementation VA |
|---|---|---|
| 0x0040103C | `MainFrame_ToggleToolBars` | 0x0040B4C0 |
| 0x004014F1 | `SimcityApp_ExitRequester` | 0x004063C0 |
| 0x00401587 | `SimcityApp_SaveCity` | 0x0042F4E0 |
| 0x004022C0 | `UpdateSectionsAndResetWindowMenu` | 0x0042E8C0 |
| 0x004026D0 | `SimcityApp_PointerToCSimcityViewClass` | 0x00407220 |
| 0x004029C8 | `SwapCycle` | 0x00456B70 |
| 0x00402405 | `ToggleColorCycling` | 0x00456A60 |
| 0x00401749 | `SimcityApp_OnQuit` | 0x00406350 |

`0x405813` and `0x42E737` (sc2kfix patch sites) fall inside functions, as expected for mid-function patches. Function export: `tools/ghidra/ExportFunctions.java`.

## Functions identified by our own analysis (session 2, build .022, VA)

Names are ours (descriptive, provisional). Evidence = Ghidra decompilation read directly; calling conventions/params partly guessed.

| VA | Proposed name | Purpose | Evidence | Confidence | Native |
|---|---|---|---|---|---|
| 0x004732A0 | `InitGlobalObjects` (static init) | Constructs global CString objects incl. archive names `large.dat`/`smallmed.dat`/`special.dat` at 0x4C9178/84/90 | xrefs from the three strings | VERIFIED | – |
| 0x00434C6F | `LoadSpriteArchive(index)` | Reads `<PATHS>\<archive>` index (BE u16 count, 10-byte entries), fills sprite table, loads raw sprite streams | decompile: reads u16, bswap, ×10-byte entries, per-id 8-byte table | STRONG INFERENCE | `sprite::load` |
| 0x004875D0 | `SetSpriteData(id,h,w,size,src)` | Copies stream into table slot `{ptr,h,w}` | decompile | VERIFIED | – |
| 0x00435100 | `SetSpriteForDrawing` | Computes clip rect (client ∩ rect) and stores draw state (name from sc2kfix, behaviour verified) | decompile | VERIFIED | renderer |
| 0x00485780 | `StoreDrawState` | Stores target bits, table, pitch, x/y, clip into globals 0x4CCE48–0x4CCE64 | decompile | VERIFIED | – |
| 0x004857F0 | `DrawSprite(id,x,y,flipFlag,mask)` | Bounds-rejects and dispatches to blit variant | decompile | VERIFIED | renderer |
| 0x00485CA0 | `BlitSpriteUnclipped` | Executes `[arg][op]` stream (ops 0,1,3,4, other=end) | decompile + decoder validates on 1,455 sprites | VERIFIED | `sprite::decode` |
| 0x0040BF80 | `StartSimTimer(period,resolution)` | `timeBeginPeriod`, `timeSetEvent` (periodic, callback thunk 0x402AA9); fallback `SetTimer`; else beep + message "Unable to start the timer…" | decompile; object fields +0xE4 running, +0xE8 timer id, +0xEC period, +0xF0 resolution | VERIFIED | sim clock |
| 0x0040CA90 | `SimTimerCallback` (mm-timer proc, via thunk 0x402AA9) | Calls `timeGetTime` (result unused), then `PostMessageA(hwnd=dwUser, WM_TIMER=0x113, wParam=1, lParam=0)` | disassembly | VERIFIED | sim clock tick source |
| 0x0040CAC0 | `SimTimerProcFallback` (via thunk 0x401942) | Same `PostMessage(WM_TIMER,1,0)` for the `SetTimer` fallback | disassembly | VERIFIED | – |
| 0x00409DFC | call site of `StartSimTimer` | Only direct call: `StartSimTimer(200, 200)` — period 0xC8 ms, resolution 0xC8 — from a constructor-like routine (`this`=ESI; app object 0x4C6010 registered right after) | disassembly | VERIFIED call; role of this routine UNKNOWN; speed-dependent restarts not found (no other direct callers; may be indirect) | sim clock |
| 0x00405A70 | `AppStepMachine` (per-idle/timer state machine on app object) | Switch on `this+0x324` "program step": step 1 `presents.bmp` shown ≥5000 ms; 3 `titlescr.bmp` ≈5001 ms; 5 another ≈5001 ms screen; 7 → main menu step 0xE → New City/Load/etc.; default = in-game; handles palette-cycle toggles (+0x314/+0x318 → `ToggleColorCycling` 0x456A60) | decompile; uses `GetTickCount` | STRONG INFERENCE | app/state flow |
| 0x0047FED0 | `SeedLCG` (thunk 0x401F37) | `state@0x4CCB98 = arg` | disasm | VERIFIED | `Lcg::seed` |
| 0x0047FEF0 | `LCGMod(n)` (thunk 0x401163) | LCG `*0x41C64E6D+0x3039`, `(s>>16)&0x7FFF`, 16-bit `IDIV` remainder | disasm; 19 callers | VERIFIED | `Lcg::mod` |
| 0x0047FF20 | `SeedLFSR` (thunk 0x40265D) | dword store to 0x4CCB94 | disasm | VERIFIED | `Lfsr::seed` |
| 0x0047FF40 | `LFSRMod(n)` (thunk 0x401CA8) | 16-bit LFSR step (`<<1`, `^0x1BF5` if top bit), `% n` | disasm; 33 callers | VERIFIED | `Lfsr::mod` |
| 0x0047FF80,FFC0,480040,4800C0,480100 | `LFSRMask1/3/F/3F/7F` | step then `& mask` | disasm | VERIFIED | `Lfsr::mask` |
| 0x00424C30 | `AppInitDisplayAndSeed` | Display capability probe (GetDeviceCaps, OS version check), then seeds both RNGs with `GetTickCount()\|1` | decompile | VERIFIED | app init |
| 0x0040BBB0 / 0x0040BBD0 | `OnTimer(id)` / `SimPace` | id==1 → phase counter, anim flags, speed-gated `g_simTicking` | decompile; matched against sc2kfix flag model | VERIFIED | `Clock::onTimerTick` |
| 0x00412FE0 | `SimulationTick` | `++day`, month=(day/25)%12, year=day/300, `switch(day%25)` phase dispatch (see simulation/clock-and-calendar.md) | decompile | VERIFIED structure; per-phase roles UNKNOWN | `simulation/` |
| 0x0045C870 | `DisasterTick` | Dispatch on pending disaster type `@0x4C9438` (1–0x12) to handlers 0x45E290, 0x45D460, 0x45DF50, … | decompile | STRONG INFERENCE | later |
| 0x004671F0 | `PowerSupplyScan` | clear flags, flood power from plants (ids 0xC6–0xCF), % powered | decompile | STRONG | later |
| 0x00467340 | `PowerFloodFrom(x,y)` | called by the scan; not yet read | – | HYPOTHESIS | later |
| 0x00463EE0 | `WaterSupplyScan` | 4 rotation-dependent iteration orders, flood water (0x464370) | decompile | STRONG | later |
| 0x00458040 | `DecayTraffic` | `v -= v>>2` on 64×64 | decompile | VERIFIED | later |
| 0x0046A1C0 | `EnvironmentScan` | pollution, land value, pop density, growth rate, crime, coverage | decompile | STRONG | later |
| 0x00435220 | `ZoneTilePass(a,b)` | per-sector tile growth/decay pass | decompile | STRONG | later |
| 0x00467AE0 | `DemandAndEvents` | RCI demand (x87), random events/messages | decompile | STRONG | later |
| 0x004686C0 | `EconomyModel` | national/economic variables | partial read | MODERATE | later |
| 0x00413910 | `RecordGraphHistory` | 16×52 int32 shift registers | decompile + XGRP check | VERIFIED | simple |
| 0x00473980 | `MonthlyFinance` | budget roll-up, treasury, maintenance from tile census, random ordinance | decompile | STRONG | later |
| 0x004736C0 | `ShowBudgetDialog` | annual budget dialog | decompile | MODERATE | UI |
| 0x0040CED0 | `UpdateTitleText` | title bar date/status | decompile | STRONG | UI |
| 0x004720A0 | `PickAdvisorNeed` | advisor message from stats | partial read | MODERATE | UI |
| 0x0048BCD8 / 0x0048BCC8 | CRT `rand` / `srand` | MSVC LCG, 335 callers / 8 callers | decompile | VERIFIED | `CrtRand` |
| 0x00430CB0 | `LoadCityMisc` (city load, MISC → globals) | Checks version 0x122, byte-swaps, copies ~1050 MISC words into globals (docs/formats/misc-fields.md) | decompile; effects verified (rotation, wind, cloud, water factor, ordinances) | VERIFIED | `CityFile` + `misc::k*` |
| 0x00467340 | `PowerFlood(a,b)` | two-pass FIFO flood (see simulation/power.cpp) | decompile + port verified on 61 saves | VERIFIED | `floodPower` |
| 0x00464370 | `WaterFlood(a,b)` | two-pass flood over piped tiles (pump 0xDC, tower 0xEB, treatment 0xF4, desal 0xFA) | decompile + port verified | VERIFIED | `floodWater` |
| 0x00413780 / 0x004137B0 / 0x00413820 | `QueueReset+Push` / `QueuePush` / `QueuePop` | 512-entry FIFO ring of (a,b) ints; overflow drops oldest | decompile | VERIFIED | `TileQueue` |
| 0x0046B420 | `SpreadCoverage(arr,x,y,strength)` | adds strength to a cell and weighted neighbours via 0x46B3B0 (4/5, 3/4·4/5, …) | partial | MODERATE | later |
| 0x0046A1C0 | `EnvironmentScan` | pollution/land value/density/coverage/crime (8 stages) | decompile + port validated (docs/validation/004) | VERIFIED structure | `runEnvironmentScan` |
| 0x0046B420 / 0x0046B3B0 | `SpreadCoverage` / `AddSaturating` | footprint of 41 cells with decaying weights; saturating add into 32×32 layer | decompile; coverage layers reproduce exactly | VERIFIED | `spread`/`addCell` |
| 0x00467AE0 (first half) | `DemandValves` | RCI valve computation (x87 doubles, caps from census, tax table, ±2000 clamp) | disassembly; model cross-check; census/pop fields exact on saves | VERIFIED reading | `runDemand` |
| 0x00473980 | `MonthlyFinance` | roll, January settlement, maintenance units, random ordinance | decompile; maintenance exact on 6 saves | VERIFIED | `runMonthlyFinance` |
| 0x0042B590 | `OrdinanceCost(i)` | switch of 20 ordinance yearly costs | decompile; ordinance sum exact on saves | VERIFIED | `ordinanceCost` |
| 0x0048C38C | `_ftol` | x87 truncating double→int | demand code | STRONG | `static_cast<int>` |
| 0x00435220 | `ZoneTilePass(a0,b0)` | zone/tile update pass (zones 0..6 ported; 7–9 pending) | decompile; **bit-identical in emulator on 61 cities** | VERIFIED | `runZonePass` |
| 0x00456F70 | `RouteTraffic(a,b,zone,kind,maxSteps)` | DFS traffic router, adds traffic to XTRF along the found path | decompile; emulator-identical | VERIFIED | `routeTraffic` |
| 0x004418B0 | `SetTile(a,b,id)` | XBLD write with tile census / military counters | emulator-identical | VERIFIED | `setTile` |
| 0x00436800 / 0x004376C0 / 0x00436F40 / 0x00436C90 / 0x00437AE0 / 0x00437910 | `StampBuilding` / `CollapseBuilding` / `GrowBuilding` / `ClearLotForArcology` / `CanBuildAt` / `DemolishSmall` | building placement primitives | emulator-identical | VERIFIED | `tile_ops.cpp` |
| 0x00456E80 / 0x00436680 / 0x00441D20 | `HasRoadNearby` / `PoweredNearby` / `SetUnderground` | helpers | emulator-identical | VERIFIED | `tile_ops.cpp` |
| 0x00456CE0 | `InitNeighbourOffsets` | fills the 24-pair offset table at 0x4C82D8 | decompile | VERIFIED | constants |
| 0x00458690 | `DestroyTileEffect(this,a,b,mode)` (thiscall, `ret 0xC`) | collapse/fire/explosion effects (761 lines) | not yet read | — | hook |
| 0x00437C30 | `PlaceMilitaryOrSpecial(a,b,id,zone)` | military base building placement (428 lines) | not yet read | — | pending |
| 0x00452500 / 0x00452190 / 0x00452EF0 / 0x00452CE0 / 0x00452660 | thing spawners (ships, planes, trains, ...) | write 12-byte records into the things array | read | STRONG | hooks |
