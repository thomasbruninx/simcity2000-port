#!/usr/bin/env python3
"""Read-only inventory of an original SimCity 2000 installation.

Usage: python3 -I tools/inventory.py <SC2K dir> <out.csv>

Writes metadata only (path, size, extension, magic-based type, SHA-256,
purpose guess). No file content is copied. The "purpose" column is a
heuristic keyed on directory/extension and is NOT evidence about internals.
"""
import os, sys, csv, hashlib, struct

def sniff(d):
    if d[:2] == b'MZ':
        o = struct.unpack_from('<I', d, 0x3c)[0] if len(d) > 0x40 else 0
        sig = d[o:o+2] if o < len(d) else b''
        return {b'PE': 'PE32 executable/DLL', b'NE': 'NE (16-bit Windows) executable/DLL'}.get(sig, 'MZ (DOS)')
    if d[:4] == b'FORM' and d[8:12] == b'SCDH': return 'IFF-like FORM/SCDH (SimCity 2000 city/scenario)'
    if d[:4] == b'MIFF': return 'MIFF container (SCURK tileset/"mif")'
    if d[:4] == b'MThd': return 'Standard MIDI file'
    if d[:4] == b'RIFF' and d[8:12] == b'WAVE': return 'RIFF WAVE'
    if d[:2] == b'BM': return 'Windows BMP'
    if d[:1] == b'\x0a' and len(d) > 128: return 'PCX'
    if d[:4] == b'?_\x03\x00': return 'WinHelp 3.x HLP'
    if d[:4] == b'\x3f\x5f\x03\x00': return 'WinHelp 3.x HLP'
    return 'unknown/raw'

PURPOSE = {
    'SIMCITY.EXE': 'main game executable', 'WINSCURK.EXE': 'SCURK (Urban Renewal Kit) executable',
    'SCURKUSA.DLL': 'SCURK resource/strings DLL (Borland-style exports)', 'D2HTOOLS.DLL': 'Doc-2-Help helper DLL for WinHelp (16-bit)',
    'TILES.DB': 'tile/graphics database (format unresolved)', 'DEFAULT.SC2': 'template city for "New City"',
    'VERSION.INF': 'game version text', 'SVERSION.INF': 'SCURK credits/version text',
}
DIRP = {'BITMAPS': 'UI/title bitmaps, palettes', 'SOUNDS': 'MIDI music (1xxxx) / WAV effects (5xx)',
        'CITIES': 'sample saved cities', 'SCENARIO': 'scenario saves', 'DATA': 'sprite/text/data archives',
        'SCURKART': 'SCURK tilesets (.MIF) and sample images'}

def main():
    root, out = sys.argv[1], sys.argv[2]
    rows = []
    for dp, _, fs in os.walk(root):
        for f in sorted(fs):
            p = os.path.join(dp, f); rel = os.path.relpath(p, root)
            d = open(p, 'rb').read()
            top = rel.split(os.sep)[0] if os.sep in rel else ''
            rows.append(dict(path=rel, size=len(d), ext=os.path.splitext(f)[1].lower(),
                             type=sniff(d), sha256=hashlib.sha256(d).hexdigest(),
                             purpose=PURPOSE.get(rel, DIRP.get(top, ''))))
    rows.sort(key=lambda r: r['path'])
    with open(out, 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    print(len(rows), 'files,', sum(r['size'] for r in rows), 'bytes')

if __name__ == '__main__': main()
