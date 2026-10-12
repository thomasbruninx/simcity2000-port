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
