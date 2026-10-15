#include "app/scurk/pick_copy.h"

#include <algorithm>
#include <set>

#include "app/ui_draw.h"

namespace sc2k::scurk {

using namespace ui;

namespace {

class PickCopy : public Editor {
public:
    PickCopy(Env& env, ResourceEditors& host) : env_(env), host_(host) {}
    int menuId() const override { return 300; }
    std::string title(const PeResources& r) const override { return r.string(29001); }

    void draw(SDL_Renderer* r, const Rect& area) override {
        area_ = area;
        const Layout l = layout();
        drawPane(r, l.source, env_.source, "Source Object Set", env_.sourceName(), true, scrollSource_);
        drawPane(r, l.working, env_.working, "Working Object Set", env_.workingName() + (env_.working.dirty ? " *" : ""), false, scrollWorking_);
        for (const Button& b : buttons()) drawButton(r, b);
        clippedText(r, l.bar.x + 380, l.bar.y + 9, std::max(0, l.bar.w - 390), std::string(objectGroupName(env_.group)) + ": " + std::to_string(selected_.size()) + " selected", theme::kTextDim);
    }

    void mouseDown(int x, int y, int button) override {
        if (button != 1) return;
        const auto btns = buttons();
        if (btns[0].hit(x, y)) { command(310); return; }
        if (btns[1].hit(x, y)) { command(311); return; }
        const Layout l = layout();
        const int cell = cellAt(l.source, scrollSource_, x, y);
        if (cell >= 0) {
            const auto objs = env_.catalog.inGroup(env_.group);
            if (cell < static_cast<int>(objs.size())) { const int id = objs[static_cast<size_t>(cell)]->id; if (!selected_.erase(id)) selected_.insert(id); }
        }
    }

    void wheel(int x, int, int dy) override {
        const Layout l = layout();
        int& s = x < l.working.x ? scrollSource_ : scrollWorking_;
        s = std::clamp(s - dy, 0, maxScroll(l.source));
    }

    bool command(int id) override {
        if (id == 310) {   // Copy Selected
            if (selected_.empty()) { host_.setMessage("Select objects in the Source Object Set first"); return true; }
            const std::vector<int> ids(selected_.begin(), selected_.end());
            env_.prompt.open("Copy " + std::to_string(ids.size()) + " object(s) from the Source to the Working Object Set?", {"Copy", "Cancel"}, [this, ids](int c, const std::string&) {
                if (c != 0) return;
                for (int o : ids) env_.working.copyObject(env_.source, o);
                host_.setMessage("Copied " + std::to_string(ids.size()) + " object(s)");
                env_.beep(500);
                selected_.clear();
            });
            return true;
        }
        if (id == 311) {   // Copy All: the objects of the shown group ("Copy entire group?")
            const auto objs = env_.catalog.inGroup(env_.group);
            env_.prompt.open(std::string("Copy entire group? ") + objectGroupName(env_.group) + " (" + std::to_string(objs.size()) + " objects)", {"Copy", "Cancel"}, [this](int c, const std::string&) {
                if (c != 0) return;
                int n = 0;
                for (const ObjectInfo* o : env_.catalog.inGroup(env_.group)) { env_.working.copyObject(env_.source, o->id); ++n; }
                host_.setMessage("Copied " + std::to_string(n) + " object(s)");
                env_.beep(500);
            });
            return true;
        }
        return false;
    }

private:
    struct Layout { Rect source, working, bar; };
    static constexpr int kCellW = 92, kCellH = 82, kHeader = 40;
    Env& env_;
    ResourceEditors& host_;
    Rect area_;
    std::set<int> selected_;
    int scrollSource_ = 0, scrollWorking_ = 0;

    Layout layout() const {
        Layout l;
        Rect a = area_.inset(8, 4);
        l.bar = a.cutBottom(34);
        a.cutBottom(4);
        auto cols = columns(a, 2, 8);
        l.source = cols[0]; l.working = cols[1];
        return l;
    }
    int columnsOf(const Rect& pane) const { return std::max(1, (pane.w - 12) / kCellW); }
    int rowsVisible(const Rect& pane) const { return std::max(1, (pane.h - kHeader) / kCellH); }
    int maxScroll(const Rect& pane) const {
        const int n = static_cast<int>(env_.catalog.inGroup(env_.group).size());
        const int rows = (n + columnsOf(pane) - 1) / columnsOf(pane);
        return std::max(0, rows - rowsVisible(pane));
    }
    Rect cellRect(const Rect& pane, int index, int scroll) const {
        const int c = columnsOf(pane);
        return {pane.x + 6 + (index % c) * kCellW, pane.y + kHeader + (index / c - scroll) * kCellH, kCellW - 4, kCellH - 4};
    }
    int cellAt(const Rect& pane, int scroll, int x, int y) const {
        if (!pane.contains(x, y) || y < pane.y + kHeader) return -1;
        const int c = columnsOf(pane);
        const int col = (x - pane.x - 6) / kCellW, row = (y - pane.y - kHeader) / kCellH + scroll;
        if (col < 0 || col >= c) return -1;
        return row * c + col;
    }
    std::vector<Button> buttons() const {
        const Layout l = layout();
        return {Button{Rect{l.bar.x, l.bar.y + 4, 180, 26}, "Copy Selected"}, Button{Rect{l.bar.x + 190, l.bar.y + 4, 180, 26}, "Copy All (group)"}};
    }

    void drawPane(SDL_Renderer* r, const Rect& pane, ObjectSet& set, const std::string& title, const std::string& file, bool source, int scroll) {
        fill(r, pane, theme::kPanel);
        frame(r, pane, theme::kPanelBorder);
        clippedText(r, pane.x + 8, pane.y + 8, pane.w - 16, title, theme::kTitle);
        clippedText(r, pane.x + 8, pane.y + 22, pane.w - 16, file, theme::kTextDim);
        const auto objs = env_.catalog.inGroup(env_.group);
        const SDL_Rect clip{pane.x + 1, pane.y + kHeader, pane.w - 2, pane.h - kHeader - 1};
        SDL_SetRenderClipRect(r, &clip);
        const char* tag = source ? "S" : "W";
        for (size_t i = 0; i < objs.size(); ++i) {
            const Rect c = cellRect(pane, static_cast<int>(i), scroll);
            if (c.y + c.h < pane.y + kHeader || c.y > pane.bottom()) continue;
            const bool sel = source && selected_.count(objs[i]->id) != 0;
            fill(r, c, sel ? theme::kRowSelected : theme::kRow);
            frame(r, c, sel ? theme::kButtonBorder : Color{50, 56, 76});
            const int id = objs[i]->id;
            const Sprite s = set.sprite(id, 2, *env_.originals);
            const std::string key = std::string(tag) + std::to_string(reinterpret_cast<uintptr_t>(&set) & 0xFFFF) + ":" + std::to_string(id) + ":" + std::to_string(set.revision) + ":" + std::to_string(set.customized(id, 2));
            env_.textures.drawFit(r, key, s, env_.palette, static_cast<float>(c.x + 4), static_cast<float>(c.y + 4), static_cast<float>(c.w - 8), static_cast<float>(c.h - 22));
            clippedText(r, c.x + 4, c.bottom() - 14, c.w - 8, set.name(id, objs[i]->shortName), theme::kText);
            if (set.customized(id, 2)) text(r, c.right() - 12, c.y + 3, "*", Color{255, 220, 120});   // the set changes this object
        }
        SDL_SetRenderClipRect(r, nullptr);
        const int ms = maxScroll(pane);
        if (ms > 0) {
            const int th = pane.h - kHeader - 4;
            const int bh = std::max(14, th * rowsVisible(pane) / (rowsVisible(pane) + ms));
            fill(r, pane.right() - 8, pane.y + kHeader + (th - bh) * scroll / ms, 5, bh, 110, 120, 150);
        }
    }
};

}  // namespace

std::unique_ptr<Editor> makePickCopy(Env& env, ResourceEditors& host) { return std::make_unique<PickCopy>(env, host); }

}  // namespace sc2k::scurk
