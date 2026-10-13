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


## Status update (2026-10-12, end of session 7)
Done and verified bit-exactly against the original code on 61 cities: M3 (libsc2), M6 (RNG + clock), and in M8+ the power, water, environment, demand, finance and zone-growth/traffic subsystems. The emulator-based differential harness (docs/validation/006) is now the primary tool: port a routine, diff it, fix until zero differences.
**Remaining simulation work, in suggested order:** destroy effects 0x458690 → military/airport/seaport zones (0x437C30 and callees) → thing system (spawners, vehicles, monsters; 0x4501D0/0x451360) → economy model 0x4686C0 and the event half of 0x467AE0 → disasters (0x45C870 handlers) → scenario goals/game over (phase 22) → message/advisor logic. Then the headless driver for the complete `SimulationTick` (0x412FE0), diff-tested end to end, followed by M4/M5 (SDL3 skeleton, renderer) and M7 (first playable).

**Update (session 10):** destroy effects, zone types 7–9, the thing system and microsim allocation are ported and emulator-verified. Remaining before the headless tick driver: economy model 0x4686C0, event half of 0x467AE0, disasters (0x45C870 handlers incl. 0x45F0D0), phase 22 goals, advisor logic 0x4720A0.

**Update (session 11):** the complete 25-phase tick, economy, events, history, yearly microsim update and the headless driver (`Simulation`, `sc2k-sim`) are done and diff-verified end to end. Remaining simulation work: disaster handlers (0x45C870 …), scenario goals / game over, message queue as saved state. Next major milestones: SDL3 skeleton (M4), renderer from the sprite archives (M5), audio, first playable (M7).

**Update (session 12):** M4/M5 first cut: SDL3 application (`sc2k-app`), software map renderer for zoom 0–2 with rotation, sprite archive and palette loaders. Next for rendering: things, traffic cars, water animation, overlays; then UI (toolbar, dialogs), tools (build/bulldoze), audio.

**Update (session 13):** the renderer now draws things, traffic, 2x2 highways and animates water by palette cycling; underground view (pipes, subway, water table) and power/water overlays exist. The building tools are ported **and emulator-verified**: bulldozer, zoning, road/rail/power/pipe/subway drags with bridges and neighbour-city links, 40+ building buttons (docs/simulation/tools.md); the app has a toolbar, drag previews with costs, modal questions (bridge type, neighbour connection) and a message line. Remaining tool work: terrain tools, landscape, highways, tunnels, onramps, rail-subway link, query/microsim tools; UI: budget, newspaper, graphs, advisors, settings dialogs, tool availability by year, original button bitmaps (`sc2k-resources bitmaps`).

**Update (session 14):** terrain tools, highways (own planner, bridges), tunnels, onramps, rail link and hydro dam are ported and emulator-verified (20 runs x 800 operations). The app now has a budget + ordinances window (taxes 0-20 %, services 0-100 %, ordinance toggles; labels from the exe's dialog templates) and uses the original toolbar bitmaps from SIMCITY.EXE. Still open: landscape tools (trees/water), query/microsim editing, bonds, ordinance side effects beyond the ported simulation, tool availability by year, newspaper/graphs/advisors/population windows, remaining original bitmaps (advisor portraits 197-204, small map-window icons), audio.

**Update (session 15):** tool availability by year (0x469A10, diff-verified), landscape tools and emergency dispatch (diff-verified), bonds and the city-value routine (diff-verified), query and sign tools, graphs / city / advisors / newspaper windows (docs/ui.md for what is original and what is the port's reading). Next: building-specific query dialogs and microsim naming, the original advisor / newspaper generators, population age model, new-city setup (0x42DDE0), scenario goals / game over, disaster handlers, then audio and the first playable build.
