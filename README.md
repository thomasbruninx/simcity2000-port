# SimCity 2000 — native macOS port (reverse-engineering project)

A clean-room-style reimplementation of SimCity 2000 (Windows 95, 1995 CD Collection build .022) for macOS/Apple Silicon, loading **your own legally obtained game files**. No game assets or binaries are included or distributed.

**Status:** a playable port with a **verified** simulation core. The simulation (RNGs, clock, save format, power, water, environment, demand, finance, zone growth, traffic, destroy effects, things, economy, events, the complete 25-phase tick, the tools, terrain generation and sea level) is bit-verified against the original machine code in an emulator on all supplied cities and scenarios ([docs/validation/006](docs/validation/006-differential-testing-against-original-code.md)). On top of it `sc2k-app` has the original's map renderer, tools, menus, windows (budget, graphs, advisors, newspaper, industry, neighbors, city map), audio, the map editor, the help system, and **the resource editors** (SCURK: Place & Print, Pick & Copy, Paint The Town; Load Tile Set). The interface itself is the port's own. Start with:
- [docs/roadmap.md](docs/roadmap.md) — plan based on findings
- [docs/ui-architecture.md](docs/ui-architecture.md) — the interface toolkit (widgets, text layout, input routing) and how to extend it
- [docs/original-file-inventory.md](docs/original-file-inventory.md)
- [docs/executable-analysis.md](docs/executable-analysis.md), [docs/windows-api-usage.md](docs/windows-api-usage.md)
- [docs/scurk.md](docs/scurk.md) — the Resource Editors (SCURK port: Place & Print, Pick & Copy, Paint The Town); [docs/ui.md](docs/ui.md) — the windows, menus, shortcuts and dialogs
- [docs/formats/](docs/formats/) — file formats (save format: verified container + RLE)
- [docs/community-patch-analysis.md](docs/community-patch-analysis.md), [docs/known-versions.md](docs/known-versions.md)
- [docs/reverse-engineering/journal.md](docs/reverse-engineering/journal.md)

Confidence labels used everywhere: **VERIFIED**, **STRONG INFERENCE**, **HYPOTHESIS**, **UNKNOWN**.

## Dependencies
Everything is built with CMake from source; on macOS the libraries come from [Homebrew](https://brew.sh).

| Dependency | Needed for | Required? | Install |
|---|---|---|---|
| C++20 compiler (Apple Clang / Xcode command line tools), CMake >= 3.20 | everything | **required** | `xcode-select --install`, `brew install cmake` |
| Ninja | faster builds (`-G Ninja`) | optional | `brew install ninja` |
| pkg-config | finds FluidSynth and FreeType | recommended | `brew install pkgconf` |
| [SDL3](https://libsdl.org) (zlib licence) | the game window, input, audio output (`sc2k-app`) | **required for `sc2k-app`** (the libraries, tests and command-line tools build without it) | `brew install sdl3` |
| [FreeType](https://freetype.org) (FTL / GPLv2) | the newspaper's Times New Roman text; without it the 8x8 debug font is used | optional | `brew install freetype` |
| [FluidSynth](https://www.fluidsynth.org) (LGPL-2.1), linked dynamically | music with a SoundFont (Settings > FluidSynth); without it only the built-in synthesizer is available | optional | `brew install fluid-synth` |
| A General MIDI SoundFont | FluidSynth music; `resources/GeneralUser-GS.sf2` ([GeneralUser GS](https://schristiancollins.com/generaluser.php), its own free licence) is picked up automatically, any other `.sf2` in `resources/` works too | optional | included |
| [Unicorn](https://www.unicorn-engine.org) (GPL-2.0) | `sc2k-emu-diff`, the differential tests against the original machine code; **dev-only, never linked into the game** | optional (`-DSC2K_BUILD_EMU=OFF` to skip) | `brew install unicorn` |
| Python 3 (standard library only) | the scripts in `tools/` | optional | `brew install python` |
| [Ghidra](https://ghidra-sre.org) | reproducing the decompilations in `reverse/` (not committed) | optional, dev-only | `brew install ghidra` |

**Runtime data** (never included): a legally owned SimCity 2000 for Windows 95 installation (build .022). Pass its folder to `sc2k-app` or set `SC2K_DATA_PATH`. At run time the app also reads **Times New Roman** from the system fonts (`/System/Library/Fonts/Supplemental/`; override the file with `SC2K_FONT`).

Quick start (macOS): `brew install cmake ninja pkgconf sdl3 freetype fluid-synth` then `cmake -S . -B build -G Ninja && cmake --build build && build/sc2k-app /path/to/SC2K`.

## Playing
`sc2k-app <SC2K dir>` opens the title screen (Start New City, Edit New Map, Load Scenario, Load Saved City, Load Tile Set, Settings, Resource Editors, Quit). In the game: the menu bar of the original with its Ctrl shortcuts (Ctrl+L load, N new, S save, P pause, 1-4 speed, B budget, O ordinances, C population, I industry, G graphs, H neighbors, M map, X exit), F1 help, right click for the shortcut menu (right drag scrolls), `+` / `-` / wheel zoom, R rotate, Esc pause menu, Ctrl+T back to the title screen. Typing `priscilla` shows the debug menu. Details: [docs/ui.md](docs/ui.md), [docs/scurk.md](docs/scurk.md).

## Tools (`tools/`, Python 3, stdlib only; run with `python3 -I`)
| Tool | Purpose |
|---|---|
| `pe_inspect.py <exe> [--imports] [--resources]` | PE32 headers, sections, imports, exports, resources, version info |
| `inventory.py <dir> <out.csv>` | Hashes/types for an installation |
| `sc2_chunks.py [--summary] <files>` | List/decode SC2/SCN chunks (RLE) |
| `gen_api_usage.py <exe>` | Import tables grouped by category |
| `ghidra/*.java` | Headless Ghidra helpers: export functions, decompile, disassemble, xrefs |
| `demand_model.py`, `rng_model.py` | Independent Python models used to cross-check ports |
| `sc2k-save-info`, `sc2k-verify` (C++, built by CMake) | Save summary; compare scans with saved cities |
| `sc2k-app` (C++, SDL3) | The game so far: live simulation of a saved city with toolbar with the original icons (bulldozer, terrain, zoning, roads incl. highways/tunnels/onramps, rail, subway, power, pipes, buildings), bridges, budget / ordinances / bonds window, query + sign tools, graphs, city, advisors and newspaper windows (original page layouts, photographs and Times New Roman through optional FreeType: `brew install freetype`), tool availability by year, surface / underground view, power and water overlays: `sc2k-app <SC2K dir> [city.sc2]`. Headless: `--screenshot out.png --days N --view underground|power|water --do "tool Road (bridge; drag 56 56 56 74; ..."` renders the real UI to a PNG |
| `sc2k-resources` (C++) | `strings`, `dialogs` (control layout and texts of the exe's dialog templates), `menus` and `bitmaps <dir>` (RT_BITMAP resources, numbered and named) of your SIMCITY.EXE. The app reads its messages, budget/ordinance labels and toolbar icons from there at runtime; nothing is stored in the repo |
| `sc2k-render` (C++) | Draw a city to a PNG: `sc2k-render <SC2K dir> city.sc2 out.png [zoom] [w] [h] [a b] [rotation]` |
| `sc2k-sim` (C++) | Headless simulation of a saved city: `sc2k-sim SIMCITY.EXE city.sc2 <days> [thing-frames-per-day] [out.sc2]` |
| `sc2k-emu-diff` (C++, optional, needs Unicorn) | Differential tests against the original machine code: `power water env demand finance zone destroy sealevel things economy tick` (`tools/run_emu_diffs.sh`), also `terrain`, `tools`, `disaster`, `advisor` |
| `sc2k-emu-diff` + `run_emu_diffs.sh` (dev-only, needs Homebrew `unicorn`, GPL) | Run original routines of your `SIMCITY.EXE` in an emulator and diff ports against them |

Build and test: `cmake -S . -B build -G Ninja && cmake --build build && SC2K_DATA_PATH=/path/to/SC2K ctest --test-dir build` (data-dependent tests skip when the variable is unset).

Ghidra headless example (project kept outside the repo):
```
analyzeHeadless <projdir> sc2k95 -import SC2K/SIMCITY.EXE -overwrite
```

## Licence & attribution
Research draws on the MIT-licensed [sc2kfix](https://sc2kfix.net) project (OpenCity Foundation); credited per fact in the docs. Licence for this repo: MIT (see LICENSE).
