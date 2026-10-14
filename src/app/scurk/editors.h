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
    std::string sourceName() const;
    std::string workingName() const;
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
    virtual bool checked(int) const { return false; }
    virtual bool enabled(int) const { return true; }
};

class ResourceEditors {
public:
    enum class Which { PlaceAndPrint, PickAndCopy, PaintTheTown };
    ResourceEditors(const std::string& gameDir, const std::array<uint32_t, 256>& palette, SpriteSet& sprites);
    ~ResourceEditors();
    bool active = false;
    std::function<void()> onExit;                       // back to the main menu
    std::function<std::unique_ptr<Editor>(Env&, ResourceEditors&)> makeCityEditor;   // Place & Print is built by the application (it reuses the game's map tools)
    void open(Which which);
    void close();
    Env& env() { return env_; }
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
    int pendingBrowser_ = 0;          // 1 load source, 2 load working, 3 save working
    int width_ = 0, height_ = 0;
    ui::Rect area() const { return {0, MenuBar::kHeight, width_, height_ - MenuBar::kHeight - 20}; }   // the status line takes the bottom 20 pixels
    void menuCommand(int id);
    void finishBrowser();
    void doSave(const std::string& path);
    bool menuChecked(int id) const;
};

}  // namespace sc2k::scurk
