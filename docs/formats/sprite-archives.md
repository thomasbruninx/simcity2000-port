# Format: sprite archives `DATA/LARGE.DAT`, `SMALLMED.DAT`, `SPECIAL.DAT`

Status: **index structure VERIFIED** (monotonic offsets, in-bounds, counts consistent). Pixel encoding **UNKNOWN**. The names appear as strings in SIMCITY.EXE (`large.dat`, `smallmed.dat`, `special.dat`) so they are runtime data (VERIFIED).

## Index (big-endian) — VERIFIED structure, field meaning STRONG INFERENCE

```
u16be count
entry[count]:  u16be id; u32be offset; u16be width; u16be height     (10 bytes each)
u16 ?          (LARGE.DAT: none — first offset == 2 + 10*count;  SMALLMED/SPECIAL: 2 extra bytes before data, UNKNOWN)
```

| File | Size | count | id range (min–max) | unique ids | First entry | Last offset |
|---|---|---|---|---|---|---|
| LARGE.DAT | 681,180 | 501 | 1001–1499 | 499 | id 1256 @5012, 17×32 | 680,880 |
| SMALLMED.DAT | 246,572 | 904 | 1–999 | 899 | id 14 @9044, 5×8 | 245,052 |
| SPECIAL.DAT | 50,376 | 50 | 573–755 | 50 | id 701 @504, 42×64 | 50,004 |

Observations (VERIFIED): entry data lengths are *smaller* than `width*height` (e.g. id 1256: 396 bytes for 17×32 = 544), so pixels are compressed or sparse (probably transparency-aware RLE) — encoding UNKNOWN. Entries are **not sorted by id** and a few ids occur twice (LARGE: 2 duplicates, SMALLMED: 5) — meaning (variants? animation frames? lookup quirk?) UNKNOWN. ids look like the same numbering space used by tile ids / resource ids (HYPOTHESIS).

## Related
- `TILES.DB` (951,400 B) is referenced only by WINSCURK.EXE. First bytes `DC 05 00 01 48 C2 10 00 05 00 08 00 01 01 …` look like a different index (little-endian?), UNKNOWN.
- `SCURKART/*.MIF` begin `MIFF` + `SC2KINFO` chunk — IFF-like, big-endian lengths (`00 00 8C 0C` etc.). UNKNOWN beyond that; WINSCURK.EXE and sc2kfix `modules/scurk_convert.cpp` / `hook_spritesandtilesets.cpp` are the references.
- sc2kfix `sprite_archive_t` / `LoadTilesFromMemory` (1996 build) is the likely pixel-format reference; must be checked against our binary.

## Next steps
1. In Ghidra, find the loader that opens `large.dat` (xref the string) → recover decompression routine.
2. Write `tools/sprite_extract.py` that dumps one sprite to PNG using `PAL_MSTR.BMP` palette; validate visually.
