#include "ui/widgets.h"

namespace sc2k::ui {

int Slider::valueAt(int x) const {
    if (r.w <= 0 || max <= min) return min;
    const int v = min + (x - r.x) * (max - min) / r.w;
    return std::clamp(v, min, max);
}
bool Slider::press(int x, int y) {
    if (!r.contains(x, y)) return false;
    dragging = true;
    value = valueAt(x);
    return true;
}
bool Slider::drag(int x) {
    if (!dragging) return false;
    value = valueAt(x);
    return true;
}
int Slider::knobX() const { return max <= min ? r.x : r.x + (value - min) * r.w / (max - min); }

int ListBox::itemAt(int x, int y) const {
    if (!r.contains(x, y)) return -1;
    const int k = scroll + (y - r.y) / rowH;
    return k >= 0 && k < static_cast<int>(items.size()) ? k : -1;
}
bool ListBox::press(int x, int y) {
    if (!r.contains(x, y)) return false;
    const int k = itemAt(x, y);
    if (k >= 0) selected = k;
    return true;
}
void ListBox::wheel(int steps) { scroll = std::clamp(scroll - steps, 0, maxScroll()); }
void ListBox::ensureVisible(int index) {
    if (index < scroll) scroll = index;
    else if (index >= scroll + visibleRows()) scroll = index - visibleRows() + 1;
    scroll = std::clamp(scroll, 0, maxScroll());
}
Rect ListBox::scrollBar() const {
    if (maxScroll() == 0) return {};
    const int th = visibleRows() * rowH;
    const int bh = std::max(12, th * visibleRows() / static_cast<int>(items.size()));
    return {r.right() - 6, r.y + (th - bh) * scroll / maxScroll(), 6, bh};
}

void TextField::textInput(const std::string& utf8) {
    for (char ch : utf8)
        if (accept(ch) && text.size() < maxLen) text.push_back(ch);
}

}  // namespace sc2k::ui
