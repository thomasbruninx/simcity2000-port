// Title screen and the pickers behind it: the main menu (dialog 103 of the original: Start New City, Edit New Map, Load Scenario, Load Saved City, Quit),
// the scenario selection (dialog 145: list, picture, description) and a list of saved cities. The picture is BITMAPS/TITLESCR.BMP; scenario names, texts and
// thumbnails come from the .SCN files (CNAM, TEXT 0x80 / 0x81 and the 65x65 PICT chunk, see docs/ui.md).
#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "resources/bmp.h"
#include "resources/pe_resources.h"

namespace sc2k {

struct ScenarioEntry {
    std::string path, name, shortText, longText;
    std::vector<uint8_t> picture;         // 65 x 65 palette indices (empty = none)
};

class TitleScreen {
public:
    enum class Action { None, NewCity, NewMap, LoadScenario, LoadCity, Quit };
    TitleScreen(const std::string& gameDir, const PeResources& res, const std::array<uint32_t, 256>& gamePalette);
    ~TitleScreen();
    bool active = true;
    Action action = Action::None;
    std::string chosenPath;
    Action take(std::string* path = nullptr) { const Action a = action; if (path) *path = chosenPath; action = Action::None; return a; }

    void draw(SDL_Renderer* r, int w, int h);
    void mouseDown(int x, int y, int w, int h);
    void keyDown(SDL_Keycode k);
    void show() { active = true; mode_ = Mode::Menu; }
    const std::vector<ScenarioEntry>& scenarios() const { return scenarios_; }
    // scripting
    void openScenarioList() { mode_ = Mode::Scenarios; }
    void select(int index) { selected_ = index; }

private:
    enum class Mode { Menu, Scenarios, Cities };
    std::string dir_;
    const PeResources& res_;
    std::array<uint32_t, 256> palette_;
    Mode mode_ = Mode::Menu;
    SDL_Texture* titleTex_ = nullptr;
    int titleW_ = 0, titleH_ = 0;
    bool triedTitle_ = false;
    std::vector<ScenarioEntry> scenarios_;
    std::vector<std::string> cities_;
    int selected_ = 0;
    std::vector<SDL_Texture*> pictures_;
    std::vector<std::string> menuLabels_;
    void scan();
    void ensureTextures(SDL_Renderer* r);
    struct Rect { int x, y, w, h; bool in(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };
    Rect menuButton(int i, int w, int h) const;
    Rect button(int which, int w, int h) const;   // 0 select, 1 cancel
    void activateMenu(int i);
    void choose();
};

}  // namespace sc2k
