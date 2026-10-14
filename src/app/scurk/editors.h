// The resource editors: a port of SCURK (the SimCity 2000 Urban Renewal Kit, WINSCURK.EXE) with its three utilities
//   Place & Print   - the city editor (place objects, terrain, zones, networks; export / print the city)
//   Pick & Copy     - browse a Source object set and copy objects into the Working set
//   Paint The Town  - the pixel editor of one object's graphics at the three zoom levels
// They share one Source and one Working object set (SCURK: "a working set for all three utilities") and use SCURK's own menus (read from WINSCURK.EXE).
#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "app/file_browser.h"
#include "app/menu_bar.h"
#include "app/scurk/prompt.h"
#include "app/scurk/sprite_view.h"
#include "render/map_renderer.h"
#include "resources/pe_resources.h"
#include "resources/sprite_archive.h"
#include "scurk/object_set.h"
#include "ui/geometry.h"

namespace sc2k::scurk {

// State shared by the editors.
struct Env {
    std::string gameDir;
    std::array<uint32_t, 256> palette{};
    SpriteSet* originals = nullptr;           // the game's graphics (the renderer's SpriteSet: Place & Print installs the working set as overrides)
    ObjectCatalog catalog;
    ObjectSet source, working;
    ObjectGroup group = ObjectGroup::All;     // the "Object Group" menu is shared by the utilities
    SpriteTextures textures;
    Prompt prompt;
    FileBrowser browser;
    std::string message;                      // status line
    bool colorCycling = true, sound = true;   // Options menu
    int cycle = 0;                            // colour cycle step
    std::unique_ptr<PaletteCycler> cycler;    // Options > Cycle Colors: the animated palette entries (water shimmer, blinking lights)
    const std::array<uint32_t, 256>& shownPalette() const { return colorCycling && cycler ? cycler->palette() : palette; }
    std::string sourceName() const;
    std::string workingName() const;
};

// Place & Print edits a city in the application's own map view; main.cpp supplies these functions (they call into the game's interface in its editing mode).
struct CityBridge {
    std::function<void()> enter, leave;
    std::function<void(SDL_Renderer*)> draw;                      // draws the map view and the city tools (below the host's menu bar and status line)
    std::function<void(int, int, int)> mouseDown, mouseUp;       // x, y, button
    std::function<void(int, int)> mouseMove;
    std::function<void(int)> wheel;
    std::function<bool(SDL_Keycode, bool)> key;                   // true when used
    std::function<bool(const std::string&)> loadCity, saveCity, exportCity;
    std::function<void()> newCity;                                // blank terrain
    std::function<std::string()> cityPath;                        // file of the city being edited ("" = none yet)
    std::function<std::string()> cityName;
    std::function<void(ui::Rect)> reserve;                        // the object selector's area (the map ignores clicks there)
    std::function<void(int)> selectObject;                        // object to place (-1 = none: the toolbox tool applies)
    std::function<int()> selectedObject;
    std::function<void(const std::string&)> applyTileSet;         // working set graphics changed (re-install the overrides)
};

class Editor {
public:
    virtual ~Editor() = default;
    virtual int menuId() const = 0;           // WINSCURK menu resource of the utility
    virtual std::string title(const PeResources& scurk) const = 0;
    virtual void draw(SDL_Renderer* r, const ui::Rect& area) = 0;
    virtual void mouseDown(int, int, int) {}
    virtual void mouseUp(int, int, int) {}
    virtual void mouseMove(int, int) {}
    virtual void wheel(int, int, int) {}
    virtual bool key(SDL_Keycode, bool) { return false; }
    virtual void textInput(const std::string&) {}
    virtual bool command(int) { return false; }
    virtual bool drawsBackground() const { return true; }   // false: the editor paints the whole area itself (the city view)
    virtual void leave() {}
    virtual bool checked(int) const { return false; }
    virtual bool enabled(int) const { return true; }
};

class ResourceEditors {
public:
    enum class Which { PlaceAndPrint, PickAndCopy, PaintTheTown };
    ResourceEditors(const std::string& gameDir, const std::array<uint32_t, 256>& palette, SpriteSet& sprites, const GameTables* tables = nullptr);
    ~ResourceEditors();
    bool active = false;
    std::function<void()> onExit;                       // back to the main menu
    CityBridge city;                                    // Place & Print: the application's map view (set by main.cpp)
    void open(Which which);
    void close();
    Env& env() { return env_; }
    bool drawsOwnBackground() const { return editor_ && !editor_->drawsBackground(); }   // Place & Print: the game's map view is drawn first
    const PeResources& scurkResources() const { return *res_; }
    bool loaded() const { return res_ != nullptr; }
    std::string loadError;

    void draw(SDL_Renderer* r, int w, int h);
    void mouseDown(int x, int y, int button, int w, int h);
    void mouseUp(int x, int y, int button);
    void mouseMove(int x, int y);
    void wheel(int x, int y, int dy);
    void keyDown(SDL_Keycode k, bool ctrl);
    void textInput(const std::string& utf8);
    void update(int dtMs);

    // file operations shared by the utilities (also called by the menus)
    // Opens the file browser; `done(path)` runs when a file was chosen.
    void pickFile(const std::string& title, const std::string& dir, const std::string& name, std::vector<std::string> exts, bool openMode, std::function<void(const std::string&)> done);
    void loadSet(bool working);
    void saveWorking(bool saveAs);
    void newWorking();
    void requestExit();
    void setMessage(const std::string& m) { env_.message = m; }
    void switchTo(Which w) { open(w); }
    Which current() const { return which_; }

private:
    std::unique_ptr<PeImage> exe_;
    std::unique_ptr<PeResources> res_;
    Env env_;
    SpriteSet& sprites_;
    std::unique_ptr<Editor> editor_;
    Which which_ = Which::PickAndCopy;
    MenuBar menu_;
    std::function<void(const std::string&)> browserDone_;   // runs with the chosen path when the file browser is accepted
    int width_ = 0, height_ = 0;
    ui::Rect area() const { return {0, MenuBar::kHeight, width_, height_ - MenuBar::kHeight - 20}; }   // the status line takes the bottom 20 pixels
    void menuCommand(int id);
    void finishBrowser();
    void doSave(const std::string& path);
    bool menuChecked(int id) const;
};

}  // namespace sc2k::scurk
