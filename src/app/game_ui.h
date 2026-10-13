// Interactive layer of the SDL application: toolbar, tool state machine (drag previews, costs, messages), modal questions raised by the
// ported tool code (bridge type, neighbour connection), and the HUD. Rendering of the map itself stays in MapRenderer; this class adds
// highlights to the software framebuffer and draws panels with an SDL_Renderer.
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/budget_window.h"
#include "app/icons.h"
#include "app/info_windows.h"
#include "app/tool_catalog.h"
#include "render/map_renderer.h"
#include "resources/pe_resources.h"
#include "save/city.h"
#include "simulation/availability.h"
#include "simulation/clock.h"
#include "simulation/query.h"
#include "simulation/simulation.h"

namespace sc2k {

class GameUi : public ZoneHooks {
public:
    GameUi(Simulation& sim, MapRenderer& renderer, Camera& cam, Clock& clock, const PeResources& resources, CityFile base);

    // Runs one iteration of the application's main loop while a modal question is open; returns false when the application should quit.
    std::function<bool()> pump;
    bool headless = false;          // modal questions are answered with their first option
    RenderLayers layers;

    void resize(int w, int h) { width_ = w; height_ = h; }
    void mouseDown(int x, int y, int button);
    void mouseUp(int x, int y, int button);
    void mouseMove(int x, int y);
    void wheel(int x, int y, int dy);
    bool keyDown(SDL_Keycode key, bool ctrl);          // false = quit requested
    void textInput(const std::string& utf8);           // typed characters while a text question is open
    void update(int dtMs);
    void setShift(bool held) { ts_.shift = held; }       // keyboard modifier read by the tree tools

    void overlay(Framebuffer& fb);                     // tile highlights / path previews on the map framebuffer
    void draw(SDL_Renderer* r);                        // panels, texts

    // scripting / tests
    bool selectTool(const std::string& name);
    bool openMenu(const std::string& groupName);
    void openBudgetDetail(int row) { budget_.openDetail(row); }
    void openInfo(int which) { info_.open = static_cast<InfoWindows::Which>(which); }       // shows the item list of a group (screenshots)
    void dragTool(int a0, int b0, int a1, int b1);
    void signTool(int a, int b, const std::string& text) { placeSign(sim_.context(), a, b, text, false); }   // scripting     // press on (a0,b0), release on (a1,b1) with the selected tool
    const std::string& message() const { return message_; }
    bool saveCity(const std::string& path);
    void newCity(int era, int difficulty);             // start a new city on the default map (scripting)
    void newCityDialog();                              // asks for the starting year and the difficulty (the original dialog 101)
    std::string defaultMapPath;                        // DEFAULT.SC2 of the game directory

    // ZoneHooks
    bool confirm(int stringId, int amount = 0) override;
    int chooseBridge(int length, int allowedMask) override;
    void toolMessage(int id) override;
    void postMessage(int id, int arg) override;
    void showDialog(int stringId) override;
    void scheduledEvent(int index) override;                 // a dated event fired: unlock tools

private:
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };
    struct Modal { std::string title; std::vector<std::string> options; int result = -2; bool textInput = false; std::string text; };

    Simulation& sim_;
    MapRenderer& renderer_;
    Camera& cam_;
    Clock& clock_;
    const PeResources& res_;
    CityFile base_;
    ToolState ts_;
    int width_ = 1280, height_ = 800;

    int group_ = -1, item_ = -1;       // selected tool
    bool menuOpen_ = false;
    int hoverA_ = -1, hoverB_ = -1;
    int mouseX_ = 0, mouseY_ = 0;
    bool dragging_ = false, scrolling_ = false;
    int startA_ = 0, startB_ = 0, lastA_ = 0, lastB_ = 0;
    PathPreview preview_;
    int previewCost_ = 0;
    std::string message_;
    int messageMs_ = 0;
    Modal* modal_ = nullptr;
    BudgetWindow budget_;
    InfoWindows info_;
    ToolAvailability avail_;
    bool available(const ToolItem& t) const;
    std::unique_ptr<IconStrip> toolbarIcons_, terrainIcons_, bookIcon_;   // created on the first draw (need the renderer)
    void drawIcon(SDL_Renderer* r, int strip, int cell, int x, int y);

    const ToolItem* tool() const;
    int unitCost(const ToolItem& t) const;
    void say(const std::string& text);
    void bulldozeAt(int a, int b);
    void terrainAt(int a, int b);
    void landscapeAt(int a, int b);
    bool emergency_ = false;
    int levelTarget_ = 0;
    int editStartAlt_ = 0, editLastAlt_ = 0, editStartY_ = 0, editPrevA_ = -1, editPrevB_ = -1, sprayMs_ = 0;   // terrain-editor drags
    void editSample(int pa, int pb, int pointerY);
              // altitude the level tool flattens to (that of the tile where the drag began)
    void updatePreview();
    void finishDrag();
    void applyClick(int a, int b);
    void queryTile(int a, int b);
    void signAt(int a, int b);
    bool askText(const std::string& title, std::string& text);   // modal single-line text entry; false = cancelled
    bool queryOpen_ = false;
    QueryInfo query_;
    void drawQuery(SDL_Renderer* r);
    void drawSigns(SDL_Renderer* r);
    std::pair<int, int> footprint(const ToolItem& t, int a, int b, int& size) const;
    int runModal(Modal& m);
    void drawModal(SDL_Renderer* r);
    Rect groupRect(int i) const;
    Rect itemRect(int i) const;
    bool overUi(int x, int y) const;
    std::string moneyText(int v) const;
};

}  // namespace sc2k
