#!/usr/bin/env python3
"""Prototype SC2/SCN chunk lister + RLE tester (read-only).

Usage: python3 -I tools/sc2_chunks.py <file.sc2|scn> [...]   (or --summary for aggregate decode check)

Container (VERIFIED on all 61 supplied files): 'FORM' u32be(len=filesize-8) 'SCDH', then chunks
[4cc][u32be len][len bytes], NO padding. RLE hypothesis (from outside documentation, tested here):
control byte c: c<=127 -> copy c literal bytes; c>=129 -> repeat the next byte (c-127) times; c==128 unused.
"""
import sys, struct

def rle_decode(b):
    out = bytearray(); i = 0
    while i < len(b):
        c = b[i]; i += 1
        if c <= 127:
            out += b[i:i+c]; i += c
        elif c >= 129:
            out += bytes([b[i]]) * (c - 127); i += 1
        else:
            raise ValueError('control byte 128')
    return bytes(out)

def chunks(d):
    if d[:4] != b'FORM' or d[8:12] != b'SCDH': raise ValueError('not SCDH')
    if struct.unpack_from('>I', d, 4)[0] + 8 != len(d): raise ValueError('FORM length mismatch')
    o = 12
    while o < len(d):
        t = d[o:o+4].decode('latin1'); n = struct.unpack_from('>I', d, o+4)[0]
        yield t, d[o+8:o+8+n]; o += 8 + n
    assert o == len(d)

RAW = {'ALTM', 'CNAM'}  # stored uncompressed (observed)
def decode(tag, body):
    return body if tag in RAW or tag in ('TEXT', 'PICT', 'SCEN', 'TMPL') else rle_decode(body)

if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0] == '--summary':
        import collections, glob
        res = collections.defaultdict(set)
        for f in args[1:]:
            for t, b in chunks(open(f, 'rb').read()):
                try: res[t].add(len(decode(t, b)))
                except Exception as e: res[t].add('ERR:' + str(e))
        for t, s in res.items(): print(t, sorted(s, key=str)[:8])
    else:
        for f in args:
            print(f)
            for t, b in chunks(open(f, 'rb').read()):
                try: dl = len(decode(t, b))
                except Exception as e: dl = 'ERR'
                print('  %-4s stored %6d  decoded %s' % (t, len(b), dl))
