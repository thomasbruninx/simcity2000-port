#include "app/scurk/place_print.h"

#include <filesystem>

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

    void draw(SDL_Renderer* r, const ui::Rect&) override { if (b_.draw) b_.draw(r); }
    void mouseDown(int x, int y, int button) override { if (b_.mouseDown) b_.mouseDown(x, y, button); }
    void mouseUp(int x, int y, int button) override { if (b_.mouseUp) b_.mouseUp(x, y, button); }
    void mouseMove(int x, int y) override { if (b_.mouseMove) b_.mouseMove(x, y); }
    void wheel(int, int, int dy) override { if (b_.wheel) b_.wheel(dy); }
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
                    host_.setMessage(b_.exportCity && b_.exportCity(p) ? "City exported: " + p : "Could not export " + p);
                });
                return true;
            }
            case 26006: case 26007: host_.setMessage("Printing is not available in this port - use Export City"); return true;
            case 141: host_.newWorking(); if (b_.applyTileSet) b_.applyTileSet({}); return true;
            default:
                if (id >= 202 && id <= 212) { if (b_.placeGroupChanged) b_.placeGroupChanged(id); return false; }   // the host sets the group, then the map view refreshes its list
                return false;
        }
    }

private:
    Env& env_;
    ResourceEditors& host_;
    CityBridge& b_;
};

}  // namespace

std::unique_ptr<Editor> makePlacePrint(Env& env, ResourceEditors& host, CityBridge& bridge) { return std::make_unique<PlacePrint>(env, host, bridge); }

}  // namespace sc2k::scurk
