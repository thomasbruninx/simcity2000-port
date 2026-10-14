// The original's menu bar (RT_MENU resource 3 of SIMCITY.EXE: File, Speed, Options, Disasters, Windows, Newspaper, Help), drawn at the top of the window.
// Commands are the original's WM_COMMAND ids; the owner decides what they do, which items are checked and which are available.
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <string>

#include "resources/pe_resources.h"

namespace sc2k {

class MenuBar {
public:
    static constexpr int kHeight = 18;
    void load(const PeResources::MenuItem& root) { root_ = root; }
    bool empty() const { return root_.items.empty(); }
    std::function<bool(int)> checked;   // command id -> shows a check mark
    std::function<bool(int)> enabled;   // command id -> selectable
    void draw(SDL_Renderer* r, int width);
    // Returns true when the click belongs to the menu (bar or open drop-down); `command` is set when an item was chosen (else 0).
    bool mouseDown(int x, int y, int& command);
    void mouseMove(int x, int y);
    bool isOpen() const { return open_ >= 0; }
    void close() { open_ = -1; sub_ = -1; }

private:
    PeResources::MenuItem root_;
    int open_ = -1, hover_ = -1, sub_ = -1, subHover_ = -1;   // sub_: the item of the open drop-down whose submenu is shown (File > Load >)
    static std::string clean(const std::string& s, std::string* shortcut = nullptr);
    int titleX(int i) const;
    int dropWidth(const PeResources::MenuItem& m) const;
    static int itemHeight(const PeResources::MenuItem& it) { return it.text.empty() ? 7 : 16; }
};

}  // namespace sc2k
