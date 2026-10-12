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
