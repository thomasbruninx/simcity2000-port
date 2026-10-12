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
