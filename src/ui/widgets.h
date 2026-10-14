// Widget state and event logic (no drawing, no SDL): the app draws them with the helpers of app/ui_draw.h. Every widget is a plain struct with a rectangle, its
// state and small methods that take mouse / keyboard events and say whether they were used, so the same code serves every window and can be unit-tested.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "ui/geometry.h"

namespace sc2k::ui {

struct Button {
    Rect r;
    std::string label;
    bool enabled = true, toggled = false;
    bool hit(int x, int y) const { return enabled && r.contains(x, y); }
};

struct Checkbox {
    Rect r;                 // the whole row: box and label
    std::string label;
    bool checked = false;
    bool click(int x, int y) { if (!r.contains(x, y)) return false; checked = !checked; return true; }
};

// An integer slider: press / drag set the value from the horizontal position inside `r`.
struct Slider {
    Rect r;
    int min = 0, max = 100, value = 0;
    bool dragging = false;
    int valueAt(int x) const;
    bool press(int x, int y);       // starts dragging when the press is inside; sets the value
    bool drag(int x);               // while dragging
    void release() { dragging = false; }
    int knobX() const;              // pixel position of the knob centre
};

// A scrolling list of text rows with one selected row.
struct ListBox {
    Rect r;
    int rowH = 18;
    std::vector<std::string> items;
    int selected = -1, scroll = 0;
    int visibleRows() const { return std::max(1, r.h / rowH); }
    int maxScroll() const { return std::max(0, static_cast<int>(items.size()) - visibleRows()); }
    int itemAt(int x, int y) const;                 // index of the row under the point or -1
    bool press(int x, int y);                       // selects the row; true when the press was inside
    void wheel(int steps);                          // positive scrolls up
    void ensureVisible(int index);
    Rect rowRect(int index) const { return {r.x, r.y + (index - scroll) * rowH, r.w, rowH}; }
    Rect scrollBar() const;                         // the thumb (empty when everything fits)
};

// A single-line text entry; characters are filtered, the length is limited.
struct TextField {
    Rect r;
    std::string text;
    size_t maxLen = 24;
    std::function<bool(char)> accept = [](char c) { return c >= 32 && c < 127; };
    void textInput(const std::string& utf8);
    bool backspace() { if (text.empty()) return false; text.pop_back(); return true; }
};

}  // namespace sc2k::ui
