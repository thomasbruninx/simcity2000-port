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
* Query: building-specific microsimulation dialogs (strings 910–982, e.g. "Employees : #1") and building names are implemented (docs/simulation/tools.md); N renames the queried building. The sign tool (0x44D140) is ported too. Ctrl+N starts a new city (docs/simulation/new-city.md), M toggles the sound (docs/audio.md).
* **Title screen** (src/app/title_screen.*): BITMAPS/TITLESCR.BMP with the menu of dialog 103 (Start New City, Edit New Map, Load Scenario, Load Saved City, Quit); the scenario picker lists the .SCN files with the name (CNAM), the short and extended description (TEXT chunks 0x80 / 0x81) and the 65 x 65 picture (PICT chunk: 8-byte header, 65 rows of 66 bytes, indices into BITMAPS/PAL_MAC.BMP). Ctrl+O returns to the title screen. Layout and fonts are the port's.
* **Pause menu**: Escape (with no tool, menu or window to cancel) pauses the simulation and offers Resume, Save game (asks for a name; files go to `<settings dir>/saves/`, Ctrl+S does the same), Load game (the title screen's list, saved games first; Esc returns to the game), Settings and Exit.
* Bonds window shows each bond's rate and yearly cost, not the issue month.
