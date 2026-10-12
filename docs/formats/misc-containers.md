# Format notes: other containers

## `DATA/*_USA.IDX` + `.DAT` (UNKNOWN, partly decoded)
- `DATA_USA.IDX` (48 B): six little-endian pairs `(id u32, offset u32)` = `(1000,0) (1001,500) (1002,1000) (1003,11000) (1004,160227) (1005,160387)`. Offsets lie inside `DATA_USA.DAT` (160,547 B). VERIFIED structure; contents of the `.DAT` UNKNOWN. It begins with ascending big-endian u16 values `0x005C 0x005D 0x0065 0x0069 …`, which look like an offset table (HYPOTHESIS).
- `TEXT_USA.IDX` (1352 B): `(id u32 LE, offset u32 LE)` pairs, starting `(128,0) (500,5959) (1000,6119) (1001,6322)…`; ids 128, 500, 1000+ look like Win32 string/resource ids. `TEXT_USA.DAT` is CRLF text (credits at start). VERIFIED for the first records; remaining structure UNKNOWN.

## `SCURKART/*.MIF`
`MIFF` + u32be length + `SC2KINFO` chunk + length `0x72`… includes an embedded Windows path `C:\SC2K4WIN\SCURKART\RIO.mif`. UNKNOWN layout; see sprite-archives.md.

## Standard formats (VERIFIED)
- `BITMAPS/*.BMP`: Windows 3.x 8-bit palettised DIBs; `PAL_MSTR`, `PAL_STTC`, `PAL_LOAD` are 101×101 and `PAL_MAC` is 32×26 (names suggest master/static/loading/Mac palettes — HYPOTHESIS; palettes still to be compared); `400–411.BMP` are 155×100 (154×100 for 411). The exe loads `%s\%d.bmp` (numbered 400–411, 128) and `\titlescr.bmp`, `\about.bmp`, `\neighbor.bmp`, `\2000win.bmp`, `\pal_mstr.bmp`, `\pal_mac.bmp`.
- `SOUNDS/5xx.WAV`: RIFF PCM 8-bit mono 11025 Hz (31 files, ids 500–530). Played by `sndPlaySoundA`.
- `SOUNDS/100xx.MID`: SMF type 1, 19 files, PPQ 192 (most), 480 (some), 120 (one); 5–22 tracks. Played by MCI sequencer.
- `SC2USA.HLP/SCURK.HLP`: WinHelp 3.x — not required for play.
