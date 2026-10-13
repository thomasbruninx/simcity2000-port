#include "app/menu_bar.h"

#include <algorithm>

#include "app/ui_draw.h"

namespace sc2k {

using namespace ui;

std::string MenuBar::clean(const std::string& s, std::string* shortcut) {
    std::string t = s, sc;
    const size_t tab = t.find('\t');
    if (tab != std::string::npos) { sc = t.substr(tab + 1); t.resize(tab); }
    t.erase(std::remove(t.begin(), t.end(), '&'), t.end());
    if (shortcut) *shortcut = sc;
    return t;
}

int MenuBar::titleX(int i) const {
    int x = 4;
    for (int k = 0; k < i; ++k) x += static_cast<int>(clean(root_.items[static_cast<size_t>(k)].text).size()) * 8 + 16;
    return x;
}

int MenuBar::dropWidth(const PeResources::MenuItem& m) const {
    size_t w = 8;
    for (const auto& it : m.items) {
        std::string sc;
        const std::string t = clean(it.text, &sc);
        w = std::max(w, t.size() + (sc.empty() ? 0 : sc.size() + 3));
    }
    return static_cast<int>(w) * 8 + 40;
}

void MenuBar::draw(SDL_Renderer* r, int width) {
    fill(r, 0, 0, width, kHeight, 44, 50, 66);
    fill(r, 0, kHeight - 1, width, 1, 12, 14, 20);
    for (size_t i = 0; i < root_.items.size(); ++i) {
        const int x = titleX(static_cast<int>(i));
        const std::string t = clean(root_.items[i].text);
        const bool sel = static_cast<int>(i) == open_;
        if (sel) fill(r, x - 6, 0, static_cast<int>(t.size()) * 8 + 12, kHeight - 1, 70, 100, 150);
        text(r, x, 5, t, 240, 240, 240);
    }
    if (open_ < 0) return;
    const auto& menu = root_.items[static_cast<size_t>(open_)];
    const int x0 = titleX(open_) - 6, w = dropWidth(menu);
    int h = 4;
    for (const auto& it : menu.items) h += itemHeight(it);
    fill(r, x0, kHeight, w, h, 36, 40, 54);
    frame(r, x0, kHeight, w, h, 190, 200, 230);
    int y = kHeight + 2;
    for (size_t i = 0; i < menu.items.size(); ++i) {
        const auto& it = menu.items[i];
        if (it.text.empty()) { fill(r, x0 + 4, y + 3, w - 8, 1, 110, 120, 150); y += itemHeight(it); continue; }
        const bool on = !enabled || enabled(it.id);
        if (static_cast<int>(i) == hover_ && on) fill(r, x0 + 2, y, w - 4, 16, 70, 100, 150);
        std::string sc;
        const std::string t = clean(it.text, &sc);
        const Uint8 c = on ? 240 : 120;
        if (checked && checked(it.id)) text(r, x0 + 8, y + 4, "*", c, c, 160);
        text(r, x0 + 24, y + 4, t, c, c, c);
        if (!sc.empty()) text(r, x0 + w - 8 - static_cast<int>(sc.size()) * 8, y + 4, sc, c * 3 / 4, c * 3 / 4, c);
        y += 16;
    }
}

bool MenuBar::mouseDown(int x, int y, int& command) {
    command = 0;
    if (y >= 0 && y < kHeight) {   // the bar
        for (size_t i = 0; i < root_.items.size(); ++i) {
            const int tx = titleX(static_cast<int>(i));
            if (x >= tx - 6 && x < tx + static_cast<int>(clean(root_.items[i].text).size()) * 8 + 6) { open_ = open_ == static_cast<int>(i) ? -1 : static_cast<int>(i); hover_ = -1; return true; }
        }
        open_ = -1;
        return x >= 0;
    }
    if (open_ < 0) return false;
    const auto& menu = root_.items[static_cast<size_t>(open_)];
    const int x0 = titleX(open_) - 6, w = dropWidth(menu);
    int yy = kHeight + 2;
    for (const auto& it : menu.items) {
        const int h = itemHeight(it);
        if (x >= x0 && x < x0 + w && y >= yy && y < yy + h) {
            if (!it.text.empty() && (!enabled || enabled(it.id))) { command = it.id; open_ = -1; }
            return true;
        }
        yy += h;
    }
    open_ = -1;   // a click elsewhere closes the menu and is swallowed
    return true;
}

void MenuBar::mouseMove(int x, int y) {
    if (open_ < 0) return;
    const auto& menu = root_.items[static_cast<size_t>(open_)];
    const int x0 = titleX(open_) - 6, w = dropWidth(menu);
    if (y >= 0 && y < kHeight) {   // sliding over another title switches menus
        for (size_t i = 0; i < root_.items.size(); ++i) {
            const int tx = titleX(static_cast<int>(i));
            if (x >= tx - 6 && x < tx + static_cast<int>(clean(root_.items[i].text).size()) * 8 + 6) { open_ = static_cast<int>(i); hover_ = -1; return; }
        }
    }
    hover_ = -1;
    int yy = kHeight + 2;
    for (size_t i = 0; i < menu.items.size(); ++i) {
        const int h = itemHeight(menu.items[i]);
        if (x >= x0 && x < x0 + w && y >= yy && y < yy + h) hover_ = static_cast<int>(i);
        yy += h;
    }
}

}  // namespace sc2k
