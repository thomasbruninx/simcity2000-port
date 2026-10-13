// Interactive layer of the SDL application: toolbar, tool state machine (drag previews, costs, messages), modal questions raised by the
// ported tool code (bridge type, neighbour connection), and the HUD. Rendering of the map itself stays in MapRenderer; this class adds
// highlights to the software framebuffer and draws panels with an SDL_Renderer.
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "app/tool_catalog.h"
#include "render/map_renderer.h"
#include "resources/pe_resources.h"
#include "save/city.h"
#include "simulation/clock.h"
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
    void update(int dtMs);

    void overlay(Framebuffer& fb);                     // tile highlights / path previews on the map framebuffer
    void draw(SDL_Renderer* r);                        // panels, texts

    // scripting / tests
    bool selectTool(const std::string& name);
    bool openMenu(const std::string& groupName);       // shows the item list of a group (screenshots)
    void dragTool(int a0, int b0, int a1, int b1);     // press on (a0,b0), release on (a1,b1) with the selected tool
    const std::string& message() const { return message_; }
    bool saveCity(const std::string& path);

    // ZoneHooks
    bool confirm(int stringId, int amount = 0) override;
    int chooseBridge(int length, int allowedMask) override;
    void toolMessage(int id) override;
    void postMessage(int id, int arg) override;
    void showDialog(int stringId) override;

private:
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };
    struct Modal { std::string title; std::vector<std::string> options; int result = -2; };

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

    const ToolItem* tool() const;
    int unitCost(const ToolItem& t) const;
    void say(const std::string& text);
    void bulldozeAt(int a, int b);
    void terrainAt(int a, int b);
    int levelTarget_ = 0;           // altitude the level tool flattens to (that of the tile where the drag began)
    void updatePreview();
    void finishDrag();
    void applyClick(int a, int b);
    void queryTile(int a, int b);
    std::pair<int, int> footprint(const ToolItem& t, int a, int b, int& size) const;
    int runModal(Modal& m);
    void drawModal(SDL_Renderer* r);
    Rect groupRect(int i) const;
    Rect itemRect(int i) const;
    bool overUi(int x, int y) const;
    std::string moneyText(int v) const;
};

}  // namespace sc2k
