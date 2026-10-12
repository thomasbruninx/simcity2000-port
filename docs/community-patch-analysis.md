# Community patch analysis — sc2kfix

Source: `sc2kfix-main/` (MIT licence, © OpenCity Foundation; no `.git` dir present in the copy — upstream revision unknown). Provenance of everything here: direct reading of the supplied source (VERIFIED about *what the code says*; claims about game behaviour are the authors', not independently verified by us unless noted).

## What it is

Not a binary diff and not a modified `SIMCITY.EXE`. It is a **runtime injection DLL named `winmm.dll`** (proxy for the real WinMM; `hooks/exports.def` forwards ~all WinMM exports) placed beside the exe. On load it:
1. identifies the host build from the PE **timestamp** (`dllmain.cpp`: `0x313E706E` = 1996 SE, **`0x302FEA8A` = 1995 CD Collection — our build**, `0x3103B687` demo, `0xBC7B1F0E`/`0x6C261F75` = SCURK builds);
2. patches game code in memory (`SafeVirtualProtect` + `NEWJMP/NEWCALL` writing `E9`/`E8` rel32; IAT-slot overwrites; single-byte patches) at hard-coded addresses;
3. adds its own features (settings dialog, console, Lua, FluidSynth/SDL3 sound engine, SC2X save format, mods).

Size: ~45k lines C++ in `dllmain.cpp`, `utility.cpp`, `hooks/` (22 files), `modules/` (30 files); third party: FluidSynth, SDL3, libsndfile, Lua, miniz, fmt. Windows-only (MSVC project), GitHub Actions builds. The README mentions `hooks/hooks.md`, which is **not present** in the supplied copy.

## Why it matters to us

- It is the largest available body of **named function/global addresses and structure layouts** for SC2K: `include/sc2k/sc2k_1996.h` (≈363 `GAMECALL`s, ≈389 `GAMEOFF` globals, 5.3k lines) and `sc2k_classes.h` (MFC class layouts). Examples: `Engine_SimulationProcessTick`, `SimulationPollutionTerrainAndLandValueScan`, `SimulationProvisionMicrosim`, `RandomWordLCGMod`, `RandomWordLFSRMod`, `SeedRandomLCG`, `SeedRandomLFSR`, `LoadTilesFromMemory`, `PlaceRoadAtCoordinates`. These are the **names of 1996-SE functions**; addresses do not transfer to our 1995 binary. They are *leads*, to be located by signature/xref and then verified.
- `include/sc2k/sc2k_1995.h` + `hooks/hook_sc2k1995_miscellaneous.cpp` + `hook_animation.cpp` give a **small verified-address set for our exact build** (see [executable-analysis.md](executable-analysis.md) — 5/5 IAT slots matched).
- `modules/save_vanilla.cpp` (1.9k lines) is a reimplementation of vanilla load/save — a secondary reference for `MISC` layout and chunk semantics.
- `modules/military.cpp`, `hook_tilegrowthorplacement.cpp`, `hook_terrainhandling.cpp`, `hook_citymanagement.cpp` etc. contain reimplemented simulation pieces (RNG, growth, budget) — secondary references to check against decompiled output, **not** to be copied.

## Findings by fix (per project instructions: classify, say whether to incorporate)

Applicability to build .022 is UNKNOWN for everything not in the 1995 hook files. "Native action" = what to do in our reimplementation.

| Fix / feature (README) | Class | Evidence in our build | Native action |
|---|---|---|---|
| Palette-cycling animations frozen on truecolour displays | bug fix / compat | 1995 hook exists (`Hook_ToggleColorCycling_SC2K1995` at 0x402405; cycles `AnimatePalette` ranges 171–219 and 224–239; swaps tables at 0x4C7BF0/0x4C80C0) | Implement palette cycling natively (we render truecolour anyway); take ranges after verifying in our exe |
| Registry → portable settings | compat | 5 registry IAT hooks for 1995 | Not needed (own prefs) |
| Load-city dialog cancel crash / dialog corruption | bug fix | `Hook_1995_LoadCityCancelFix` @0x42E737 | N/A (new UI), note as pitfall |
| Open/save filename filters (adds `.CTY` SimCity Classic) | quality of life | `Hook_1995_LoadStringA` | Optional: SimCity Classic `.CTY` import (UNKNOWN format here) |
| City/mayor names lost on save/load | bug fix | 1996 only? UNKNOWN for .022 | Verify in our exe; preserve names in our serializer |
| Military bases never grow; retry placement | bug fix (gameplay) | 1996 `military.cpp`; UNKNOWN for .022 | Verify; keep behind "fixed" compat mode |
| Rail/highway neighbour connections broken after load | bug fix | 1996; UNKNOWN | Verify |
| Infrastructure valuation / city centre / bond rating | bug fix (gameplay) | 1996; UNKNOWN | Verify; "fixed" mode |
| Sign font, display bugs, "Add all inventions" debug option | bug fix | 1996; UNKNOWN | Low priority |
| Real-time city rendering, seasons/weather, dark underground, terrain colour variants, floating status widget, hotkeys, Lua, mods, advanced query | new features / QoL | – | Out of scope for fidelity milestone |
| Restored music playlist entries, WillTV / intro cutscenes, cheat codes from other ports | content restoration | needs data we may not have (movies absent from `SC2K/`) | Playlist: determine from exe; cutscenes: skip |
| Replacement sound engine (SDL3 + FluidSynth, MP3, volume) | platform | – | Same architecture as our plan (FluidSynth + SDL3) — independent reimplementation |
| Higher-quality replacement WAVs (`blobs/500,503,508,514,529.wav`) | content | not original assets | Do not import; they are community-made |

Hook sites known for build .022 (all addresses VA, from `hook_sc2k1995_miscellaneous.cpp`; **purposes are the authors', unverified by us**): 0x401749 `SimcityApp_OnQuit`, 0x405813 `SimcityApp_InitInstance` fix, 0x42E737/0x42E746 load-city cancel fix, 0x4A4246 `Wnd::OnCommand`, 0x4A185E `CmdUI::Enable`, 0x402405 `ToggleColorCycling`, plus byte patches at 0x44D213, 0x44D5C2/CF, 0x47643F, 0x488117, 0x4051DD, 0x4E5120, 0x4E6234, 0x4E6314. Each needs disassembly to learn what behaviour the byte patch changes — pending (journal task).

## Licensing / hygiene
MIT: reuse of ideas, names and *facts* is fine; if any code or tables are copied the MIT notice must be retained. Plan: reimplement from our own analysis; credit sc2kfix in `docs/` where its research is the source. The patch's AI-contribution policy (README) is noted — we should not submit upstream, and should be honest that our work uses AI assistance.

## Open questions
Which README fixes reproduce in build .022; whether sc2kfix's 1996 function names can be matched in our binary by byte signature (likely, since the code base is shared — try Ghidra BSim/FunctionID or manual signature search).
