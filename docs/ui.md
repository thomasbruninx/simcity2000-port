> Architecture, the widget toolkit and how to add windows: see [ui-architecture.md](ui-architecture.md). This page records what each window shows and where its behaviour comes from.

# The port's user interface (src/app)

`GameUi` (toolbar, tool state machine, modal questions), `BudgetWindow`, `InfoWindows`, `IconStrip`, `tool_catalog`. All text comes from the player's SIMCITY.EXE at runtime
(`PeResources`: strings, dialog templates, bitmaps); nothing is stored in the repository. Drawing uses SDL_Renderer with SDL's 8x8 debug font, so the whole UI also renders into a PNG
headlessly (`sc2k-app <dir> city.sc2 --screenshot out.png --do "..."`).

## Verified against the original
* tool behaviour (docs/simulation/tools.md), tool availability by year (tool-availability.md), city value and bonds (finance.md), budget settlement arithmetic.
* labels: budget rows and the 20 ordinance names come from dialogs 102 / 119; graph series names from dialog 152; query texts (the exe's strings 593–754 name every tile / building
  by id group — table `@0x4E5F70` + `0x42AE10` — and strings 812–827 are the query labels) follow the original's paint routine 0x427640 (traffic, zone name and density, altitude in feet,
  land value, crime / pollution levels, power, water; pump output, water-tower storage).
* graphs: the ported history (0x413910): 16 series x 52 samples = 12 monthly, 20 half-yearly, 20 five-yearly; scales per series group.

## The port's own (documented approximations)
* **Advisors** and the **newspaper** follow the original's code (docs/simulation/newspaper.md): advice selection and opinion poll are emulator-verified, the stories come from the original's text grammar in DATA/DATA_USA.DAT. Only the page layout (one layout instead of six), fonts, photographs and the advisors' titles are the port's own.
* **City window** lists statistics and shows the population window's three charts (residents, health, education by age class): the job-market arrays of the ported economy model are the original's age model (20 classes of 5 years: headcount, wage-like and education-like sums; the paint routine FUN_0046D4E2 reads exactly these), so there is no separate age simulation to port. Chart scaling and colours are the port's.
* Query: building-specific microsimulation dialogs (strings 910–982, e.g. "Employees : #1") and building names are implemented (docs/simulation/tools.md); N renames the queried building. The sign tool (0x44D140) is ported too. The menu's accelerators work: Ctrl+L / N / E / Z / S / X (load, new, edit map, scenario, save, exit), Ctrl+P and Ctrl+1..4 (speed), Ctrl+B / O / C / I / G / H / M (budget, ordinances, population, industry, graphs, neighbors, map); Ctrl+T (not in the original) returns to the title screen. M toggles the sound (docs/audio.md).
* **Title screen** (src/app/title_screen.*): BITMAPS/TITLESCR.BMP with the menu of dialog 103 (Start New City, Edit New Map, Load Scenario, Load Saved City, Quit); the scenario picker lists the .SCN files with the name (CNAM), the short and extended description (TEXT chunks 0x80 / 0x81) and the 65 x 65 picture (PICT chunk: 8-byte header, 65 rows of 66 bytes, indices into BITMAPS/PAL_MAC.BMP). Ctrl+T returns to the title screen. Layout and fonts are the port's.
* **Pause menu**: Escape (with no tool, menu or window to cancel) pauses the simulation and offers Resume, Save game (asks for a name; files go to `<settings dir>/saves/`, Ctrl+S does the same), Load game (the title screen's list, saved games first; Esc returns to the game), Settings and Exit.
* Bonds window shows each bond's rate and yearly cost, not the issue month.

## Right mouse button
Dragging with the right button scrolls the map; a plain right click opens the original's shortcut menu (resource 243: Center Map, Query Tile, Bulldoze Tile) on the tile under the pointer. A click on an item runs it, any other click or Esc closes the menu. It also works in Place & Print (Bulldoze Tile demolishes there).

## High DPI mode and zoom levels
* **High DPI mode** (title screen > Settings, top right; saved as `uiscale=` in settings.ini): x1, x1.5, x2, x4. The whole game (map and interface) is laid out in window pixels / scale and stretched to the window with nearest-neighbour scaling through SDL's logical presentation; mouse coordinates are divided by the scale. `--uiscale PCT` overrides it (screenshots).
* **Map zoom levels**: seven steps (`+`/`-`, wheel, top-bar buttons): the original's three sprite sets (8/16/32-pixel tiles), then the large set magnified 1.5x, 2x, 3x and 4x. The map is drawn into a framebuffer of window size / magnification and stretched, so picking, scrolling and the tile highlights work in that smaller space (`GameUi::mapPx`). Zooming keeps the tile in the middle of the view.

## Menu bar (RT_MENU 3 of SIMCITY.EXE)
`sc2k-resources SIMCITY.EXE menus` dumps the original's menu templates. The port draws menu 3 (File, Speed, Options, Disasters, Windows, Newspaper, Help; `src/app/menu_bar.*`) above its top bar and runs the original's WM_COMMAND handlers (message map at 0x4D2C48):
* File: Load City / Load Scenario open the title screen's pickers, New City, Edit New Map, Save (As), Exit; **Load Tile Set** is shown disabled (no alternative tile sets). Speed: the five speeds with the current one checked.
* Options: Auto-Budget (skips the yearly budget dialog and tune, 0x4736C0), Auto-Goto (the view follows disasters, read by the disaster handlers), Sound Effects, Music (separate switches in the mixer; the original keeps them in the profile).
* Disasters: Fire (kind 1, random focus within 20 tiles of the view), Flood 2, Air Crash 0x12, Tornado 7, Earthquake 6, Monster 8, Hurricane 0x10, Rioters 3; **No Disasters** toggles `cb4ec` (the flag the random disaster chooser checks; saved in the city file).
* Windows: Budget, Ordinances, Population (the city window), Graphs; Industry, Neighbors and Map are not ported yet and say so.
* Newspaper: Subscription (the paper opens itself in April and August, 0x412FE0) and Extra!!! (stories 3, 4, 5 and 0x24 pop the paper up; story 2 always does, 0x47BA90).
* Not shown: the hidden debug menu (resource 223: More Money, Add All Gifts, Melt Down, Volcano ...) and the help commands (no help file).

## Industry, Neighbors and City Map windows, demand bars, Debug menu
* **Demand bars**: the top bar shows the residential / commercial / industrial demand as bars (green, blue, yellow) above / below a zero line instead of numbers; the valves run from -2000 to +2000.
* **City Industry** (dialog 160, painter 0x471080; Windows > Industry, Ctrl+I): eleven sectors (strings 422..432) as blue bars with three modes: Ratios (`sec408`, scale starts at 70), Tax Rates (`sec3f8`, scale 30, values printed with %) and Demand (`sec940c`, scale 100). The scale grows with the data like the original's. Which array belongs to which button is read from the painter; the meaning of the names is STRONG INFERENCE.
* **Neighbors / SimNation** (dialog 157, painters 0x46BED2 / 0x46C4D0 / 0x46C790; Ctrl+H): a 204 x 160 canvas of the original's brown map (index 0x80) with the city in the middle and the four neighbours at the corners, turned with the view rotation. The blob picture comes from `BITMAPS/NEIGHBOR.BMP` (rows of 64 lines chosen by population: <2000, <10000, <50000, <100000, more), the names from strings 0x224..0x247 (`nbrName`), the populations are printed raw as `%ld`.
* **City Map** (dialog 246; Ctrl+M): 128 x 128 tiles at 3x with the 18 layers of the original's menu 185 (strings 327..344): structures, zones, roads, rail, traffic, power, water, population density, growth, crime, police, pollution, land value, fire; click to centre the main view; "Show City In Window" toggles the view marker. The colours are the port's own (the original uses palette indices); schools / colleges / police and fire departments show the structures or the coverage arrays.
* **Debug menu** (resource 223): enabled like in the game by typing the cheat word `priscilla` (also Ctrl+Shift+D): version info, More Money (+$500,000), Add All Gifts / Inventions (every dated event fires, the council adds $500,000), Melt Down, Microwave, Volcano, Fire Storm, Mass Riots, Major Flood, Toxic Spill, Graph Kludge. The original flags such cities as cheated; the port does not.

## Saving and renaming
File > Save City writes to the current file once there is one (after a Save As); otherwise, and for **Save City As**, the folder / file browser opens (`src/app/file_browser.*`): folders and `.sc2` / `.scn` files of a directory, click a folder to enter it (`..` goes up), click a file to take its name (to overwrite it) or type a new name, Enter / Save / Cancel / Esc. It starts in the saves folder next to the settings file. **File > Rename City...** (not in the original) changes the name used by the newspaper and the saved CNAM; the file name stays as it is.
