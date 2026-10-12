#!/usr/bin/env python3
"""Independent Python model of the original RNGs (from the x86 disassembly, docs/simulation/rng.md).
Prints known-answer vectors used by tests/test_rng.cpp."""
M32 = 0xFFFFFFFF
def s16(v): v &= 0xFFFF; return v - 0x10000 if v & 0x8000 else v

class Lcg:
    def __init__(s, seed): s.s = seed & M32
    def step(s): s.s = (s.s * 0x41C64E6D + 0x3039) & M32; return (s.s >> 16) & 0x7FFF
    def mod(s, n):                       # CWD; IDIV word -> remainder sign follows dividend (non-negative here)
        r = s.step(); n = s16(n); assert n != 0
        q = abs(r) // abs(n); return s16(r - q * n if n > 0 else r - q * (-n))

class Lfsr:
    def __init__(s, seed): s.s = seed & 0xFFFF
    def step(s):
        v = s.s; s.s = ((v << 1) ^ 0x1BF5) & 0xFFFF if v & 0x8000 else (v << 1) & 0xFFFF; return s.s
    def mod(s, n): n = s16(n); assert n != 0; return s16(s.step() % abs(n))   # state >= 0
    def mask(s, m): return s.step() & m

class CrtRand:
    def __init__(s, seed=1): s.s = seed & M32
    def next(s): s.s = (s.s * 0x343FD + 0x269EC3) & M32; return (s.s >> 16) & 0x7FFF

if __name__ == '__main__':
    c = CrtRand(); print('crt rand seed 1:', [c.next() for _ in range(10)])
    l = Lcg(1); print('lcg seed 1 mod 100  :', [l.mod(100) for _ in range(8)])
    l = Lcg(0x12345679); print('lcg seed 0x12345679 mod 200:', [l.mod(200) for _ in range(8)])
    f = Lfsr(1); print('lfsr seed 1 raw     :', [f.step() for _ in range(8)])
    f = Lfsr(0xACE1); print('lfsr seed 0xACE1 mod 128 :', [f.mod(128) for _ in range(8)])
    f = Lfsr(0xACE1); print('lfsr seed 0xACE1 &3:', [f.mask(3) for _ in range(8)])
    f = Lfsr(1); n = 0
    while True:
        f.step(); n += 1
        if f.s == 1 or n > 70000: break
    print('lfsr period from seed 1:', n)
