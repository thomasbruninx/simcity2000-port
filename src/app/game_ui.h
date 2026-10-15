// Interactive layer of the SDL application: toolbar, tool state machine (drag previews, costs, messages), modal questions raised by the
// ported tool code (bridge type, neighbour connection), and the HUD. Rendering of the map itself stays in MapRenderer; this class adds
// highlights to the software framebuffer and draws panels with an SDL_Renderer.
#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <functional>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/budget_window.h"
#include "app/icons.h"
#include "app/file_browser.h"
#include "ui/geometry.h"
#include "ui/widgets.h"
#include "app/info_windows.h"
#include "app/menu_bar.h"
#include "app/tool_catalog.h"
#include "audio/audio.h"
#include "render/map_renderer.h"
#include "resources/pe_resources.h"
#include "save/city.h"
#include "simulation/availability.h"
#include "simulation/clock.h"
#include "simulation/query.h"
#include "simulation/simulation.h"
#include "simulation/terrain_gen.h"

namespace sc2k {

class GameUi : public ZoneHooks {
public:
    GameUi(Simulation& sim, MapRenderer& renderer, Camera& cam, Clock& clock, const PeResources& resources, CityFile base);

    // Runs one iteration of the application's main loop while a modal question is open; returns false when the application should quit.
    std::function<bool()> pump;
    std::function<void()> onTitle;     // Ctrl+O: back to the title screen
    std::function<void(int)> onPauseAction;   // pause menu / menu bar: 1 load game, 2 settings, 3 exit, 4 load scenario
    static constexpr int kZoomLevelCount = 7;
    void setZoomLevel(int level);                       // 0..6, see game_ui.cpp
    bool pickScreen(int x, int y, int& a, int& b) { return renderer_.pickTile(sim_.world(), cam_, mapPx(x), mapPx(y), a, b); }   // screen position -> file tile
    int zoomLevel() const { return zoomLevel_; }
    float mapMag() const { return mag_; }                // magnification of the map framebuffer on screen
    int mapW() const { return std::max(1, static_cast<int>(std::ceil(width_ / mag_))); }   // size of the map framebuffer
    int mapH() const { return std::max(1, static_cast<int>(std::ceil(height_ / mag_))); }
    bool paused() const { return pauseOpen_ || mapEdit_ || cityEdit_ || budget_.open || info_.open != InfoWindows::Which::None; }   // the pause menu and the city / graphs / advisors / newspaper windows stop the game
    static std::string savesDir();     // where saved games go (next to the settings file)
    bool headless = false;          // modal questions are answered with their first option
    RenderLayers layers;
    Mixer* audio = nullptr;            // sound effects / music (optional)

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
    void newCity(int era, int difficulty, const TerrainParams* terrain = nullptr);             // start a new city on the default map (scripting)
    void newCityDialog(bool randomMap = false);
    void enterMapEditor();                              // File > Edit New Map: the terrain generator panel (Hills / Water / Trees, Coast, River, MAKE, DONE) and the terrain tools
    void showTestModal(const std::string& title, bool field) { testModal_ = Modal{}; testModal_.title = title; testModal_.textInput = field; testModal_.text = "A long city name"; testModal_.options = {"Yes", "No"}; modal_ = &testModal_; }   // screenshots
    void showBrowser() { browser_.open("Save City As", savesDir(), "city"); }   // screenshots
    bool inputCaptured() const { return browser_.active() || modal_ || pauseOpen_ || budget_.open || info_.open != InfoWindows::Which::None || menu_.isOpen(); }   // dialogs swallow the keyboard (no camera panning)
    bool mapEditing() const { return mapEdit_; }
    // Place & Print (SCURK): the city is edited in this view - paused, free, SCURK's own toolbox and view buttons; the object selector belongs to the editor.
    void setCityEdit(bool on);
    bool cityEditing() const { return cityEdit_; }
    void setPlaceObject(int id) { placeSel_ = id; if (id >= 0) { group_ = item_ = -1; menuOpen_ = false; } }   // -1: use the toolbox tool
    int placeObject() const { return placeSel_; }
    void setReserved(ui::Rect r) { reserved_ = r; }        // area covered by the object selector (the map ignores clicks there)
    bool smallView() const { return cam_.zoom == 0; }
    bool clearSelection() { const bool had = group_ >= 0 || placeSel_ >= 0 || menuOpen_; group_ = item_ = -1; menuOpen_ = false; placeSel_ = -1; return had; }   // Esc in the city editor
    void loadCity(const CityFile& f);                  // a city / scenario was loaded into the simulation
    const std::string& cityPath() const { return savePath_; }
    std::string cityName() const { return info_.cityName; }
    void resetCityPath() { savePath_.clear(); }
    std::string defaultMapPath;
    void setCityName(const std::string& n) { info_.cityName = n; }
    void initNews(const std::string& dir) { info_.initNews(dir); }                        // DEFAULT.SC2 of the game directory

    // ZoneHooks
    bool confirm(int stringId, int amount = 0) override;
    int chooseBridge(int length, int allowedMask) override;
    void toolMessage(int id) override;
    void postMessage(int id, int arg) override;
    void newsAge() override { info_.age(); }
    void showDialog(int stringId) override;
    const uint8_t* labelTemplate(int id, bool custom) override;   // default building names (strings 831..888) for new microsim buildings
    StadiumChoice chooseStadiumTeam(int takenMask) override;
    void terrainProgress(int stage) override;                // 0x414E50: shows the generator's current step
    void scrollTo(int a, int b) override;                    // 0x440C90: centre the view on a disaster (only with Options > Auto-Goto)
    void centerView(int a, int b) { cam_.centerOn(a, b, mapW() / 2, mapH() / 2); }
    void gameOver(int kind) override;                        // scenario won / lost, bankruptcy
    void playSound(int id, int repeat = 0) override;
    void stopSound(int id) override;
    void yearlyBudget() override;
    uint32_t tickCount() override { return SDL_GetTicks(); }                        // id < 0: everything (0x424B10)
    void playMusic(int id) override;
    void scheduledEvent(int index) override;                 // a dated event fired: unlock tools

private:
    using Rect = ui::Rect;   // src/ui/geometry.h
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
    Modal testModal_;
    BudgetWindow budget_;
    InfoWindows info_;
    MenuBar menu_;
    bool autoBudget_ = false, autoGoto_ = true, soundOn_ = true, musicOn_ = true;   // Options menu (the original keeps them in the profile; defaults = the menu's check marks)
    int lastMonth_ = -1;
    bool mapEdit_ = false, cityEdit_ = false;
    int editTreasury_ = 0;                                 // the city's own funds while Place & Print shows an unlimited amount
    Rect reserved_{};
    std::vector<ToolGroup> editCatalog_;                   // Place & Print's toolbox, built from the game's catalogue
    const std::vector<ToolGroup>& cat() const { return cityEdit_ ? editCatalog_ : toolCatalog(); }
    void demolishAt(int a, int b);
    void rotateView(int dir);
    int placeSel_ = -1;                                    // object id selected in the Place Object list (-1 = use the tool)
    void placeObjectAt(int a, int b);
    TerrainParams editParams_;
    int sliderDrag_ = -1;
    std::string progressText_;
    Rect editPanel() const { return {width_ - 236, kTopBarPx + 8, 228, 276}; }
    static constexpr int kTopBarPx = 46;
    // dialogs laid out from the widget toolkit (src/ui): the same functions give the rectangles for drawing and for hit testing
    struct ModalLayout { Rect panel, title, field; std::vector<ui::Button> buttons; };
    ModalLayout modalLayout() const;
    std::vector<ui::Button> pauseButtons() const;
    struct EditorWidgets { Rect panel; ui::Slider slider[3]; ui::Checkbox coast, river; ui::Button make, done; };
    EditorWidgets editorWidgets() const;
    void drawMapEditor(SDL_Renderer* r);
    bool mapEditorMouse(int x, int y, bool press);
    void makeTerrain();
    void finishMapEditor();
    bool groupAllowed(size_t g) const;
    bool debugMenu_ = false;           // the hidden Debug menu (resource 223), enabled by typing the cheat word
    std::string cheatTyped_;
    void buildMenu();
    void setCityNameAll(const std::string& name);
    void enableDebugMenu();
    void debugCommand(int id);
    void menuCommand(int id);
    bool menuChecked(int id) const;
    bool menuEnabled(int id) const;
    void requestDisaster(int kind, int a, int b);
    int zoomLevel_ = 2;
    float mag_ = 1.f, scrollRemX_ = 0, scrollRemY_ = 0;
    int mapPx(int v) const { return static_cast<int>(std::floor(v / mag_)); }
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
    bool pauseOpen_ = false;
    void pauseClick(int i);
    void drawPause(SDL_Renderer* r);
    void saveDialog();                                   // Save City: straight to the current file once there is one
    void saveAs();                                       // Save City As: the folder / file browser
    FileBrowser browser_;
    std::string savePath_;
    bool askText(const std::string& title, std::string& text);   // modal single-line text entry; false = cancelled
    bool queryOpen_ = false;
    QueryInfo query_;
    MicrosimInfo micro_;                // building-specific data of the queried tile (valid = a microsim building)
    std::array<uint8_t, 25> templateName_{};
    void renameQueried();
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
