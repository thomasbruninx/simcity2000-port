// SDL_Renderer drawing helpers shared by the UI windows: primitives (rectangles, SDL's 8x8 debug font) and the drawing of the widgets of src/ui
// (docs/ui-architecture.md). The widgets keep the state and the event logic; this file only paints them.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "ui/geometry.h"
#include "ui/text_layout.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace sc2k::ui {

inline void fill(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca = 255) {
    SDL_SetRenderDrawBlendMode(r, ca < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(r, &f);
}
inline void frame(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderRect(r, &f);
}
inline void text(SDL_Renderer* r, int x, int y, const std::string& s, Uint8 cr = 235, Uint8 cg = 235, Uint8 cb = 235) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, static_cast<float>(x), static_cast<float>(y), s.c_str());
}
inline void fill(SDL_Renderer* r, const Rect& q, Color c, Uint8 a = 255) { fill(r, q.x, q.y, q.w, q.h, c.r, c.g, c.b, a); }
inline void frame(SDL_Renderer* r, const Rect& q, Color c) { frame(r, q.x, q.y, q.w, q.h, c.r, c.g, c.b); }
inline void text(SDL_Renderer* r, int x, int y, const std::string& s, Color c) { text(r, x, y, s, c.r, c.g, c.b); }

// Text that never leaves its box: wrapped inside `box` (lines beyond the height are dropped with a trailing "..."). Returns the pixel height of the lines drawn.
inline int wrappedText(SDL_Renderer* r, const Rect& box, const std::string& s, Color c, int lineGap = 4) {
    const auto lines = wrapText(s, box.w);
    const int lh = kDebugLineH + lineGap;
    const int fit = std::max(1, box.h > 0 ? (box.h + lineGap) / lh : static_cast<int>(lines.size()));
    int y = box.y;
    for (int i = 0; i < static_cast<int>(lines.size()) && i < fit; ++i, y += lh) {
        std::string l = lines[static_cast<size_t>(i)];
        if (i == fit - 1 && static_cast<int>(lines.size()) > fit) l = ellipsize(l + "...", box.w);
        text(r, box.x, y, l, c);
    }
    return y - box.y;
}
// Height that `s` needs when wrapped to `width` pixels.
inline int wrappedHeight(const std::string& s, int width, int lineGap = 4) { return static_cast<int>(wrapText(s, width).size()) * (kDebugLineH + lineGap) - lineGap; }
// A single line cut with "..." when it is too wide for `maxWidth`.
inline void clippedText(SDL_Renderer* r, int x, int y, int maxWidth, const std::string& s, Color c) { text(r, x, y, ellipsize(s, maxWidth), c); }

inline void drawPanel(SDL_Renderer* r, const Rect& q, const std::string& title = {}) {
    fill(r, q, theme::kPanel);
    frame(r, q, theme::kPanelBorder);
    if (!title.empty()) clippedText(r, q.x + 12, q.y + 10, q.w - 24, title, theme::kTitle);
}
inline void drawButton(SDL_Renderer* r, const Button& b, Color base = theme::kButton) {
    fill(r, b.r, !b.enabled ? theme::kButtonDisabled : b.toggled ? theme::kButtonOn : base);
    frame(r, b.r, theme::kButtonBorder);
    const std::string t = ellipsize(b.label, std::max(8, b.r.w - 8));
    const int tw = debugWidth(t);
    text(r, b.r.x + (b.r.w - tw) / 2, b.r.y + (b.r.h - kDebugLineH) / 2, t, b.enabled ? theme::kButtonText : theme::kTextDim);
}
inline void drawCheckbox(SDL_Renderer* r, const Checkbox& c) {
    const Rect box{c.r.x, c.r.y + (c.r.h - 12) / 2, 12, 12};
    frame(r, box, theme::kButtonBorder);
    if (c.checked) fill(r, box.inset(3), Color{120, 230, 120});
    clippedText(r, c.r.x + 18, c.r.y + (c.r.h - kDebugLineH) / 2, c.r.w - 18, c.label, theme::kText);
}
inline void drawSlider(SDL_Renderer* r, const Slider& s) {
    fill(r, s.r.x, s.r.y + s.r.h / 2 - 2, s.r.w, 4, theme::kTrack.r, theme::kTrack.g, theme::kTrack.b);
    fill(r, s.knobX() - 3, s.r.y + s.r.h / 2 - 7, 7, 14, theme::kKnob.r, theme::kKnob.g, theme::kKnob.b);
}
inline void drawListBox(SDL_Renderer* r, const ListBox& l) {
    fill(r, l.r, theme::kField);
    for (int i = 0; i < l.visibleRows(); ++i) {
        const int k = l.scroll + i;
        if (k >= static_cast<int>(l.items.size())) break;
        const Rect row = l.rowRect(k);
        if (k == l.selected) fill(r, row, theme::kRowSelected);
        clippedText(r, row.x + 8, row.y + (l.rowH - kDebugLineH) / 2, row.w - 20, l.items[static_cast<size_t>(k)], theme::kText);
    }
    const Rect bar = l.scrollBar();
    if (bar.h > 0) fill(r, bar, Color{110, 120, 150});
}
inline void drawTextField(SDL_Renderer* r, const TextField& t, bool focused = true) {
    fill(r, t.r, theme::kField);
    const int room = std::max(8, t.r.w - 12);
    std::string shown = t.text + (focused ? "_" : "");
    shown = ellipsizeStart(shown, room);   // a long entry scrolls: the end (where the caret is) stays visible
    if (t.selected && !t.text.empty()) fill(r, t.r.x + 4, t.r.y + 3, std::min(t.r.w - 8, debugWidth(shown) + 4), t.r.h - 6, theme::kRowSelected.r, theme::kRowSelected.g, theme::kRowSelected.b);
    text(r, t.r.x + 6, t.r.y + (t.r.h - kDebugLineH) / 2, shown, theme::kFieldText);
}

inline std::string money(long long v) {
    std::string digits = std::to_string(v < 0 ? -v : v), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return (v < 0 ? "-$" : "$") + out;
}

}  // namespace sc2k::ui
