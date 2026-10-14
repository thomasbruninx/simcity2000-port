# MIF — SCURK object sets

Files: `SCURKART/*.MIF` (ORIGINAL.MIF holds the game's own graphics; user sets hold edited objects). Implemented in `src/scurk/mif.{h,cpp}`; the reader/writer round-trips every MIF of the installation byte for byte (test_scurk).

## Container (VERIFIED, all files parse)
IFF-like, **big-endian** chunk sizes: `MIFF` form containing `SC2K` type, an `INFO` chunk, then one `TILE` chunk per object. A `TILE` holds sub-chunks `SHAP` (one per zoom: small / medium / large) and optionally `NAME`.

## Object ids (VERIFIED)
Object *N* is the building / tile id N; its sprites in the game's archives are **N** (small), **N+500** (medium), **N+1000** (large).
A shape of height 1 is a *placeholder*: "not customised, use the original" (the port falls back to the original archives).

## SHAP pixel stream (VERIFIED)
The game's row RLE, pairs `[arg][op]`:
| op | meaning |
|---|---|
| 1 | new row, arg = byte length of the row |
| 3 | skip arg transparent pixels |
| 4 | literal run of arg pixels, data padded to an even length |
| other | end of shape; arg is kept as `endArg` |
Unedited streams are preserved byte for byte (the padding bytes carry junk); edited shapes are re-encoded with zero padding.

## Names and groups (STRONG INFERENCE)
186 objects in ORIGINAL.MIF first-appearance order. Long / short names are the consecutive non-empty strings 32001.. of WINSCURK.EXE (one id is skipped); the groups of the *Object Group* menu are assigned by the first object of each group.

## Unknown
`TILES.DB` (referenced only by WINSCURK.EXE) — format not analysed.
