# Random terrain (Edit New Map)

Port: `src/simulation/terrain_gen.{h,cpp}` (`generateTerrain`), `Simulation::startNewCity(..., terrain)`, Ctrl+N dialog "Random terrain". Differential test `sc2k-emu-diff terrain`: the whole original generator 0x410430 (progress-window calls stubbed) against the port on 7 parameter sets (including 0 % and 100 % settings, coast and river on and off) on 4 files: **identical** (altitudes, tiles, water flags, census, RNG and LCG states).

Parameters (the dialog's sliders, 0..47 each): hills, water, trees; flags: coast, river. Steps (strings 800..809 are the progress texts):
1. clear: altitudes 0, terrain 0, every tile id 0 (census updated), water flag cleared; rotation 0.
2. **Creating Hills** (0x447040): an 8x8 grid of `rand() % (hills + 11) + 1` seeds every 16th tile, then four midpoint-displacement passes 0x4470E0 (steps 8, 4, 2, 1; outside the map the altitude is `hills + 10`, 0 beyond the high-a edge when there is a coast).
3. sea level = (water + 4) / 8 (at least 4 with a coast or river). **Carving Water**: coast 0x4474C0 (a wandering shore 10..39 tiles deep along the high-a edge, LCG), river 0x447610 (7 tiles wide from a = 127 to 0, drifting columns, LCG).
4. **Smoothing** x2 (0x447380: average with the four neighbours), altitude transform `t = (v + 3) / 2; v = t - 4 >= 4 ? t - 4 : (t - 4 >= 0 ? 4 : t)`, **More Smoothing**, water-level bits cleared.
5. **Reticulating Splines**: cliff limiter 0x446830 (no tile more than one level above a neighbour) and the shore / water fixer 0x4469C0 on every tile.
6. **Adding Forest** (0x446EA0): `trees^2 / 2` clusters of up to 63 random tree tiles (ids 6..12).
7. coast: **Smoothing Coast** (0x4477C0: salt-water flag clean-up, four passes); then `water / 4` **streams** (0x445DF0, 50..177 tiles from random places).
The original then centres the view on tile (64, 64); the new-city setup (docs/simulation/new-city.md) follows.
