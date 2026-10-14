#include "app/scurk/editors.h"

#include <filesystem>

#include "app/scurk/pick_copy.h"
#include "app/scurk/paint.h"
#include "app/scurk/place_print.h"
#include "app/ui_draw.h"
#include "save/error.h"

namespace sc2k::scurk {

using namespace ui;
namespace fs = std::filesystem;

std::string Env::sourceName() const { return source.path().empty() ? "Untitled" : fs::path(source.path()).filename().string(); }
std::string Env::workingName() const { return working.path().empty() ? "Untitled" : fs::path(working.path()).filename().string(); }

ResourceEditors::ResourceEditors(const std::string& gameDir, const std::array<uint32_t, 256>& palette, SpriteSet& sprites, const GameTables* tables) : sprites_(sprites) {
    env_.gameDir = gameDir;
    env_.palette = palette;
    if (tables) env_.cycler = std::make_unique<PaletteCycler>(palette, *tables);
    env_.originals = &sprites;
    try {
        exe_ = std::make_unique<PeImage>(PeImage::load(gameDir + "/WINSCURK.EXE"));
        res_ = std::make_unique<PeResources>(*exe_);
        const std::string original = gameDir + "/SCURKART/ORIGINAL.MIF";
        env_.source = ObjectSet::load(original);
        env_.working = ObjectSet::load(original);
        env_.working.setPath({});          // "Untitled" until it is saved
        env_.catalog = ObjectCatalog::build(env_.source.mif(), *res_);
    } catch (const std::exception& e) {
        loadError = std::string("The resource editors need WINSCURK.EXE and SCURKART/ORIGINAL.MIF of the game folder: ") + e.what();
        res_.reset();
    }
}

ResourceEditors::~ResourceEditors() = default;

void ResourceEditors::open(Which which) {
    if (!res_) return;
    which_ = which;
    active = true;
    switch (which) {
        case Which::PickAndCopy: editor_ = makePickCopy(env_, *this); break;
        case Which::PaintTheTown: editor_ = makePaint(env_, *this); break;
        case Which::PlaceAndPrint: if (city.enter) editor_ = makePlacePrint(env_, *this, city); break;
    }
    if (!editor_) { active = false; return; }
    const auto it = res_->menus().find("#" + std::to_string(editor_->menuId()));
    if (it != res_->menus().end()) menu_.load(it->second);
    menu_.checked = [this](int id) { return menuChecked(id) || (editor_ && editor_->checked(id)); };
    menu_.enabled = [this](int id) { return !editor_ || editor_->enabled(id); };
    env_.message.clear();
}

void ResourceEditors::close() { if (editor_) editor_->leave(); active = false; editor_.reset(); if (onExit) onExit(); }

bool ResourceEditors::menuChecked(int id) const {
    if (id >= 202 && id <= 212) {
        static const ObjectGroup byId[11] = {ObjectGroup::Residential, ObjectGroup::Commercial, ObjectGroup::Industrial, ObjectGroup::Special, ObjectGroup::Power, ObjectGroup::TransportMilitary,
                                             ObjectGroup::Misc, ObjectGroup::Animated1, ObjectGroup::Animated2, ObjectGroup::Construction, ObjectGroup::All};
        return env_.group == byId[id - 202];
    }
    if (id == 514) return env_.colorCycling;
    if (id == 516) return env_.sound;
    return false;
}

void ResourceEditors::menuCommand(int id) {
    static const ObjectGroup byId[11] = {ObjectGroup::Residential, ObjectGroup::Commercial, ObjectGroup::Industrial, ObjectGroup::Special, ObjectGroup::Power, ObjectGroup::TransportMilitary,
                                         ObjectGroup::Misc, ObjectGroup::Animated1, ObjectGroup::Animated2, ObjectGroup::Construction, ObjectGroup::All};
    if (editor_ && editor_->command(id)) return;
    switch (id) {
        case 111: loadSet(false); break;
        case 112: loadSet(true); break;
        case 121: saveWorking(false); break;
        case 131: saveWorking(true); break;
        case 141: newWorking(); break;
        case 24310: requestExit(); break;
        case 514: env_.colorCycling = !env_.colorCycling; break;
        case 515: if (env_.cycler) { env_.cycler->update(140); env_.cycler->update(500); } break;   // Increment Cycle: one step by hand
        case 516: env_.sound = !env_.sound; break;
        case 2009: env_.prompt.open(res_->string(29003) + " - ported into the SimCity 2000 port (resource editors)", {"OK"}, [](int, const std::string&) {}); break;
        case 24341: case 24345: setMessage("The SCURK help file is not available in this port"); break;
        default:
            if (id >= 202 && id <= 212) env_.group = byId[id - 202];
            else setMessage("That command is not available in this editor");
            break;
    }
}

void ResourceEditors::pickFile(const std::string& title, const std::string& dir, const std::string& name, std::vector<std::string> exts, bool openMode, std::function<void(const std::string&)> done) {
    browserDone_ = std::move(done);
    env_.browser.open(title, dir, name, std::move(exts), openMode);
}

void ResourceEditors::loadSet(bool working) {
    pickFile(working ? "Load Working Object Set" : "Load Source Object Set", env_.gameDir + "/SCURKART", {}, {".mif"}, true, [this, working](const std::string& path) {
        try {
            if (working) { env_.working = ObjectSet::load(path); setMessage("Working Object Set: " + path); if (city.applyTileSet) city.applyTileSet(path); }
            else { env_.source = ObjectSet::load(path); setMessage("Source Object Set: " + path); }
        } catch (const std::exception&) { setMessage(res_->string(29009)); }   // "Not a valid *.MIF file"
    });
}

void ResourceEditors::saveWorking(bool saveAs) {
    if (!saveAs && !env_.working.path().empty()) { doSave(env_.working.path()); return; }
    const std::string name = env_.working.path().empty() ? "MYSET" : fs::path(env_.working.path()).stem().string();
    pickFile("Save Working Object Set As", env_.working.path().empty() ? env_.gameDir + "/SCURKART" : fs::path(env_.working.path()).parent_path().string(), name, {".mif"}, false,
             [this](const std::string& path) { doSave(path); });
}

void ResourceEditors::doSave(const std::string& path) {
    try {
        env_.working.mif().save(path);
        env_.working.setPath(path);
        env_.working.dirty = false;
        setMessage("Object Set saved as: " + path);
    } catch (const std::exception& e) { setMessage(std::string("Could not save: ") + e.what()); }
}

void ResourceEditors::newWorking() {
    auto go = [this] {
        try {
            env_.working = ObjectSet::load(env_.gameDir + "/SCURKART/ORIGINAL.MIF");   // "a clean copy of the original graphics"
            env_.working.setPath({});
            setMessage("A clean copy of the original graphics is the new working set");
        } catch (const std::exception& e) { setMessage(e.what()); }
    };
    if (env_.working.dirty) env_.prompt.open(res_->string(29005), {"Yes", "No"}, [this, go](int c, const std::string&) { if (c == 0) saveWorking(false); go(); });
    else go();
}

void ResourceEditors::requestExit() {
    if (env_.working.dirty) env_.prompt.open(res_->string(29005), {"Yes", "No", "Cancel"}, [this](int c, const std::string&) {
        if (c == 0) { if (env_.working.path().empty()) { saveWorking(true); return; } doSave(env_.working.path()); close(); }
        else if (c == 1) { env_.working.dirty = false; close(); }
    });
    else close();
}

void ResourceEditors::finishBrowser() {
    if (!browserDone_ || env_.browser.active()) return;
    auto cb = std::move(browserDone_);
    browserDone_ = nullptr;
    if (env_.browser.result() == 0) cb(env_.browser.path());
}

void ResourceEditors::update(int dtMs) {
    finishBrowser();
    if (env_.colorCycling && env_.cycler) env_.cycler->update(dtMs);
}

void ResourceEditors::draw(SDL_Renderer* r, int w, int h) {
    width_ = w; height_ = h;
    finishBrowser();
    if (!res_ || !editor_) { fill(r, 0, 0, w, h, 16, 20, 30); text(r, 40, 60, loadError, theme::kTextWarn); return; }
    if (editor_->drawsBackground()) fill(r, 0, 0, w, h, 16, 20, 30);
    editor_->draw(r, area());
    menu_.draw(r, w);
    // status line
    fill(r, 0, h - 20, w, 20, 20, 24, 32);
    clippedText(r, 8, h - 14, w - 16, env_.message, Color{190, 220, 160});
    env_.prompt.draw(r, w, h);
    env_.browser.draw(r, w, h);
}

void ResourceEditors::mouseDown(int x, int y, int button, int w, int h) {
    width_ = w; height_ = h;
    if (env_.browser.active()) { if (button == 1) env_.browser.mouseDown(x, y, w, h); return; }
    if (env_.prompt.active()) { if (button == 1) env_.prompt.mouseDown(x, y, w, h); return; }
    if (button == 1) {
        int cmd = 0;
        if (menu_.mouseDown(x, y, cmd)) { if (cmd) menuCommand(cmd); return; }
    }
    if (editor_) editor_->mouseDown(x, y, button);
}

void ResourceEditors::mouseUp(int x, int y, int button) { if (editor_ && !env_.browser.active() && !env_.prompt.active()) editor_->mouseUp(x, y, button); }

void ResourceEditors::mouseMove(int x, int y) {
    menu_.mouseMove(x, y);
    if (editor_ && !env_.browser.active() && !env_.prompt.active()) editor_->mouseMove(x, y);
}

void ResourceEditors::wheel(int x, int y, int dy) {
    if (env_.browser.active()) { env_.browser.wheel(dy); return; }
    if (editor_ && !env_.prompt.active()) editor_->wheel(x, y, dy);
}

void ResourceEditors::keyDown(SDL_Keycode k, bool ctrl) {
    if (env_.browser.active()) { env_.browser.key(k); return; }
    if (env_.prompt.active()) { env_.prompt.key(k); return; }
    if (editor_ && editor_->key(k, ctrl)) return;
    if (k == SDLK_ESCAPE) { if (menu_.isOpen()) menu_.close(); else requestExit(); }
}

void ResourceEditors::textInput(const std::string& utf8) {
    if (env_.browser.active()) { env_.browser.textInput(utf8); return; }
    if (env_.prompt.active()) { env_.prompt.textInput(utf8); return; }
    if (editor_) editor_->textInput(utf8);
}

}  // namespace sc2k::scurk
