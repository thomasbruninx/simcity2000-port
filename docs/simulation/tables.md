# Constant tables read from SIMCITY.EXE

All are read at run time by `GameTables::fromExe` (after checking the PE timestamp `0x302FEA8A`); none are stored in this repository.

| VA | Content | Used by |
|---|---|---|
| 0x4E85A8 | pollution per XBLD id (bytes) | environment scan stage 1 |
| 0x4E8638 | population-density value per XBLD id (bytes; ids 0x70–0xC5) | environment scan stage 6 |
| 0x4E6468 | tile class per XBLD id; value 2 halves land value (zone pass reads the same bytes at 0x4E64D8 = id−0x70) | environment scan |
| 0x4D9CE8..0x4D9D08 | five doubles: 1.0, 1/150000, 0.01, 15.0, 600.0 | demand |
| 0x4E8528 | four doubles `[0, 1.2, 1.1, 0.95]` (economy factor by MISC word 7) | demand |
| 0x4E8318 | int16 tax→demand table (23 entries then unrelated data) | demand |
| 0x4E8EA0 | 16 × int16 budget divisors | finance |
