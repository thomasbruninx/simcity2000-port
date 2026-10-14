# Resource Editors (SCURK port)

Main menu → **Resource Editors** opens a submenu with SCURK's three utilities. They share one **Source** and one **Working** object set (`.MIF`, see [formats/mif.md](formats/mif.md)) and use SCURK's own menus, read from WINSCURK.EXE (menu ids 200/300/400/500, strings 29000+). The menu bar offers *File / Edit / Options / Object Group / Window / Help* as in the original.

## Usage
* **Place & Print** — the city editor. The city is shown in the game's own map view, paused and free of cost; every tool except disasters / dispatch is available. Pick *Object Group* from the menu, then an object from the **Place Object** list and click the map to place it (buildings ≥ 0x70 through the simulation's special-building placement, other ids as plain tiles). The working set's graphics are installed as sprite overrides while editing. File: Load City, Save / Save As, New City (blank terrain from DEFAULT.SC2), Export City (whole map, zoom 0, 24-bit BMP). Printing is not available in the port. Esc deselects the tool, then leaves.
* **Pick & Copy** — two thumbnail panes (Source / Working), filtered by Object Group. Select objects and *Copy Selected* / *Copy All* into the Working set.
* **Paint The Town** — the pixel editor for one object at its three sizes: pencil, brush, line, rectangle / ellipse (hollow or filled), fill, undo (60 steps), cut / copy / paste, rotate, flip, rename, revert, export BMP to `SCURKART/<spriteId>.BMP`, 16×16 palette with a transparent colour, grid / snap options.
* File menus: load Source / Working set, save / save as Working set, New working set.

## Architecture
* `src/scurk/` (no SDL): `MifFile`, `ObjectCatalog`, `ObjectSet` (overrides per object and zoom, names, revision counter, `applyTo(SpriteSet&)`).
* `src/app/scurk/editors.{h,cpp}`: `Env` (shared state), `Editor` interface, `ResourceEditors` host (menu bar from the PE resources, status line, file browser via `pickFile(callback)`, prompts).
* `pick_copy`, `paint`: editors drawn by the host. `place_print`: an adapter that forwards input to the application through `CityBridge` (set up in `main.cpp`), which switches `GameUi` into its city-edit mode (`setCityEdit`, `setPlaceItems`).
* Unit tests: `tests/test_scurk.cpp` (MIF round trip, stream encoder, catalogue, object sets).

## Not ported / unknown
Printing, SCURK's help file (SCURK.HLP), `TILES.DB`.
