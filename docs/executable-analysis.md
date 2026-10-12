# Executable analysis — `SIMCITY.EXE`

Status: **initial reconnaissance** (headers, imports, resources, strings). No disassembly has been interpreted yet.
Tool: [`tools/pe_inspect.py`](../tools/pe_inspect.py). Every value below is parsed directly from the file (VERIFIED) unless marked.

## Identity

| Field | Value | Confidence |
|---|---|---|
| Size | 1,299,456 bytes | VERIFIED |
| SHA-256 | `c2f735be63bb2e8a6f4c8e9a7080c8754496606038259020fe784ea1b062c33b` | VERIFIED |
| MD5 | `df136923ca204e3451ed4349f08b5d1f` | VERIFIED |
| PE timestamp | `0x302FEA8A` = 1995-08-15 00:30:02 UTC | VERIFIED |
| Version resource | FileDescription "Sim City 2000® Windows 95", ProductVersion "1.00", **PrivateBuild ".022"**, Comments "He tried to kill me with a forklift!", © 1995 Maxis | VERIFIED |
| `VERSION.INF` | "SimCity 2000 Windows 95 / Release 1.0 / Internal Version .022 / 5:30 PM 8/14/95" | VERIFIED |
| Edition | **1995 CD Collection** build. sc2kfix keys this edition on the same timestamp `0x302FEA8A` | VERIFIED (timestamp match) |

## PE headers

- Machine i386, PE32, subsystem 2 (Windows GUI), OS/subsystem version 4.0 (Win95/NT4 era).
- ImageBase `0x00400000`, **entry RVA `0x8DF90`** (VA `0x0048DF90`), in `.text`. Entry bytes `64 A1 00 00 00 00 55 8B` = `mov eax, fs:[0]; push ebp; mov ebp, esp`: an SEH-frame prologue (VERIFIED bytes; interpretation as C runtime startup is STRONG INFERENCE).
- Stack reserve 0x20480, heap reserve 0x100000. Checksum 0. Characteristics `0x010E` (executable, 32-bit, line numbers/locals stripped; **relocations present**: 24,930 base-relocation entries, so the image is relocatable).
- Linker version field 2.55.
- **No debug directory, no Rich header, no TLS, no exceptions directory.**
- Export directory exists (0x3E bytes) but lists **0 functions**; only the module name.

## Sections

| Name | RVA | VirtSize | RawSize | Entropy | Notes |
|---|---|---|---|---|---|
| `.text` | 0x1000 | 0xC41A8 | 0xC4200 | 5.90 | Code (~800 KB). Starts with a block of `jmp rel32` thunks (VERIFIED at VA 0x40103C, 0x4014F1, 0x4029C8, 0x402405) |
| `.bss` | 0xC6000 | 0xA0F4 | 0 | – | Uninitialised globals, VAs 0x4C6000–0x4D00F4 (e.g. app object at VA 0x4C6010 per sc2kfix) |
| `.rdata` | 0xD1000 | 0x13640 | 0x13800 | 3.37 | Read-only data / strings / vtables (UNVERIFIED) |
| `.data` | 0xE5000 | 0x8950 | 0x8A00 | 3.54 | Initialised globals |
| `.idata` | 0xEE000 | 0x2ADC | 0x2C00 | 5.62 | Imports |
| `.edata` | 0xF1000 | 0x3E | 0x200 | 0.51 | Empty export table |
| `.rsrc` | 0xF2000 | 0x4AB49 | 0x4AC00 | 4.34 | 381 resource leaves |
| `.reloc` | 0x13D000 | 0xF1B1 | 0xF200 | 6.15 | Base relocations |

No section is packed (entropies are normal for code/data); no overlay detected beyond sections.

## Compiler / framework fingerprints

- **MFC (statically linked)**: strings `AfxWnd`, `AfxFrameOrView`, `AfxMDIFrame`, `AfxControlBar`, `AfxOldWndProc`, `Afx:%x:%x:%x:%x`; and an MFC assertion/diagnostic string referencing source file `SIMVW.CPP` ("See CSimcityView::OnInsertObject in SIMVW.CPP"). No `MFC*.DLL` import. (VERIFIED strings; static MFC 3.x per sc2kfix headers `mfc3xhelp.h`.)
- Section layout (`.text .bss .rdata .data .idata .edata .rsrc .reloc`) is characteristic of the Microsoft toolchain of the era. HYPOTHESIS: Microsoft Visual C++ 2.x/4.x. Not proven; no Rich header or debug info exists to confirm.
- Original class names known from the community project (UNVERIFIED in this binary until located): `CSimcityApp`, `CSimcityDoc`, `CSimcityView`, `CMainFrame`, `CCityToolBar`, `CStatusControlBar`, dialogs for budget, newspaper, etc.
- **RTTI: none expected** (pre-RTTI MSVC). No RTTI type descriptors searched for yet; MFC `CRuntimeClass` structures are the likely source of class names (HYPOTHESIS, to check in `.data`).

## Imports (summary — full list in [windows-api-usage.md](windows-api-usage.md))

`WINMM.dll` (7), `KERNEL32.dll` (109), `USER32.dll` (176), `GDI32.dll` (109), `comdlg32.dll` (5), `WINSPOOL.DRV` (3), `ADVAPI32.dll` (9), `SHELL32.dll` (3), `COMCTL32.dll` (7). No `DDRAW`, `DSOUND`, `WING`, or `MFC` DLLs.

## Resources (`.rsrc`)

381 leaves: 35 BITMAP, 10 ICON (5 groups), 101 CURSOR (101 groups), 7 MENU, 35 DIALOG, 84 STRING tables, 1 ACCELERATOR, 1 VERSION, and one **custom type 240** (id 190, 81 bytes) containing NUL-separated records of stadium-sport names ("Football", "Baseball", "Soccer", "Cricket") each preceded by an 8-byte header `47 01 03 04 NN 00 00 00` where NN was 09/09/07/08 for the first four records (VERIFIED bytes; meaning UNKNOWN). Language 1033 (US English). Bitmaps are UI chrome (button states `*D/*F/*U`), 189–204 numbered strips, etc. Large artwork is in external files, not the exe.
Extract with `tools/pe_inspect.py <exe> --resources` (listing); a resource extractor is still to be written.

## Strings of interest

- External file references: `large.dat`, `smallmed.dat`, `special.dat`, `\%d.MID`, `\%d.WAV`, `%s\%d.bmp`, `%s\pal_mac.bmp`, `\pal_mstr.bmp`, `\titlescr.bmp`, `\about.bmp`, `\neighbor.bmp`, `\2000win.bmp`, `presents.bmp`, `original.mif`, `*.mif`, `*.scn`, `*.sc2`, `.\scenario\`, `.\sounds`, directory names `CITIES`, `SCENARIOS`, `TILESETS`, registry names `Software`, `Maxis`, `SC2K`, `REGISTRATION`.
- MIDI uses the **MCI `sequencer`** device (string `sequencer`; import `mciSendCommandA`). WAV effects use `sndPlaySoundA`.
- Timer failure message: "Unable to start the timer. SimCity 2000 cities won't grow." — the simulation clock is driven by a multimedia timer (`timeSetEvent`), VERIFIED string + import; exact period UNKNOWN.
- Developer/debug strings (e.g. "Marvin Maxis", "Debug") exist; the 1996 build has a hidden debug menu per sc2kfix — not yet confirmed here.

## Cross-check with the community project (sc2kfix)

sc2kfix's `include/sc2k/sc2k_1995.h` lists addresses for *this* build. Spot checks against our file:

| sc2kfix address | Claimed | Observed in our file | Result |
|---|---|---|---|
| 0x40103C `MainFrame_ToggleToolBars` | thunk | `E9 7F A4 00 00` (jmp) | consistent |
| 0x4014F1 `SimcityApp_ExitRequester` | thunk | `E9 CA 4E 00 00` (jmp) | consistent |
| 0x4029C8 `SwapCycle` | thunk | `E9 A3 41 05 00` (jmp) | consistent |
| 0x4A1700 `CmdTarget_OnCmdMsg` | MFC code | `83 EC 04 89 4C 24 00 53` (function prologue-like) | consistent (weak) |
| 0x4C6010 `pCSimcityAppThis` | global in .bss | inside `.bss` range | consistent |
| 0x4E903C `bLoColor` | global in .data | inside `.data`/bss range, zero on disk | consistent |
| 0x4EE79C / 7A0 / 7A4 / 7A8 / 7AC (IAT slots patched by sc2kfix's `InstallRegistryPathingHooks_SC2K1995`) | `RegOpenKeyExA`, `RegCreateKeyExA`, `RegQueryValueExA`, `RegSetValueExA`, `RegCloseKey` | exactly those imports at exactly those IAT addresses (parsed from our import table) | **VERIFIED match** |
| 0x42E737 / 0x42E746 (patched call sites) | `call` instructions | `E8 00 29 FD FF` / `E8 75 3B FD FF` (call rel32) | consistent |
| 0x401749 `SimcityApp_OnQuit` hook site | thunk | `E9 02 4C 00 00` (jmp) | consistent |

Five exact IAT-slot matches essentially rule out a different build: **sc2kfix's `sc2k_1995.h` / `hook_sc2k1995_miscellaneous.cpp` addresses are very likely valid for this binary (STRONG INFERENCE; each used address still gets checked before use).** That supports using sc2kfix's 1995 addresses as *hypotheses to verify*, but their much larger 1996-SE address table (`sc2k_1996.h`, ~360 named functions, ~390 globals) does **not** apply to this binary and must not be assumed to.

## Open questions

- Exact compiler/CRT/MFC versions.
- Locate main message loop, `WinMain`, `CSimcityApp::InitInstance`, the sim-clock timer callback.
- Where `TILESETS`/`.mif` are loaded at runtime and what the 8-byte type-240 record header means.
- Whether the 1995 build contains any of the 1996 SE features (sc2kfix claims otherwise for most).
