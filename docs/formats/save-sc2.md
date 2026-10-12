# Format: SimCity 2000 city / scenario (`.SC2`, `.SCN`)

Status: container + chunk sizes **VERIFIED** on all 61 supplied files (42 cities, 18 scenarios, `DEFAULT.SC2`) using `tools/sc2_chunks.py`. Field-level meaning is mostly **not yet verified in the executable**; items sourced from sc2kfix are labelled *(sc2kfix)*.

## Container (VERIFIED)

```
"FORM"  u32be length (= filesize - 8)  "SCDH"
chunk*: 4-byte tag, u32be length, <length> bytes   -- big-endian lengths, NO pad byte
```

Every file walks exactly to EOF. Chunk order is otherwise fixed (`MISC ALTM XTER XBLD XZON XUND XTXT XLAB XMIC XTHG XBIT XTRF XPLT XVAL XCRM XPLC XFIR XPOP XROG XGRP`), with `CNAM` **first** in 29 files (`DEFAULT.SC2`, all 18 scenarios, 10 cities), **last** in 27 files (e.g. `NYC.SC2`) and **absent** in 5 (`LONDON`, `PARADE`, `SYMCITY`, `TOKYO`, `VENICE`). Scenario files append `TEXT`(×2), `SCEN`, `PICT` and, in some, `TMPL` after `XGRP`. A loader must therefore not assume chunk order or presence of `CNAM`.

## Compression (VERIFIED)

All chunks except `CNAM`, `ALTM` and the scenario-only chunks are **byte RLE**:
- control `c` in `0x01..0x7F`: copy next `c` bytes literally;
- control `c` in `0x81..0xFF`: repeat the next single byte `c − 127` times;
- `0x80` never occurs (decoder rejects it).

Evidence: decoded sizes are identical across all 61 files (table below), which a wrong scheme would not produce.
Whether `SCEN/PICT/TEXT/TMPL` are compressed is UNKNOWN (stored sizes vary; treated as raw).

## Chunks (decoded sizes VERIFIED; interpretation noted)

| Tag | Decoded bytes | Structure | Interpretation / confidence |
|---|---|---|---|
| `CNAM` | 32 (raw) | Pascal-style: byte0 = length, name, padding | City name. Byte0 equals real length in scenarios ("Atlanta" → 07). In `DEFAULT.SC2` byte0 is 0x1F with "New City\0" then 0xFD fill — length byte unreliable there (VERIFIED observation) |
| `MISC` | 4800 | 1200 × 32-bit (endianness UNVERIFIED) | Global city state: dates, funds, population, RCI demand, budget, tax, etc. Layout UNKNOWN here; sc2kfix `save_vanilla.cpp` holds a field map to cross-check |
| `ALTM` | 32768 (raw) | 128×128 × **u16 big-endian** | Altitude map. **VERIFIED on NYC.SC2:** bits 0–4 land altitude (values 4 and 6 dominate), bits 5–9 water level (6 on 99% of tiles), bits 10–15 tunnel depth (all 0); e.g. raw 198 = altitude 6 / water level 6 (flat land), 196 = altitude 4 under water level 6 |
| `XTER` | 16384 | 128×128 × u8 | Terrain tile id |
| `XBLD` | 16384 | 128×128 × u8 | Building/overlay tile id |
| `XZON` | 16384 | 128×128 × u8 | Zone type (low nibble) + corner flags (high nibble) *(sc2kfix; bit order UNVERIFIED)* |
| `XUND` | 16384 | 128×128 × u8 | Underground (subway/pipes) tile id |
| `XTXT` | 16384 | 128×128 × u8 | Sign/text overlay index |
| `XBIT` | 16384 | 128×128 × u8 flags | Tile flags: salt water, flipped, water, mark, watered, piped, powered, powerable *(sc2kfix, bit order UNVERIFIED)* |
| `XLAB` | 6400 | 256 × 25 bytes | Labels: length-prefixed ≤24-char strings *(sc2kfix `map_XLAB_t`: 24 chars + pad)* |
| `XMIC` | 1200 | 150 × 8 | "Microsim" records *(sc2kfix `microsim_t`: u8 tile, u8 stat0, 3×u16 stats)* — size matches the 8-byte struct exactly |
| `XTHG` | 480 | 40 × 12 | "Things": ships, helicopters, monsters, units *(sc2kfix `map_XTHG_t`, 12 bytes: type, dir, state, x, y, z, px, py, dx, dy, label, goal)* — size matches exactly |
| `XTRF` `XPLT` `XVAL` `XCRM` | 4096 each | 64×64 × u8 | Half-resolution maps: traffic, pollution, land value, crime (names by community convention; UNVERIFIED in exe) |
| `XPLC` `XFIR` `XPOP` `XROG` | 1024 each | 32×32 × u8 | Quarter-resolution maps: police coverage, fire coverage, population density, rate of growth (UNVERIFIED) |
| `XGRP` | 3328 | **16 series × 52 × int32 big-endian** (VERIFIED size = 16·52·4 and plausible values: NYC series 0 starts 81,380 = population×10) | Graph history, written by 0x413910: series order City Size, Residential, Commercial, Industrial, Traffic, Pollution, Value, Crime, Power, Water, Health, Education, Unemployment, GNP, Nat'l Population, Fed Rate; window layout per series: [0..11] monthly, [12..31] 6-monthly, [32..51] 5-yearly (STRONG INFERENCE from the shift code; order from the in-game graph dialog) |
| `SCEN` | 52 or 56 | UNKNOWN | Scenario parameters (disaster, time limit, goals) — scenarios only |
| `PICT` | 4298 / 4364 | UNKNOWN | Scenario picture (size suggests small bitmap; possibly Mac PICT remnant by name) |
| `TEXT` (×2) | 105–131 / ~300 | text | Scenario description/briefing strings |
| `TMPL` | 338 | UNKNOWN | Present in 5 scenario files |

A `.sc2kfix` extension ("SC2X") exists in the patch as a ZIP+JSON container; it is **not** part of the original format and is not needed.

## Native implementation plan
`src/save/sc2_container.{h,cpp}`: chunk reader + RLE decode/encode (round-trip test: decode→encode→decode equality; byte-identical re-encode is *not* assumed because RLE encodings aren't unique). Keep unknown chunks verbatim. CLI `sc2k-save-info` built on it (this Python prototype is its reference). Next step: reconstruct `MISC` offsets from the exe's load routine and compare with sc2kfix before trusting them.

## Endianness (VERIFIED)
All multi-byte values observed in saves are **big-endian** (XGRP int32, ALTM u16). The game flips arrays on load (`FlipDWORDArrayEndianness`, sc2kfix name) — the format is the Mac layout.

## Open questions
Endianness of `MISC`/`ALTM`/`XMIC` words; bit order of `XZON`/`XBIT`/`ALTM` bitfields (check by loading a known city and counting); `XGRP`, `SCEN`, `PICT`, `TMPL` layouts; whether the game requires specific chunk order.
