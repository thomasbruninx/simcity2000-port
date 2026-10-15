# Resource Editors (SCURK port)

Main menu → **Resource Editors** opens a submenu with SCURK's three utilities. They share one **Source** and one **Working** object set (`.MIF`, see [formats/mif.md](formats/mif.md)) and use SCURK's own menus, read from WINSCURK.EXE (menu ids 200/300/400/500, strings 29000+). The menu bar offers *File / Edit / Options / Object Group / Window / Help* as in the original.

## Usage
* **Place & Print** — the city editor, modelled on SCURK's help (SCURK.HLP). The city is shown in the game's own map view, paused and without any game constraints (no costs, power, funds or simulation). Window parts:
  * *Toolbox* (left): **Bulldozer** (Demolish/Clear — removes objects without leaving rubble, terrain and zoning stay — Level, Raise, Lower terrain, De-zone), **Water** (Pond/Lake/River, Pipes), **Zones** (the nine zone types), **Roads** (Road, Highway, Tunnel, Onramp), **Rail**, **Power** lines, **Center** tool.
  * *Top buttons*: rotate counter-clockwise / clockwise, zoom out / in, centre; the tile in the middle of the view stays there.
  * *View buttons* (bottom): Underground, Buildings, Infrastructure (roads, rails, power lines, trees and other ground objects), Signs, Zones (zoned buildings and the zone colours).
  * *Object Selector* (right): thumbnails of the objects of the chosen **Object Group** from the Working set (names on hover). The animated groups cannot be placed. Choose an object, then click the map; the footprint is highlighted. Objects replace whatever stands on their tiles and ignore slopes and water.
  * File: Load City, Save / Save As, New City (blank terrain from DEFAULT.SC2), Export City (BMP of the whole map; small view only, as in SCURK). Printing is not implemented. Esc deselects the object / tool, then leaves.
* **Pick & Copy** — two thumbnail panes (Source / Working), filtered by Object Group. Select objects and *Copy Selected* / *Copy All* into the Working set.
* **Paint The Town** — the pixel editor for one object at its three sizes: pencil, brush, line, rectangle / ellipse (hollow or filled), fill, undo (60 steps), cut / copy / paste, rotate, flip, rename, revert, export BMP to `SCURKART/<spriteId>.BMP`, 16×16 palette with a transparent colour, grid / snap options.
* File menus: load Source / Working set, save / save as Working set, New working set.

## Windows, options, sound
* **Window** menu: the three utilities (the original's MDI window list). Switching keeps the working set and Place & Print's city; Pick & Copy / Paint The Town start fresh on the shared sets.
* **Options > Cycle Colors / Increment Cycle** also govern the colour animation of Place & Print's map; **Sound** switches the sound effects (placing an object plays the "built" / "refused" sounds, copying objects the "built" sound) and is restored to the game's own setting when you leave.
* Text fields that open with a name filled in (rename object, save as, the new-city name) have it selected: typing replaces it, Backspace clears it.
* Exercised with scripted input (copy objects from FUTURE.MIF, save, New, load; draw, undo, rename in Paint The Town, export BMP, save, reload): the graphics hash of the object and its name come back identical after the file round trip.

## Architecture
* `src/scurk/` (no SDL): `MifFile`, `ObjectCatalog`, `ObjectSet` (overrides per object and zoom, names, revision counter, `applyTo(SpriteSet&)`).
* `src/app/scurk/editors.{h,cpp}`: `Env` (shared state), `Editor` interface, `ResourceEditors` host (menu bar from the PE resources, status line, file browser via `pickFile(callback)`, prompts).
* `pick_copy`, `paint`: editors drawn by the host. `place_print`: an adapter that forwards input to the application through `CityBridge` (set up in `main.cpp`), which switches `GameUi` into its city-edit mode (`setCityEdit`, `setPlaceItems`).
* Unit tests: `tests/test_scurk.cpp` (MIF round trip, stream encoder, catalogue, object sets).

## Not ported / unknown
Printing, SCURK's help file (SCURK.HLP), `TILES.DB`.
