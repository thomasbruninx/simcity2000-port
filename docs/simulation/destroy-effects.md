# Destroy effects (0x458690 and helpers)

Status: **ported and bit-verified** against the original machine code (docs/validation/006): 122 runs (61 cities × animate on/off), ≈ 200,000 demolitions, all layers + census + labels/microsim/things + `rand()`/LFSR end state identical. Code: `src/simulation/destroy.cpp`.

## What the routine is
`0x458690(this, a, b, animate)` (thiscall, 3 stack args, `ret 0xC`) demolishes whatever stands at `(a, b)`: ordinary buildings, runway/taxiway networks, tunnels, bridges. It is called by the zone pass (collapsing bridges/tunnels), by the shoreline fixer (`0x4469C0`, which makes the two mutually recursive), by disasters and by the demolish tool. Its *simulation* effect is small and exact; its bulk is **animation**: sprite blits, dirty-rectangle bookkeeping (`0x439EB0`, `0x451360`, `0x45AA50`, `0x435100`, `0x4351F0`, `0x40ED80`), sound (`0x424AA0`), `UpdateWindow` and a 100 ms delay with message pump (`0x45AEC0`). All of that is presentation (VERIFIED by reading each helper: they touch only redraw rectangles and window state) and is omitted. **What must be kept is the `rand()` traffic of the animation code**, because the CRT generator is shared with the simulation: two calls per animated sprite (sprite id, flip).

## Cases (`id` = building at (a,b); `size` from `footprintOf`)
| Case | Sim effect | `rand()` calls |
|---|---|---|
| id < 6 | nothing | – |
| bridge / highway-bridge | walk back to the span start; abutment tile on dry land becomes water (altitude −1, XBIT water set, shoreline fix); then remove every span piece (2×2 for highways) and finish with a second abutment | per piece: 2 if animated (+3 more for 2×2) |
| 0xDF / 0xE0 | depth-first flood of connected 0xDF/0xE0 tiles → 0 (zone corner flags cleared, XBIT `&0x3D`) | 2 per tile if animated |
| 0xDD / 0xDE | same flood, tiles become rubble `(rand&3)+1` | 1 per tile + 2 if animated |
| tunnel mouth 0x3F–0x42 | direction from the id; clear the tunnel-depth bits (ALTM `&0x3FF`) along the line until the other mouth; both mouths → 0 | 4 **always** (drawn regardless of `animate`) |
| any other building | `size³×2` calls if animated (frames × tiles × 2); toolbar flags for 0xF3/0xD0/0xDB/0xFF (hook); military mask update for 0xD7 microsim; every footprint tile → rubble `(rand&3)+1` unless XTER ≠ 0 (then 0), XBIT `&0x3D`, XZON `&0x0F`, text overlay released (`0x44C850`: label/microsim/thing record cleared; 0xFA decrements `c9408`/`cb4f0`); highways and water-adjacent single tiles trigger the shoreline fixer | as listed |

Quirks reproduced: the flood's 4th neighbour test uses `x < 0x7F` where `y` was meant; the animation `rand` calls in the tunnel branch ignore `animate`; the shoreline fixer destroys any building on a tile whose altitude changes.

## Helpers
- `footprintOf (0x45AB10)`: highway pieces snap to an even/odd pair (size 2); ids < 0x70 size 1; sizes 3/4 probe the eight neighbours' XZON corner flags against the rotation's corner table to reach the anchor; the result tile is adjusted using the tile's own corner flag. Table `footprint[id]` @0x4E75C8 (ids 0xC9–0xCF and 0xD7–0xDA, 0xFB–0xFF are 4×4; 0xAE–0xC5 3×3; …).
- `bridgeSegment (0x4635D0)`: classifies highway/bridge pieces (returns −1 for non-highway, 0xE/0xF/0x10 for water-adjacent end pieces, `id − 0x5D` otherwise).
- `releaseTextOverlay (0x44C850)`: XTXT value 1..0x32 → label record cleared; 0x3D..0xC8 → microsim record + label cleared; 0xC9..0xF0 → thing record's label byte cleared.
- `fixShore (0x4469C0)`: **water-level fixer.** destroys buildings on the tile (> 0xC), clears underground, builds an 8-neighbour "higher neighbour" mask from altitudes, looks up the terrain shape in `shoreType[mask]` (@0x4E6948), then sets/clears the water flag against the **sea level** (MISC word 912 / `0x4C9830` — previously documented as "water factor"), writing XTER (shape, +0x10 or +0x20 for shallow water) and raising ALTM water-level bits.

## Known limitations
`footprintOf` and `bridgeSegment` probe tiles at index −1/128 for buildings on the very border of the map; the original reads memory outside its tables there (undefined). The port wraps the index; the emulator test skips border tiles.

## Hooks left
`ZoneHooks::uiFlag` (toolbar flags), presentation calls are simply omitted.
