# Random-number generation (build .022)

Status: **VERIFIED** from disassembly (`reverse/ghidra`, VA below). Two independent generators exist, plus the MSVC CRT `rand` (unused by game logic as far as seen).

## Original implementation

### LCG — "RandomWordLCGMod" (sc2kfix name) — state `DWORD @0x4CCB98`
| VA | Function | Behaviour |
|---|---|---|
| 0x0047FED0 (thunk 0x401F37) | `SeedLCG(u32)` | `state = arg` (full 32 bits) |
| 0x0047FEF0 (thunk 0x401163) | `LCGMod(i16 n) -> i16` | `state = state*0x41C64E6D + 0x3039` (mod 2³²); `r = (state>>16) & 0x7FFF`; `return r % n` using 16-bit signed `CWD; IDIV word`, low 16 bits of remainder (DX) returned |

Disassembly:
```
IMUL EAX,[0x4ccb98],0x41c64e6d ; ADD EAX,0x3039 ; MOV [0x4ccb98],EAX
SHR EAX,0x10 ; AND AX,0x7fff ; CWD ; IDIV word [ESP+4] ; MOV AX,DX
```
Edge cases (from instruction semantics): `n == 0` → divide-error exception (crash); `r` is always non-negative so `n < 0` gives a non-negative remainder (`r % |n|`); `n == -1`/`1` → 0.

### LFSR — state `WORD @0x4CCB94` (a DWORD is written; upper half unused)
| VA | Function | Result |
|---|---|---|
| 0x0047FF20 (thunk 0x40265D) | `SeedLFSR(u32)` | writes the dword; only the low 16 bits are ever used |
| 0x0047FF40 (thunk 0x401CA8) | `LFSRMod(i16 n)` | step, then `state % n` with `n` sign-extended to 32-bit signed, state zero-extended; returns low 16 bits |
| 0x0047FF80 / FFC0 / 480040 / 4800C0 / 480100 | `LFSR & 1 / 3 / 0xF / 0x3F / 0x7F` | step, return `state & mask` (sc2kfix: "Mod4", "Mod128" = `&3`, `&0x7F`) |

Step (`ADD AX,AX` then conditional `XOR AX,0x1BF5`): `if (s & 0x8000) s = (s<<1) ^ 0x1BF5; else s = s<<1;` (16-bit). Seed 0 is a fixed point (stays 0 forever).
Static usage: 33 direct calls to `LFSRMod`, 19 to `LCGMod` (plus the mask variants).

### Seeding (VERIFIED, `FUN_00424c30`, VA 0x424CD6–0x424CF6)
```
SeedLFSR(GetTickCount() | 1);  SeedLCG(GetTickCount() | 1);
```
Both seeds come from wall-clock time; there is no fixed seed and **the RNG state is not stored in saves** (not present in any chunk we decoded — UNVERIFIED for `MISC`). Consequently a city advanced N ticks is not reproducible in the original; validation must compare statistics, not exact states (see [validation notes](../validation/)).

### CRT `rand` (0x48BCD8): `s = s*0x343FD + 0x269EC3; return (s>>16)&0x7FFF` (MSVC). Use by game code: none found (CRT-internal).

## Native implementation
`src/simulation/rng.{h,cpp}`: `Lcg`, `Lfsr` with exactly the semantics above; tests with known-answer vectors from an independent model. Seeding is explicit (the app seeds from time like the original; tests/validation seed deterministically). `n == 0` is a precondition violation (original faults).

## Open questions
Which subsystems use which generator (call-site census → `docs/simulation/rng-usage.md`), and whether the `% n` bias for LCGMod (`r` 15-bit) is observable for large `n` (it is for `n > 32768` — never used that big?).
