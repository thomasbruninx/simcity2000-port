# `MISC` chunk field map (1200 big-endian 32-bit words)

Source: the game's city loader `FUN_00430CB0` (VA 0x430CB0, build .022), decompiled. It reads the decoded `MISC` array (global `DAT_004E6278`, 0x12C0 bytes) after byte-swapping and copies word *i* into runtime globals. Index → global is **VERIFIED**; semantics are noted per row. Words are 32-bit even for fields stored in 16-bit or 8-bit globals (truncation on load). Constants for the verified ones are in `src/save/misc_fields.h`.

Sanity check on all 61 samples: word 0 = `0x122` (the loader rejects anything else with "Unable To Load: Old File Version"); start year 1900/1950/2050, speeds 1–4, rotations 0–3, treasury values plausible.

| Word(s) | Runtime global | Meaning | Confidence |
|---|---|---|---|
| 0 | – | file version, must equal 0x122 | VERIFIED |
| 1 | 0x4C9444 (short) | mode (always 1 in samples) | UNKNOWN meaning |
| 2 | 0x4C8444 | **view rotation** 0–3 | VERIFIED (selects water-scan order; effect confirmed) |
| 3 | 0x4C960C | start year | VERIFIED (year shown = start + day/300) |
| 4 | 0x4C9E1C | day counter | VERIFIED |
| 5 | 0x4C945C | treasury | VERIFIED |
| 6 | 0x4CB500 | debt-like value subtracted in goal tests | HYPOTHESIS |
| 7 | 0x4CA41C | index into 8-byte double table 0x4E8528 (economy) | STRONG |
| 8 | 0x4CA028 | city-class milestone index | HYPOTHESIS |
| 9–13 | 0x4C94E8, 0x4C9458, 0x4CA46C, 0x4CB70C, 0x4CB928 | …, land-value total, crime total, traffic total, pollution total | STRONG (totals produced by scans) |
| 14–17 | 0x4C9D40, 0x4CA030, 0x4C9608, 0x4CA478 | counters (4C9608 accumulates population sums) | UNKNOWN |
| 18, 19 | 0x4CA428, 0x4CB4CC | health / education scores (graph series 10, 11) | STRONG |
| 20–23 | 0x4C9940, 0x4CA02C, 0x4C9950, 0x4C91D4 | national population, GNP-like, federal rate, … (graph series 14, 13, 15) | STRONG |
| 24 | 0x4C9DF8 (byte) | (weather-table derived) | UNKNOWN |
| 25 | 0x4C9E24 (byte) | **wind** | VERIFIED (wind-turbine output; improved fit of random-plant cities) |
| 26 | 0x4CA3EC (byte) | **cloudiness** | VERIFIED (solar output, pump output) |
| 27 | 0x4CA424 (byte) | weather-table state | STRONG |
| 28 | 0x4C9438 | pending disaster type (1–0x12) | STRONG |
| 29 | 0x4C91AC | economy scalar | UNKNOWN |
| 30 | 0x4E8A14 | flag (triggers a UI call when it changes) | UNKNOWN |
| 31–90 | 0x4C84CC/84D4/9A88 [20] | 20 interleaved triples (3 arrays of 20 dwords) | UNKNOWN |
| 91–123 | 0x4C940C[11] (short), 0x4CA3F8[11] (short), 0x4CA408[11] (dword) | 11 interleaved triples — industry/sector model arrays used by the economy phase | STRONG |
| 124–379 | 0x4C94E0[256] (short) | **tile census**: count of each XBLD id | VERIFIED |
| 380–387 | 0x4C9440[8] | zone population accumulators | STRONG |
| 388–437 | 0x4C8F08[50] (short) | UNKNOWN table | UNKNOWN |
| 438–453 | 4 × {0x4CB4F4 short, 0x4C9D28, 0x4C981C, 0x4C9944} | 4 neighbour cities: name index (0 = "Ocean"), values | STRONG |
| 454–461 | 0x4CB910[8] (short) | demand valves: R, C, I at 0–2 (clamped ±2000) | STRONG |
| 462–478 | 0x4CA448[17] (short) | scheduled-event years | HYPOTHESIS |
| 479–910 | 0x4CB4E4 budget: 16 × 27 words | per category: currentCosts, fundingPercent, yearToDate, then 12 × (countMonth, fundMonth) — **layout differs from the in-memory record** (0x70 bytes: counts at +0, funds at +0x30) | VERIFIED order |
| 911 | 0x4C9D44 | budget-year flag | STRONG |
| 912 | 0x4C9830 (short) | **water factor**: pump output term `factor*5` | VERIFIED (water validation) |
| 913–915 | 0x4C84D8, 0x4CA410, 0x4CB4FC (byte) | flags/military base type … | UNKNOWN |
| 916–945 | 0x4C9D3C: 6 × 5 bytes | UNKNOWN | UNKNOWN |
| 946–999 | 0x4C91D8: 9 × {short, short, 4 bytes} | 9 records (8 bytes in memory) | UNKNOWN |
| 1000 | 0x4C9A58 | **ordinance bitmask** (20 bits) | VERIFIED (energy-conservation bit 0x10000 used by power scan) |
| 1001 | 0x4C9610 | military/unemployment style percentage | UNKNOWN |
| 1002–1017 | 0x4C994C[16] (short) | per-category counters | UNKNOWN |
| 1018 | 0x4C9434 | counter | UNKNOWN |
| 1019 | app+0x308 | **game speed** | VERIFIED |
| 1020–1027 | 0x4CB4C8, 0x4C95F0, app+0x1E4, app+0x1E0, 0x4CB4EC, 0x4CA3E8, 0x4CB4D4, 0x4C9820 | misc flags (newspaper subscription, etc.) | UNKNOWN |
| 1028 | – | scroll position (0xFFFF = default 0x40/0x80; low 7 bits and high byte) | STRONG |
| 1029 | 0x4C6CB0 | zoom/scale | UNKNOWN |
| 1030, 1031 | 0x4CA470, 0x4CA474 | city centre ×2 (row, col) | STRONG (produced by environment scan) |
| 1032–1050 | 0x4C84DC, 0x4C9DF4, 0x4CB4F8, 0x4C9A8C, 0x4C9200, 0x4C8440, 0x4C9448, 0x4C91F4, 0x4C9834, 0x4C9954, 0x4C8F00, 0x4CA3F4 | misc state (4C9A8C = population ×10; 4C8F00 = disaster-in-progress; 4CA3F4 = water-shortage flag) | mixed |

Note: the tile census is stored one 16-bit value per 32-bit word, so `MISC[124+id]` is the count for building id `id`.

## Native implementation
`CityFile::miscWord/miscInt` + `misc::k*` constants; a typed `CityState` view will be added as more fields are verified. Unknown words are preserved verbatim (the 4800-byte array round-trips).
