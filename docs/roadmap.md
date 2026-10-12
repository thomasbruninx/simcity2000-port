# Proposed reverse-engineering roadmap

Derived from what reconnaissance actually showed. Order is driven by dependencies, not by the generic phase list.

## What changed relative to the generic plan
1. **A large head start exists, with a catch.** sc2kfix gives names/addresses for ≈360 functions, but for the *1996 SE*. Our binary is the 1995 CD build; only ~10 addresses are shared. → Build a **cross-build matcher** (signature/function-hash based) to port labels 1996→1995 instead of re-deriving everything.
2. **Rendering is GDI/DIB + palette animation, MFC-based.** There is no DirectDraw. The port can render an 8-bit software framebuffer and convert to RGBA; palette cycling becomes a table swap.
3. **Sprite pixel format is the gating item for any rendering.** The archives are indexed but compressed.
4. **The save format is nearly solved** (container+RLE proven) → quick win for tests and for loading real cities.
5. **TILES.DB / MIF / SCURK are not needed** for the first playable milestone.

## Milestones

**M0 — tooling & knowledge base (this session, done):** inventory, PE inspector, chunk lister, Ghidra headless pipeline, docs.

**M1 — static map of the engine (next 1–3 sessions)**
- Import SIMCITY.EXE into a persistent Ghidra project (kept outside git); apply sc2kfix 1995 labels as *unverified*; extract MFC class/message-map structure and vtables to name UI classes.
- Locate by xref: `large.dat` loader, `timeSetEvent` call (sim clock), RNG routines (sc2kfix hints: LCG + LFSR), `\%d.MID` player, `%s\%d.bmp` loader, registry key construction.
- Build the cross-build matcher (1996 names → 1995 functions) using byte-pattern hashing with relocation masking.

**M2 — assets**
- Decode sprite archives (`LARGE/SMALLMED/SPECIAL`) → PNG extractor + palette from `PAL_MSTR.BMP`/resources; validate visually.
- Resource extractor for exe bitmaps/icons/cursors/menus/dialogs/strings. `TEXT_USA`/`DATA_USA` formats.

**M3 — saves & world model**
- `libsc2`: container, RLE, `MISC` map, layer accessors, scenario chunks; `sc2k-save-info`; round-trip tests on all 61 files.

**M4 — native skeleton**
- CMake presets (Homebrew: SDL3 3.4.x, fluid-synth 2.6.x, cmake present; Dear ImGui needs FetchContent/submodule — not installed), window + palette-indexed framebuffer, load verified sprite, input, ImGui dev overlay.

**M5 — isometric renderer:** recover projection constants/draw order from the draw routines; screenshot-compare tests.

**M6 — time & RNG:** recover timer period, speed settings, calendar, RNG exactness + tests.

**M7 — first playable (Phase 32):** load city, render, scroll, query tile, sim clock, stats, MIDI via FluidSynth, SFX.

**M8+ — simulation subsystems** in dependency order discovered in M1 (likely: RNG → clock → power/water scans → zone growth → traffic → economy → disasters), each with tests and validation experiments against the Windows build.

## Risks / challenges
- **Compressed sprite format** and tile→sprite mapping (UNKNOWN effort).
- **MFC-heavy code**: separating game logic from UI plumbing requires vtable/message-map recovery.
- **Simulation fidelity** (16-bit arithmetic, RNG exactness, ordering of per-tick scans) needs a way to run the original: validation will need a Windows environment (the user has one; no Wine here).
- **Patch applicability**: sc2kfix fixes are unproven for build .022.
- **Legal hygiene**: keep Ghidra DBs, extracted assets and the exe out of git (`.gitignore` enforces); docs summarise structure only.
- **Anomalous `SCURKUSA.DLL`** (2019 timestamp) — don't treat as authoritative.

## Blockers / decisions needed from you
None blocking. Open choices: see the final report.
