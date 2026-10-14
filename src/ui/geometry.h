// Rectangles and small layout helpers shared by all widgets (no SDL here, so the logic is unit-testable).
#pragma once
#include <algorithm>
#include <vector>

namespace sc2k::ui {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    Rect inset(int d) const { return {x + d, y + d, std::max(0, w - 2 * d), std::max(0, h - 2 * d)}; }
    Rect inset(int dx, int dy) const { return {x + dx, y + dy, std::max(0, w - 2 * dx), std::max(0, h - 2 * dy)}; }
    Rect moved(int dx, int dy) const { return {x + dx, y + dy, w, h}; }
    // Cuts a strip off one side and returns it; this rectangle shrinks accordingly.
    Rect cutTop(int hh) { hh = std::min(hh, h); const Rect r{x, y, w, hh}; y += hh; h -= hh; return r; }
    Rect cutBottom(int hh) { hh = std::min(hh, h); const Rect r{x, y + h - hh, w, hh}; h -= hh; return r; }
    Rect cutLeft(int ww) { ww = std::min(ww, w); const Rect r{x, y, ww, h}; x += ww; w -= ww; return r; }
    Rect cutRight(int ww) { ww = std::min(ww, w); const Rect r{x + w - ww, y, ww, h}; w -= ww; return r; }
};

// `n` equal rows (gap pixels apart) inside `r`.
inline std::vector<Rect> rows(Rect r, int n, int gap = 0) {
    std::vector<Rect> out;
    if (n <= 0) return out;
    const int each = std::max(0, (r.h - gap * (n - 1)) / n);
    for (int i = 0; i < n; ++i) out.push_back({r.x, r.y + i * (each + gap), r.w, each});
    return out;
}
// `n` equal columns inside `r`.
inline std::vector<Rect> columns(Rect r, int n, int gap = 0) {
    std::vector<Rect> out;
    if (n <= 0) return out;
    const int each = std::max(0, (r.w - gap * (n - 1)) / n);
    for (int i = 0; i < n; ++i) out.push_back({r.x + i * (each + gap), r.y, each, r.h});
    return out;
}
// A rectangle of the given size centred in `outer`.
inline Rect centered(Rect outer, int w, int h) { return {outer.x + (outer.w - w) / 2, outer.y + (outer.h - h) / 2, w, h}; }

}  // namespace sc2k::ui
