# User interface: architecture and usage

The interface is the port's own code on top of **SDL3** (window, input, `SDL_Renderer`). Nothing here comes from the original executable except the *content* it shows (texts, icons, bitmaps, dialog and menu resources, which are read from the player's own game files at run time). The visual design follows the original where the original is known (layouts, labels, fonts) and is the port's own elsewhere.

## Layers
```
src/ui/          toolkit, no SDL, unit-tested (tests/test_ui.cpp)
  geometry.h       Rect (cut / inset / split helpers), rows(), columns(), centered()
  text_layout.*    Measure, wrapText(), ellipsize(), ellipsizeStart()
  theme.h          every colour of the interface in one place
  widgets.*        Button, Checkbox, Slider, ListBox, TextField: state + event logic
src/app/ui_draw.h  SDL drawing: fill / frame / text primitives and drawPanel, drawButton,
                   drawCheckbox, drawSlider, drawListBox, drawTextField, wrappedText, clippedText
src/app/         the windows and screens built from the above
  game_ui.*        the in-game interface: top bar, toolbar, tool menus, pause menu, modal questions,
                   map editor panel, query popup, message line; routes all input
  menu_bar.*       the original's menu bar (RT_MENU 3) with drop-downs
  info_windows.*   tabbed window: graphs, city, advisors, newspaper (+ city_windows.cpp: industry,
                   neighbors, city map)
  budget_window.*  budget / ordinances / bonds
  file_browser.*   "Save City As" browser
  title_screen.*   title screen, scenario / city pickers, settings
  ft_text.*        FreeType text (Times New Roman) for the newspaper and neighbours
```
Dependencies only point downwards: `app` uses `ui`; `ui` knows nothing about SDL, the simulation or the game files.

## The widget rules
* A widget is a **plain struct**: its rectangle, its state, and small methods that take events and say whether they used them (`press`, `click`, `hit`, `drag`, `wheel`, `textInput`). It never draws.
* **Layout is recomputed from the window size** by one function per window (for example `GameUi::modalLayout()`, `FileBrowser::layout()`, `GameUi::editorWidgets()`): the same rectangles serve drawing and hit testing, so they cannot drift apart. Use `Rect::cutTop/cutLeft/...`, `rows()`, `columns()` and `centered()` instead of hard-coded pixel sums.
* **Text never leaves its box.** Use `wrappedText()` (word wrap, long words are cut, extra lines end with "...") for sentences, `clippedText()` / `ellipsize()` for one-liners, `ellipsizeStart()` for paths. `wrappedHeight()` tells a dialog how tall it must be so that the box grows with the text. Widths are measured with a `Measure` function; the default is the 8-pixel SDL debug font (8 px per character), pass a FreeType measure for proportional text.
* Colours come from `ui/theme.h`.

### Quick reference
| Widget | State | Events |
|---|---|---|
| `Button` | `r`, `label`, `enabled`, `toggled` | `hit(x, y)` |
| `Checkbox` | `r`, `label`, `checked` | `click(x, y)` toggles and returns true when hit |
| `Slider` | `r`, `min`, `max`, `value`, `dragging` | `press`, `drag(x)`, `release()`, `knobX()` |
| `ListBox` | `r`, `rowH`, `items`, `selected`, `scroll` | `press`, `itemAt`, `wheel`, `ensureVisible`, `scrollBar()` |
| `TextField` | `r`, `text`, `maxLen`, `accept` (character filter) | `textInput(utf8)`, `backspace()` |

### Example: a small dialog
```cpp
// layout (shared by draw and mouse handling)
Rect panel = ui::centered(Rect{0, 0, w, h}, 420, 160);
Rect in = panel.inset(16);
ui::TextField name; name.r = in.cutTop(22);
in.cutTop(10);
auto cols = ui::columns(in.cutTop(26), 2, 12);
ui::Button ok{cols[0], "OK"}, cancel{cols[1], "Cancel"};

// draw
drawPanel(r, panel, "Name your city");
drawTextField(r, name);
drawButton(r, ok); drawButton(r, cancel);

// events
if (ok.hit(x, y)) accept();
name.textInput(utf8);              // from SDL_EVENT_TEXT_INPUT
```

## Input routing (`GameUi`)
Events from `main.cpp` reach `GameUi::mouseDown / mouseMove / mouseUp / wheel / keyDown / textInput`. The first consumer wins, top-most first:
1. the **file browser** (`browser_`), 2. the **pause menu**, 3. a **modal question** (`modal_`: option buttons, optional text field), 4. the **menu bar**, 5. the **budget window**, 6. the **information windows** (`InfoWindows::mouseDown`), 7. the **map editor panel**, 8. the toolbar and bottom bar, 9. the map (tools, query, scrolling).

While anything from 1-6 is open the keyboard is captured: `GameUi::inputCaptured()` stops the WASD / arrow-key camera scrolling, `keyDown` returns before any hotkey, and the game is paused (`GameUi::paused()`). Blocking questions (`askText`, `runModal`, `saveAs`) run a nested loop through `GameUi::pump`, which performs one iteration of the application's main loop; headless runs (`--screenshot`) answer them with their defaults.

## Coordinates and scaling
All UI coordinates are **logical pixels**. The window is drawn through `SDL_SetRenderLogicalPresentation`: High DPI mode (settings: x1.5 / x2 / x4) simply makes the logical size smaller than the window, so every widget scales by itself; mouse positions are divided by the scale in `main.cpp`. The map is rendered into its own framebuffer of `mapW() x mapH()` (logical size divided by the map magnification); `GameUi::mapPx()` converts a UI position to map pixels. Never mix the two spaces: UI code uses logical pixels, tile picking uses `mapPx`.

## Adding a window or dialog
* **A question**: fill a `Modal` (`title`, `options`, `textInput`) and call `runModal(m)`; the title wraps and the dialog grows. For text use `askText(title, text)`.
* **An information window**: add a `Which` value and a `draw...` / click branch in `info_windows.cpp` (the tab strip is built by `tabButtons()`); keep layout in a helper that returns widgets, as `industryButtons()` does.
* **A new top-level screen**: a class with `draw(SDL_Renderer*, w, h)`, `mouseDown`, `key`, `textInput`, `wheel`, a layout function and widget members, like `FileBrowser`; route it first in `GameUi`'s input functions and draw it last.
* Text from the original game is already wrapped by the helpers; never print a string of unknown length with the bare `text()` primitive.

## Testing the interface
* `tests/test_ui.cpp` covers wrapping, ellipsizing, the layout helpers and all widgets.
* `sc2k-app <dir> <city> --size W H --uiscale PCT --screenshot out.png --do "..."` renders the real UI headlessly. Useful commands: `window N` (1 graphs, 2 city, 3 advisors, 4 newspaper, 5 industry, 6 neighbors, 7 map), `budget [row]`, `press X Y` (a left click in logical coordinates), `key NAME`, `modal TEXT` / `modal field TEXT` (shows a question without blocking), `browser`, `mapedit`, `tile A B`, `pick X Y`. Screenshots at several `--uiscale` values are the quickest check that nothing overflows.
