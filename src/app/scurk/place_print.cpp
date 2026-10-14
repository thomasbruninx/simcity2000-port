#include "app/scurk/place_print.h"

#include <algorithm>
#include <filesystem>

#include "app/ui_draw.h"

namespace sc2k::scurk {

namespace fs = std::filesystem;

namespace {

class PlacePrint : public Editor {
public:
    PlacePrint(Env& env, ResourceEditors& host, CityBridge& bridge) : env_(env), host_(host), b_(bridge) { if (b_.enter) b_.enter(); }

    int menuId() const override { return 200; }
    std::string title(const PeResources& r) const override { return r.string(29000); }
    bool drawsBackground() const override { return false; }
    void leave() override { if (b_.leave) b_.leave(); }

    void draw(SDL_Renderer* r, const ui::Rect& area) override {
        area_ = area;
        panel_ = {area.right() - kPanelW - 6, area.y + 30, kPanelW, std::max(100, area.h - 30 - 24)};
        if (b_.reserve) b_.reserve(panel_);
        drawSelector(r);
    }
    void mouseDown(int x, int y, int button) override {
        if (button == 1 && panel_.contains(x, y)) { clickSelector(x, y); return; }
        if (b_.mouseDown) b_.mouseDown(x, y, button);
    }
    void mouseUp(int x, int y, int button) override { if (b_.mouseUp) b_.mouseUp(x, y, button); }
    void mouseMove(int x, int y) override {
        mouseX_ = x; mouseY_ = y;
        hover_ = panel_.contains(x, y) ? cellAt(x, y) : -1;
        if (b_.mouseMove) b_.mouseMove(x, y);
    }
    void wheel(int x, int y, int dy) override {
        if (panel_.contains(mouseX_, mouseY_)) scroll_ = std::clamp(scroll_ - dy, 0, maxScroll());
        else if (b_.wheel) b_.wheel(dy);
        (void)x; (void)y;
    }
    bool key(SDL_Keycode k, bool ctrl) override { return b_.key && b_.key(k, ctrl); }

    bool command(int id) override {
        const std::string saves = env_.gameDir + "/CITIES";
        switch (id) {
            case 110:   // Load City
                host_.pickFile("Load City", saves, {}, {".sc2", ".scn"}, true, [this](const std::string& p) {
                    host_.setMessage(b_.loadCity && b_.loadCity(p) ? "City loaded: " + p : "Could not load " + p);
                });
                return true;
            case 120:   // Save City
                if (b_.cityPath && !b_.cityPath().empty()) { const std::string p = b_.cityPath(); host_.setMessage(b_.saveCity && b_.saveCity(p) ? "City saved: " + p : "Could not save " + p); return true; }
                [[fallthrough]];
            case 130: {  // Save City As
                std::string name = b_.cityName ? b_.cityName() : std::string("city");
                for (char& c : name) if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';
                host_.pickFile("Save City As", saves, name.empty() ? "city" : name, {".sc2", ".scn"}, false, [this](const std::string& p) {
                    host_.setMessage(b_.saveCity && b_.saveCity(p) ? "City saved: " + p : "Could not save " + p);
                });
                return true;
            }
            case 140:   // New City: a blank terrain
                if (b_.newCity) { b_.newCity(); host_.setMessage("A blank terrain is loaded"); }
                return true;
            case 123: { // Export City (small view only)
                host_.pickFile("Export City as BMP", env_.gameDir + "/SCURKART", "CITY", {".bmp"}, false, [this](const std::string& p) {
                    host_.setMessage(b_.exportCity && b_.exportCity(p) ? "City exported: " + p : "Export City works in the small view only (zoom out) - or the file could not be written");
                });
                return true;
            }
            case 26006: case 26007: host_.setMessage("Printing is not available in this port - use Export City"); return true;
            case 141: host_.newWorking(); if (b_.applyTileSet) b_.applyTileSet({}); return true;
            default:
                if (id >= 202 && id <= 212) { scroll_ = 0; if (b_.selectObject) b_.selectObject(-1); }   // the host sets the group; the selector shows it
                return false;
        }
    }

private:
    static constexpr int kPanelW = 232, kCellW = 74, kCellH = 70, kHeader = 24, kFooter = 34;
    ui::Rect area_, panel_;
    int scroll_ = 0, hover_ = -1, mouseX_ = 0, mouseY_ = 0;

    // The Object Selector: the objects of the Object Group, placeable ones only (the animated groups cannot be placed in Place & Print).
    std::vector<const ObjectInfo*> objects() const {
        std::vector<const ObjectInfo*> out;
        if (env_.group == ObjectGroup::Animated1 || env_.group == ObjectGroup::Animated2) return out;
        for (const ObjectInfo* o : env_.catalog.inGroup(env_.group)) if (o->group != ObjectGroup::Animated1 && o->group != ObjectGroup::Animated2) out.push_back(o);
        return out;
    }
    int cols() const { return std::max(1, (panel_.w - 8) / kCellW); }
    int rowsVisible() const { return std::max(1, (panel_.h - kHeader - kFooter) / kCellH); }
    int maxScroll() const { const int rows = (static_cast<int>(objects().size()) + cols() - 1) / cols(); return std::max(0, rows - rowsVisible()); }
    ui::Rect cell(int i) const { return {panel_.x + 4 + (i % cols()) * kCellW, panel_.y + kHeader + (i / cols() - scroll_) * kCellH, kCellW - 3, kCellH - 3}; }
    int cellAt(int x, int y) const {
        if (!panel_.contains(x, y) || y < panel_.y + kHeader || y >= panel_.bottom() - kFooter) return -1;
        const int col = (x - panel_.x - 4) / kCellW, row = (y - panel_.y - kHeader) / kCellH + scroll_;
        if (col < 0 || col >= cols()) return -1;
        const int i = row * cols() + col;
        return i < static_cast<int>(objects().size()) ? i : -1;
    }
    void clickSelector(int x, int y) {
        const int i = cellAt(x, y);
        if (i >= 0 && b_.selectObject) b_.selectObject(objects()[static_cast<size_t>(i)]->id);
    }
    void drawSelector(SDL_Renderer* r) {
        using namespace ui;
        const auto objs = objects();
        scroll_ = std::clamp(scroll_, 0, maxScroll());
        drawPanel(r, panel_, "Object Selector");
        const SDL_Rect clip{panel_.x + 1, panel_.y + kHeader, panel_.w - 2, panel_.h - kHeader - kFooter};
        SDL_SetRenderClipRect(r, &clip);
        const int selected = b_.selectedObject ? b_.selectedObject() : -1;
        for (size_t i = 0; i < objs.size(); ++i) {
            const Rect c = cell(static_cast<int>(i));
            if (c.bottom() < clip.y || c.y > clip.y + clip.h) continue;
            const bool sel = objs[i]->id == selected;
            fill(r, c, sel ? theme::kRowSelected : theme::kRow);
            frame(r, c, sel ? theme::kButtonBorder : static_cast<int>(i) == hover_ ? Color{120, 130, 170} : Color{50, 56, 76});
            const int id = objs[i]->id;
            const Sprite s = env_.working.sprite(id, 2, *env_.originals);
            const std::string key = "PP" + std::to_string(id) + ":" + std::to_string(env_.working.revision) + ":" + std::to_string(env_.working.customized(id, 2));
            env_.textures.drawFit(r, key, s, env_.palette, static_cast<float>(c.x + 3), static_cast<float>(c.y + 3), static_cast<float>(c.w - 6), static_cast<float>(c.h - 6));
        }
        SDL_SetRenderClipRect(r, nullptr);
        if (objs.empty()) wrappedText(r, Rect{panel_.x + 8, panel_.y + kHeader + 8, panel_.w - 16, 80}, "The animated groups cannot be placed in Place & Print. Pick another Object Group.", theme::kTextDim);
        const int shown = hover_ >= 0 ? hover_ : -1;
        std::string label;
        if (shown >= 0) label = env_.working.name(objs[static_cast<size_t>(shown)]->id, objs[static_cast<size_t>(shown)]->name);
        else if (selected >= 0) label = env_.working.name(selected, env_.catalog.find(selected) ? env_.catalog.find(selected)->name : std::string("Object"));
        else label = objectGroupName(env_.group);
        clippedText(r, panel_.x + 8, panel_.bottom() - kFooter + 10, panel_.w - 16, label, theme::kText);
    }
    Env& env_;
    ResourceEditors& host_;
    CityBridge& b_;
};

}  // namespace

std::unique_ptr<Editor> makePlacePrint(Env& env, ResourceEditors& host, CityBridge& bridge) { return std::make_unique<PlacePrint>(env, host, bridge); }

}  // namespace sc2k::scurk
