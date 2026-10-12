# Original file inventory

Source: `SC2K/` (user-supplied, read-only; never modified). Generated with `tools/inventory.py`;
the machine-readable table with SHA-256 for every file is [`original-file-inventory.csv`](original-file-inventory.csv).

Totals: **168 files, 16,044,082 bytes.** Types below come from magic-byte sniffing (VERIFIED) unless marked otherwise;
"purpose" statements are marked with confidence.

| Path | Files | Detected type | Purpose | Confidence |
|---|---|---|---|---|
| `SIMCITY.EXE` (1,299,456 B) | 1 | PE32 i386 GUI exe | Main game | VERIFIED (version resource, imports) |
| `WINSCURK.EXE` (2,014,870 B) | 1 | PE32 i386 GUI exe, Borland-style exports | SimCity Urban Renewal Kit (tile/city editor) | VERIFIED (version resource, strings) |
| `SCURKUSA.DLL` (165,237 B) | 1 | PE32 DLL | SCURK strings/menus/accelerators (59 STRING, 4 MENU, 4 ACCEL) | STRONG INFERENCE |
| `D2HTOOLS.DLL` (44,464 B) | 1 | **NE (16-bit)** DLL | Wextech "Doc-2-Help" WinHelp extension | VERIFIED (strings: "Doc-2-Help Tools DLL - Copyright 1993 by Wextech") — irrelevant to the port |
| `SC2USA.HLP`, `SCURK.HLP`, `SC2USA.GID` | 3 | WinHelp 3.x | Online help / generated help index | VERIFIED magic; contents unparsed |
| `VERSION.INF`, `SVERSION.INF` | 2 | text | Game version string / SCURK credits | VERIFIED |
| `DEFAULT.SC2` (45,129 B) | 1 | FORM/SCDH | Template city for *New City* | STRONG INFERENCE (name; sc2kfix) |
| `TILES.DB` (951,400 B) | 1 | unknown binary | **Referenced only by WINSCURK.EXE** (`%s\tiles.db`), not by SIMCITY.EXE | VERIFIED reference; format UNKNOWN |
| `DATA/LARGE.DAT` `SMALLMED.DAT` `SPECIAL.DAT` | 3 | self-indexed binary archives (see [formats/sprite-archives.md](formats/sprite-archives.md)) | Sprite/tile graphics; names are in SIMCITY.EXE strings (`large.dat`, `smallmed.dat`, `special.dat`) | VERIFIED names; index structure VERIFIED; pixel encoding UNKNOWN |
| `DATA/DATA_USA.DAT` + `.IDX` | 2 | binary + 6×(u32,u32) index | Unknown data (index ids 1000–1005) | index VERIFIED; contents UNKNOWN |
| `DATA/TEXT_USA.DAT` + `.IDX` | 2 | text (CRLF) + index | Game text (credits readable at start) | VERIFIED text; index format partly decoded |
| `CITIES/*.SC2` | 42 | FORM/SCDH | Sample cities | VERIFIED magic & chunk walk |
| `CITIES/CITIES.WRI` | 1 | Write document | Description of sample cities | STRONG INFERENCE (extension) |
| `SCENARIO/*.SCN` | 18 | FORM/SCDH + extra chunks (`SCEN`,`PICT`,`TEXT`,`TMPL`) | Scenarios | VERIFIED |
| `SCURKART/*.MIF` | 12 | `MIFF`/`SC2KINFO` container | SCURK tilesets (e.g. `ORIGINAL.MIF`, `FUTURE.MIF`, city styles) | VERIFIED magic; format UNKNOWN |
| `SCURKART/SAMPLE{1,2}.PCX` | 2 | PCX | Sample art for SCURK | VERIFIED |
| `SOUNDS/10000–10018.MID` | 19 | SMF format 1 | Music (names `\%d.MID` appear in exe) | VERIFIED |
| `SOUNDS/500–530.WAV` | 31 | RIFF PCM, 8-bit mono 11025 Hz | Sound effects (names `\%d.WAV` appear in exe) | VERIFIED |
| `BITMAPS/*.BMP` | 25 | Windows 8-bit BMP | Title screen, about, palette masters (`PAL_MSTR`, `PAL_STTC`, `PAL_MAC`, `PAL_LOAD`), terrain, numbered UI/dialog art (128, 400–411) | VERIFIED |

## Unusual observations

1. **No INI/registry export/installer** is present. SIMCITY.EXE imports registry APIs (`RegCreateKeyExA`, `RegQueryValueExA`, …) and `GetPrivateProfile*`; settings are therefore expected in the registry/INI, not shipped. sc2kfix's README calls the original installer "non-functional". (STRONG INFERENCE)
2. **`SCURKUSA.DLL` has a PE timestamp of 2019-01-03** (`0x5C2E1EFC`), far later than everything else (1995). The file is not a pristine 1995 build, or its header was rewritten. HYPOTHESIS: community-modified / re-linked. Hash recorded; do not rely on it for archaeology.
3. **`WINSCURK.EXE` PE timestamp is `0xBC7B1F0E` (year 2070)** — a bogus value, but sc2kfix uses the same constant to recognize this exact build (VERIFIED in `dllmain.cpp`).
4. Music files use **mixed time bases** (192, 480 and 120 PPQ) and 5–22 tracks; some have > 16 tracks (so are not simple single-port GM files). Relevant for FluidSynth playback ([audio notes](audio/)).
5. `TILES.DB` is **not** a game runtime file for SIMCITY.EXE (see above). Do not spend effort on it for the first playable milestone.
6. MIDI IDs 10000–10018 = 19 files; sc2kfix states some music "was included but never added to the random playlist" — playlist order lives in the exe/resources (UNKNOWN here).
7. `DATA_USA.*`/`TEXT_USA.*` suffix `USA` suggests localisation by suffix (HYPOTHESIS); only the USA set is present.
8. `D2HTOOLS.DLL` is a 16-bit NE binary; irrelevant on any modern platform.
