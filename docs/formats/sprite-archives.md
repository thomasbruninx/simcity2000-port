# Format: sprite archives `DATA/LARGE.DAT`, `SMALLMED.DAT`, `SPECIAL.DAT`

Status: **index and pixel encoding VERIFIED** — decoder `tools/sprite_decode.py` decodes all 1,455 sprites (501+904+50) with zero structural warnings and renders correctly using the `BITMAPS/PAL_MSTR.BMP` palette (visual check of LARGE.DAT: buildings, plants, stadium, terrain wedges). The names appear as strings in SIMCITY.EXE (`large.dat`, `smallmed.dat`, `special.dat`) so they are runtime data (VERIFIED).

## Index (big-endian) — VERIFIED structure, field meaning STRONG INFERENCE

```
u16be count
entry[count]:  u16be id; u32be offset; u16be HEIGHT; u16be WIDTH     (10 bytes each)
u16 ?          (LARGE.DAT: none — first offset == 2 + 10*count;  SMALLMED/SPECIAL: 2 extra bytes before data, UNKNOWN)
```

| File | Size | count | id range (min–max) | unique ids | First entry | Last offset |
|---|---|---|---|---|---|---|
| LARGE.DAT | 681,180 | 501 | 1001–1499 | 499 | id 1256 @5012, 17 high × 32 wide | 680,880 |
| SMALLMED.DAT | 246,572 | 904 | 1–999 | 899 | id 14 @9044, 5 high × 8 wide | 245,052 |
| SPECIAL.DAT | 50,376 | 50 | 573–755 | 50 | id 701 @504, 42 high × 64 wide | 50,004 |

Observations (VERIFIED): entry data lengths differ from `width*height` because the stream is row-RLE (below). Entries are **not sorted by id** and a few ids occur twice (LARGE: 2 duplicates, SMALLMED: 5) — meaning (variants? animation frames? lookup quirk?) UNKNOWN. ids look like the same numbering space used by tile ids / resource ids (HYPOTHESIS).

## Pixel stream (VERIFIED)
Each sprite is the byte range `[offset, next offset)`. It is a sequence of 2-byte records `[arg][op]`, read from the game's unclipped blitter (`FUN_00485ca0`, build .022) and confirmed by the decoder:

| op | meaning |
|---|---|
| 0 | no-op |
| 1 | start of a new scanline; `arg` = byte length of the records that follow for that row (matches in every sprite) |
| 3 | skip `arg` transparent pixels |
| 4 | copy `arg` literal palette-index bytes that follow; the data is padded to an even length |
| 2 (any other) | end of sprite |

Sprites are 8-bit palette indices (index of `PAL_MSTR.BMP`), no alpha: transparency = skipped pixels. Header order **height, width** (the earlier doc had these swapped): the blitter adds +4 to Y and +6 to X.
At runtime the game keeps the raw stream in memory and decodes on the fly: table `DAT_004ca1d0`, 8 bytes per id `{ptr, height, width}` (loader `FUN_00434c6f`, setter `FUN_004875d0`); `SetSpriteForDrawing` = `FUN_00435100` stores clip rect/target in globals `0x4CCE48–0x4CCE64`, `FUN_004857f0` selects among blit variants (unclipped `FUN_00485ca0`, clipped `FUN_00485e10`, flipped/mirror `…85d70`/`…85fa0`, masked `…86140`/`…86210`; further variants to 0x486BA0 — only the first is analysed).
Native: `src/resources/sprite.{h,cpp}` = this decoder; keep mirror/mask variants to be analysed.

## Open
Meaning of duplicate ids; the 2 extra bytes in SMALLMED/SPECIAL index; mirror/mask blit variants; tile-id→sprite-id mapping.

## Related
- `TILES.DB` (951,400 B) is referenced only by WINSCURK.EXE. First bytes `DC 05 00 01 48 C2 10 00 05 00 08 00 01 01 …` look like a different index (little-endian?), UNKNOWN.
- `SCURKART/*.MIF` begin `MIFF` + `SC2KINFO` chunk — IFF-like, big-endian lengths (`00 00 8C 0C` etc.). UNKNOWN beyond that; WINSCURK.EXE and sc2kfix `modules/scurk_convert.cpp` / `hook_spritesandtilesets.cpp` are the references.
- sc2kfix `sprite_archive_t` / `LoadTilesFromMemory` (1996 build) is the likely pixel-format reference; must be checked against our binary.

