# SimCity 2000 — native macOS port (reverse-engineering project)

A clean-room-style reimplementation of SimCity 2000 (Windows 95, 1995 CD Collection build .022) for macOS/Apple Silicon, loading **your own legally obtained game files**. No game assets or binaries are included or distributed.

**Status:** reconnaissance complete; no engine code yet. Start with:
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
| `ghidra/ExportFunctions.java` | Headless Ghidra: export function list |

Ghidra headless example (project kept outside the repo):
```
analyzeHeadless <projdir> sc2k95 -import SC2K/SIMCITY.EXE -overwrite
```

## Licence & attribution
Research draws on the MIT-licensed [sc2kfix](https://sc2kfix.net) project (OpenCity Foundation); credited per fact in the docs. Licence for this repo: TBD by the owner.
