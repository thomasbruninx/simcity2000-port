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
* **Advisors**: the exe's advisor sentences (strings 294–326) and portraits (bitmaps 197–204) are used, and the simulation's advisor messages (ported `msg()` calls, queue of 6 as in 0x47BA90)
  fill the window, but the assignment message id -> sentence / advisor (e.g. 0x10 crime -> 301) and the advisor labels are the port's reading; the original's per-advisor advice
  functions live inside large MFC dialog handlers and were not recovered.
* **Newspaper**: paper name from strings 360–365, poll lines from strings 353 / 367–373 with percentages scaled from the history indicators; the original's token-based article
  generator (TEXT_USA.DAT, "VERB the ADJECTIVE NOUN") and the real approval rating are not ported.
* **City window** lists statistics; the original's population / health / education age graphs need the age model, which is not ported.
* Query: building-specific microsimulation dialogs (strings 910–982, e.g. "Employees : #1") and building names are implemented (docs/simulation/tools.md); N renames the queried building. The sign tool (0x44D140) is ported too. Ctrl+N starts a new city (docs/simulation/new-city.md), M toggles the sound (docs/audio.md).
* Bonds window shows each bond's rate and yearly cost, not the issue month.
