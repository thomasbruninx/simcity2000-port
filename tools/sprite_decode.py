#!/usr/bin/env python3
"""SC2K sprite archive reader/decoder + validator + PNG sheet exporter (read-only).

Usage:
  sprite_decode.py validate <SC2K_dir>
  sprite_decode.py sheet <SC2K_dir> <archive: LARGE|SMALLMED|SPECIAL> <first_index> <count> <out.png>

Archive index (big-endian): u16 count; entries {u16 id; u32 offset; u16 height; u16 width}.
(Header order height,width confirmed by SIMCITY.EXE blitter FUN_004857f0: +4 added to Y, +6 to X.)
Sprite stream: 2-byte records [arg][op], derived from the unclipped blitter FUN_00485ca0 (VA, build .022):
  op 0 : no-op            op 1 : end of scanline (arg = byte length of the row's records, observed)
  op 3 : skip `arg` transparent pixels
  op 4 : copy `arg` literal palette-index bytes that follow; data padded to even length
  op 2 / other : end of sprite (blitter 'default: return')
"""
import sys, struct, zlib, os

def load_archive(path):
    d = open(path, 'rb').read(); n = struct.unpack_from('>H', d, 0)[0]
    ents = [struct.unpack_from('>HIHH', d, 2 + 10 * i) for i in range(n)]
    out = []
    for i, (sid, off, h, w) in enumerate(ents):
        end = ents[i + 1][1] if i + 1 < n else len(d)
        out.append(dict(index=i, id=sid, h=h, w=w, data=d[off:end]))
    return out

def decode(spr, strict=True):
    """Return (rows, info): rows = list of bytearray width w with None->transparent as 256."""
    w, h, s = spr['w'], spr['h'], spr['data']
    img = [[256] * w for _ in range(h)]
    y = -1; x = 0; i = 0; rowstart = 0; ended = False; warn = []
    while i + 1 < len(s):
        arg, op = s[i], s[i + 1]; i += 2
        if op == 0: continue
        if op == 1:
            if y >= 0 and strict and (i - 2 - rowstart) != rowlen: warn.append('rowlen y=%d got %d want %d' % (y, i - 2 - rowstart, rowlen))
            y += 1; x = 0; rowstart = i; rowlen = arg
            if y >= h: warn.append('too many rows'); break
        elif op == 3: x += arg
        elif op == 4:
            if y < 0: warn.append('pixels before first row'); break
            if x + arg > w: warn.append('row overflow y=%d x=%d n=%d' % (y, x, arg))
            for k in range(arg):
                if x + k < w: img[y][x + k] = s[i + k]
            x += arg; i += arg + (arg & 1)
        else:
            ended = True; break
    if y >= 0 and strict and ended and (i - 2 - rowstart) != rowlen: warn.append('last rowlen got %d want %d' % (i - 2 - rowstart, rowlen))
    if y + 1 != h: warn.append('rows %d != h %d' % (y + 1, h))
    return img, dict(ended=ended, warn=warn, consumed=i, size=len(s))

def read_bmp_palette(path):
    d = open(path, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]
    pal = [(d[54 + 4 * i + 2], d[54 + 4 * i + 1], d[54 + 4 * i]) for i in range((off - 54) // 4)]
    return pal

def write_png(path, w, h, rgba):
    raw = b''.join(b'\x00' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def ch(t, b): c = struct.pack('>I', len(b)) + t + b; return c + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))

ARCH = {'LARGE': 'DATA/LARGE.DAT', 'SMALLMED': 'DATA/SMALLMED.DAT', 'SPECIAL': 'DATA/SPECIAL.DAT'}

def main():
    cmd, root = sys.argv[1], sys.argv[2]
    if cmd == 'validate':
        for name, rel in ARCH.items():
            sp = load_archive(os.path.join(root, rel)); bad = 0; pix = 0; ex = []
            for s in sp:
                img, info = decode(s)
                if info['warn'] or not info['ended']:
                    bad += 1
                    if len(ex) < 3: ex.append((s['id'], s['w'], s['h'], info['warn'][:2], info['ended']))
                pix += sum(1 for r in img for p in r if p != 256)
            print('%s: %d sprites, %d with warnings, opaque pixels %d' % (name, len(sp), bad, pix), ex)
    elif cmd == 'sheet':
        sp = load_archive(os.path.join(root, ARCH[sys.argv[3]])); a = int(sys.argv[4]); n = int(sys.argv[5])
        pal = read_bmp_palette(os.path.join(root, 'BITMAPS/PAL_MSTR.BMP'))
        sel = sp[a:a + n]; cols = 16; cw = max(s['w'] for s in sel) + 2; chh = max(s['h'] for s in sel) + 2
        rows = (len(sel) + cols - 1) // cols; W = cols * cw; H = rows * chh
        buf = bytearray([40, 40, 40, 255] * (W * H))
        for k, s in enumerate(sel):
            img, _ = decode(s, strict=False); ox = (k % cols) * cw + 1; oy = (k // cols) * chh + 1
            for y, r in enumerate(img):
                for x, p in enumerate(r):
                    if p != 256:
                        c = pal[p]; o = ((oy + y) * W + ox + x) * 4; buf[o:o + 4] = bytes((c[0], c[1], c[2], 255))
        write_png(sys.argv[6], W, H, buf); print('wrote', sys.argv[6], W, H)

if __name__ == '__main__': main()
