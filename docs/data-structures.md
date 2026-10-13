# Data structures

Policy: record the **original** byte layout first; modern C++ types come later. Every row carries provenance. Sources so far are (a) our decoded file sizes and (b) sc2kfix headers, which describe the 1996 SE build and must be re-verified in build .022 before use.

## Saved-city map layers (sizes VERIFIED, fields from sc2kfix UNVERIFIED)

Map is 128×128 tiles (VERIFIED: 16384-byte layers, `ALTM` = 2 bytes/tile).

| Layer | Element | Layout | Provenance |
|---|---|---|---|
| ALTM | u16 | bits 0–4 altitude, 5–9 water level, 10–15 tunnel depth (LSB-first, endianness of the word unknown) | sc2kfix `map_ALTM_attribs_t` — HYPOTHESIS |
| XZON | u8 | low nibble zone type, high nibble corner flags (bit packing order: C bitfield rule, LSB first) | sc2kfix — HYPOTHESIS |
| XBIT | u8 | b0 saltwater, b1 flipped, b2 water, b3 mark, b4 watered, b5 piped, b6 powered, b7 powerable | sc2kfix `map_XBIT_bits_t` — HYPOTHESIS |
| XTER / XBLD / XUND / XTXT | u8 | tile id / tile id / tile id / text overlay index | sc2kfix; sizes VERIFIED |

## Entity tables (sizes VERIFIED)

| Table | Count × size | Layout (sc2kfix, UNVERIFIED) |
|---|---|---|
| XMIC microsim | 150 × 8 | `u8 tileId; u8 stat0; u16 stat1; u16 stat2; u16 stat3` (packed) — 8-byte total matches the 1200-byte chunk |
| XTHG things | 40 × 12 | `u8 type, dir, state, x, y, z, px, py, dx, dy, label, goal` — 12-byte total matches the 480-byte chunk |
| XLAB labels | 256 × 25 | files: Pascal strings (length byte, ≤24 chars); in memory (and in `World::xlab`) C strings, converted in `World::fromCity` / `writeBack` (0x44D140 treats them as C strings) |

## Integer-width caution (Phase 4 requirement)
Do not "modernise" counters until their widths are proven. sc2kfix function signatures show `__int16` returns/params for the RNG and tile coordinates, `WORD` for microsim stats and `BYTE` for most map data. These are the first places where overflow/wrap behaviour must be examined (Phase 11, 16).

## TODO (next sessions)
`MISC` field map (1200 words), budget structure (`budget_t` in sc2kfix: 12 monthly counts, 12 monthly funds, current costs, funding %, YTD cost, estimate), newspaper/news structs, neighbour-city blocks (`neighbor_city_t`, 32 bytes), scenario `SCEN` block.
