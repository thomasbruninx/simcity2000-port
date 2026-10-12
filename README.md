# SimCity 2000 — native macOS port (reverse-engineering project)

A clean-room-style reimplementation of SimCity 2000 (Windows 95, 1995 CD Collection build .022) for macOS/Apple Silicon, loading **your own legally obtained game files**. No game assets or binaries are included or distributed.

**Status:** reverse-engineering with a growing, *verified* simulation core. Ported and bit-verified against the original machine code (differential tests in an emulator, see [docs/validation/006](docs/validation/006-differential-testing-against-original-code.md)): RNGs, clock/calendar, save format (`libsc2`), power, water, environment (pollution/land value/crime/density/coverage), demand valves, monthly finance, **zone growth pass (all zone types incl. military/airport/seaport), traffic router, tile primitives and destroy effects**. Pending: things (ships/trains/aircraft/monsters), economy model, random events, full-tick driver. Start with:
- [docs/roadmap.md](docs/roadmap.md) — plan based on findings
- [docs/original-file-inventory.md](docs/original-file-inventory.md)
- [docs/executable-analysis.md](docs/executable-analysis.md), [docs/windows-api-usage.md](docs/windows-api-usage.md)
- [docs/formats/](docs/formats/) — file formats (save format: verified container + RLE)
- [docs/community-patch-analysis.md](docs/community-patch-analysis.md), [docs/known-versions.md](docs/known-versions.md)
- [docs/reverse-engineering/journal.md](docs/reverse-engineering/journal.md)

Confidence labels used everywhere: **VERIFIED**, **STRONG INFERENCE**, **HYPOTHESIS**, **UNKNOWN**.

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
| `sc2k-emu-diff` + `run_emu_diffs.sh` (dev-only, needs Homebrew `unicorn`, GPL) | Run original routines of your `SIMCITY.EXE` in an emulator and diff ports against them |

Build and test: `cmake -S . -B build -G Ninja && cmake --build build && SC2K_DATA_PATH=/path/to/SC2K ctest --test-dir build` (data-dependent tests skip when the variable is unset).

Ghidra headless example (project kept outside the repo):
```
analyzeHeadless <projdir> sc2k95 -import SC2K/SIMCITY.EXE -overwrite
```

## Licence & attribution
Research draws on the MIT-licensed [sc2kfix](https://sc2kfix.net) project (OpenCity Foundation); credited per fact in the docs. Licence for this repo: MIT (see LICENSE).
