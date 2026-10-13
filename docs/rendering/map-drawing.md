# Map drawing (zoom 0–2) — renderer notes

Code: `src/render/map_renderer.*` (software renderer), `src/resources/{sprite_archive,bmp}.*`, viewer `src/app/main.cpp` (SDL3), headless `sc2k-render`.
Sources: the original redraw routines `0x43A660` (single ground tile), `0x439EB0` (dirty rectangle of a tile) and `0x43C0DB` (zoom-2 tile body, a Ghidra fragment of the big redraw), decompiled and read.

## Projection (VERIFIED from 0x43A660 / 0x439EB0)
For zoom `z` (0..3 in the code; sprites exist for 0..2): tile half-width `4<<z`, half-height `2<<z`, altitude step `3<<z`.
`x = (a - b) * (4<<z) + scrollX` (left edge of the 2:1 diamond), `yBottom = (a + b) * (2<<z) + scrollY - alt * (3<<z)`, sprite top = `yBottom - spriteHeight`.
`alt` = `ALTM & 0x1F` for land (XTER < 0x10) and `(ALTM & 0x3E0) >> 5` (water level) for water tiles.
Sprite ids are `base + 500 * z`: zoom 2 → LARGE.DAT (1000+), zooms 0/1 → SMALLMED.DAT. Zoom 3 sprites (SPECIAL.DAT etc.) are not used yet.

## What a tile shows (STRONG INFERENCE; the viewer reproduces it for ordinary city views)
* **Ground**: sprite `terrainSprite[XTER]` (table `0x4E6608`, shore/slope variants 256.., water 270..). An empty zoned lot (XBLD = 0, XTER = 0, zone ≠ 0) uses the zone tile `0x50A + zone − 1000` (+500·z).
* **Small objects** (XBLD 0x01–0x6F: rubble, trees, roads, rails, power lines, bridges): ground first, then sprite `XBLD + 500·z` with its bottom at the tile; XBIT bit 2 mirrors the sprite. Bridge pieces stand at the (water-level) altitude of their tile like the ground — the earlier "shifted by (landAlt − waterAlt)·step" inference was wrong (0x43C0DB has no such term); the drawbridge (0x58) is drawn raised (0x59) when a ship is within a Manhattan distance of 8.
* **Buildings** (XBLD ≥ 0x70): no ground; the sprite is drawn once, from the corner tile whose XZON high-nibble bit matches `rotationZoneMask[rotation]` (the left-most corner on screen); `y_top = yBottom + width/4 − (2<<z) − height`.
* **Order**: painter's order by `a+b` (display coordinates), left to right.
* **View rotation**: display coordinates `(da,db)` from file `(a,b)`: rot 0 `(a,b)`, rot 1 `(b,127−a)`, rot 2 `(127−a,127−b)`, rot 3 `(127−b,a)` — derived so that the corner flags written by building stamps select the left-most corner in each case; visually verified.

## Not drawn yet
Things (ships, planes, trains, …: sprite table `0x4E7070`, positions from tile + sub-position), traffic cars on roads, animated water, power/water pipe overlays and the underground view, zone-view colouring, highways' multi-part pieces (drawn as plain sprites), network/pipe overlays, night shading. The original also animates by palette cycling.

## Verification
`sc2k-render` writes PNGs; NYC, Paris, a rotated view and zoom 0–2 were inspected. `tests/test_render.cpp` pins a fingerprint of the output of this renderer (regression only — it is not recorded from the original, which cannot be run headless).
