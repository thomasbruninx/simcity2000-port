#include "app/scurk/paint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "app/ui_draw.h"
#include "resources/bmp.h"

namespace sc2k::scurk {

using namespace ui;
namespace fs = std::filesystem;

namespace {

enum class Tool { Brush, Eraser, Eyedropper, Fill, Line, Square, Circle, Diamond, RightWall, LeftWall, Paste };
const char* kToolNames[] = {"Paintbrush", "Eraser", "Eyedropper", "Fill", "Line", "Square", "Circle", "Diamond", "Right Wall", "Left Wall", "Paste"};
using Pt = std::pair<int, int>;

// ---- shape rasterisers (points of the outline or of the filled area) ---------------------------------------------------------------------------------------
void lineSegments(std::vector<Pt>& out, int x0, int y0, int x1, int y1) {   // Bresenham
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        out.emplace_back(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void polygon(std::vector<Pt>& out, const std::vector<Pt>& v, bool filled) {
    if (!filled) { for (size_t i = 0; i < v.size(); ++i) lineSegments(out, v[i].first, v[i].second, v[(i + 1) % v.size()].first, v[(i + 1) % v.size()].second); return; }
    int top = v[0].second, bot = v[0].second;
    for (const Pt& p : v) { top = std::min(top, p.second); bot = std::max(bot, p.second); }
    for (int y = top; y <= bot; ++y) {
        std::vector<int> xs;
        for (size_t i = 0; i < v.size(); ++i) {
            const Pt a = v[i], b = v[(i + 1) % v.size()];
            if (a.second == b.second) { if (a.second == y) { xs.push_back(a.first); xs.push_back(b.first); } continue; }
            if ((y >= a.second && y <= b.second) || (y >= b.second && y <= a.second)) xs.push_back(a.first + (b.first - a.first) * (y - a.second) / (b.second - a.second));
        }
        if (xs.size() < 2) continue;
        const int lo = *std::min_element(xs.begin(), xs.end()), hi = *std::max_element(xs.begin(), xs.end());
        for (int x = lo; x <= hi; ++x) out.emplace_back(x, y);
    }
    polygon(out, v, false);
}
void ellipse(std::vector<Pt>& out, int x0, int y0, int x1, int y1, bool filled) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    const double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0, rx = std::max(0.5, (x1 - x0) / 2.0), ry = std::max(0.5, (y1 - y0) / 2.0);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const double d = ((x - cx) * (x - cx)) / (rx * rx) + ((y - cy) * (y - cy)) / (ry * ry);
            if (filled ? d <= 1.0 : (d <= 1.0 && d >= 1.0 - 1.6 / std::max(rx, ry) * 2)) out.emplace_back(x, y);
        }
}

std::vector<Pt> shapePoints(Tool t, int x0, int y0, int x1, int y1, bool filled) {
    std::vector<Pt> p;
    const int lx = std::min(x0, x1), hx = std::max(x0, x1), ly = std::min(y0, y1), hy = std::max(y0, y1);
    switch (t) {
        case Tool::Line: lineSegments(p, x0, y0, x1, y1); break;
        case Tool::Square: polygon(p, {{lx, ly}, {hx, ly}, {hx, hy}, {lx, hy}}, filled); break;
        case Tool::Circle: ellipse(p, x0, y0, x1, y1, filled); break;
        case Tool::Diamond: polygon(p, {{(lx + hx) / 2, ly}, {hx, (ly + hy) / 2}, {(lx + hx) / 2, hy}, {lx, (ly + hy) / 2}}, filled); break;
        case Tool::RightWall: {   // a wall face sloping down to the right (2:1), as tall as the drag minus the slope
            const int drop = (hx - lx) / 2, h = std::max(1, (hy - ly) - drop);
            polygon(p, {{lx, ly}, {hx, ly + drop}, {hx, ly + drop + h}, {lx, ly + h}}, filled);
            break;
        }
        case Tool::LeftWall: {
            const int drop = (hx - lx) / 2, h = std::max(1, (hy - ly) - drop);
            polygon(p, {{hx, ly}, {lx, ly + drop}, {lx, ly + drop + h}, {hx, ly + h}}, filled);
            break;
        }
        default: break;
    }
    return p;
}

class Paint : public Editor {
public:
    Paint(Env& env, ResourceEditors& host) : env_(env), host_(host) {
        const auto objs = env_.catalog.inGroup(env_.group);
        if (!objs.empty()) activate(objs.front()->id, 2);
    }
    int menuId() const override { return 400; }
    std::string title(const PeResources& r) const override { return r.string(29002); }

    // ---- drawing --------------------------------------------------------------------------------------------------------------------------------------
    void draw(SDL_Renderer* r, const Rect& area) override {
        area_ = area;
        rebuildListIfGroupChanged();
        const Layout l = layout();
        if (needFit_) { needFit_ = false; mag_ = std::max(1, std::min({16, std::max(1, l.canvas.w / std::max(1, img_.width)), std::max(1, l.canvas.h / std::max(1, img_.height))})); }
        drawTools(r, l);
        drawCanvas(r, l);
        drawSide(r, l);
        const std::string info = "Paint The Town - " + env_.working.name(id_, nameOf(id_)) + " (" + std::to_string(img_.width) + "x" + std::to_string(img_.height) + ")   " + (hover_.first >= 0 ? "x " + std::to_string(hover_.first) + " y " + std::to_string(hover_.second) : std::string());
        clippedText(r, l.canvas.x, l.canvas.y - 14, l.canvas.w, info, theme::kTitle);
    }

    void mouseDown(int x, int y, int button) override {
        if (button != 1) return;
        const Layout l = layout();
        for (size_t i = 0; i < toolButtons(l).size(); ++i) if (toolButtons(l)[i].hit(x, y)) { tool_ = static_cast<Tool>(i); return; }
        const auto act = actionButtons(l);
        if (act[0].hit(x, y)) { filled_ = !filled_; return; }
        if (act[1].hit(x, y)) { setMag(mag_ + 1); return; }
        if (act[2].hit(x, y)) { setMag(mag_ - 1); return; }
        for (int z = 0; z < 3; ++z) if (zoomButtons(l)[static_cast<size_t>(z)].hit(x, y)) { activate(id_, z); return; }
        if (pickColor(l, x, y)) return;
        if (list_.r.contains(x, y)) { if (list_.press(x, y) && list_.selected >= 0 && list_.selected < static_cast<int>(listIds_.size())) activate(listIds_[static_cast<size_t>(list_.selected)], zoom_); return; }
        if (l.canvas.contains(x, y)) beginStroke(x, y);
    }
    void mouseMove(int x, int y) override {
        const Layout l = layout();
        hover_ = pixelAt(l, x, y);
        if (drawing_) continueStroke(x, y);
    }
    void mouseUp(int x, int, int button) override { if (button == 1 && drawing_) endStroke(x, lastY_); }
    void wheel(int x, int, int dy) override {
        if (list_.r.contains(x, area_.y + 40)) list_.wheel(dy * 2);
        else setMag(mag_ + dy);
    }

    bool key(SDL_Keycode k, bool ctrl) override {
        if (ctrl && k == SDLK_Z) { command(410); return true; }
        return false;
    }

    bool command(int id) override {
        switch (id) {
            case 410: undo(); return true;
            case 411: clip_ = img_; hasClip_ = true; host_.setMessage("Copied the drawing to the buffer"); return true;
            case 412: if (hasClip_) { pushUndo(); pasteAt(0, 0); commit(); } return true;
            case 413: pushUndo(); std::fill(img_.opaque.begin(), img_.opaque.end(), 0); commit(); return true;
            case 414: pushUndo(); img_ = original_; commit(); return true;
            case 415: if (hasClip_) rotateClip(); return true;
            case 416: if (hasClip_) flipClip(true); return true;
            case 417: if (hasClip_) flipClip(false); return true;
            case 418:
                env_.prompt.open(host_.scurkResources().string(29008), {"OK", "Cancel"}, [this](int c, const std::string& t) { if (c == 0 && !t.empty()) env_.working.setName(id_, t); }, true, env_.working.name(id_, nameOf(id_)), 24);
                return true;
            case 419: env_.working.clearName(id_); return true;
            case 104: exportBmp(); return true;
            case 511: snap_ = !snap_; return true;
            case 512: grid_ = !grid_; return true;
            case 513: clipRegion_ = !clipRegion_; return true;
            case 510:
                env_.prompt.open("Grid Settings: size of a grid cell in pixels", {"2", "4", "8", "16"}, [this](int c, const std::string&) { static const int s[4] = {2, 4, 8, 16}; if (c >= 0) gridStep_ = s[c]; });
                return true;
            default: return false;
        }
    }
    bool checked(int id) const override { return (id == 511 && snap_) || (id == 512 && grid_) || (id == 513 && clipRegion_); }

private:
    struct Layout { Rect tools, canvas, side, palette, zoomRow; };
    Env& env_;
    ResourceEditors& host_;
    Rect area_;
    int id_ = 0, zoom_ = 2;
    Sprite img_, original_, clip_;
    bool hasClip_ = false;
    std::vector<Sprite> undo_;
    Tool tool_ = Tool::Brush;
    bool filled_ = false, grid_ = false, snap_ = false, clipRegion_ = true;
    int gridStep_ = 4;
    int color_ = 1;            // palette index; -1 = transparent
    int mag_ = 4;
    int scrollX_ = 0, scrollY_ = 0;
    bool drawing_ = false, needFit_ = true;
    Pt start_{0, 0}, cur_{0, 0}, hover_{-1, -1};
    int lastY_ = 0;
    ObjectGroup listGroup_ = ObjectGroup::Misc;
    bool listBuilt_ = false;
    ListBox list_;
    std::vector<int> listIds_;
    unsigned lastRev_ = 0;

    std::string nameOf(int id) const { const ObjectInfo* o = env_.catalog.find(id); return o ? o->name : "Object " + std::to_string(id); }

    void activate(int id, int zoom) {
        id_ = id; zoom_ = zoom;
        img_ = env_.working.sprite(id, zoom, *env_.originals);
        if (img_.width <= 0) { img_.width = zoom == 2 ? 32 : zoom == 1 ? 16 : 8; img_.height = img_.width / 2; img_.index.assign(static_cast<size_t>(img_.width * img_.height), 0); img_.opaque = img_.index; }
        original_ = img_;
        undo_.clear();
        needFit_ = true;
        scrollX_ = scrollY_ = 0;
        for (size_t i = 0; i < listIds_.size(); ++i) if (listIds_[i] == id) list_.selected = static_cast<int>(i);
    }
    void commit() { env_.working.setSprite(id_, zoom_, img_); }
    void pushUndo() { undo_.push_back(img_); if (undo_.size() > 60) undo_.erase(undo_.begin()); }
    void undo() { if (undo_.empty()) return; img_ = undo_.back(); undo_.pop_back(); commit(); }
    void setMag(int m) { mag_ = std::clamp(m, 1, 24); }

    Layout layout() const {
        Layout l;
        Rect a = area_.inset(6, 4);
        l.tools = a.cutLeft(116);
        a.cutLeft(6);
        l.side = a.cutRight(232);
        a.cutRight(6);
        a.cutTop(18);
        l.canvas = a;
        Rect s = l.side;
        l.zoomRow = s.cutTop(24);
        s.cutTop(4);
        l.palette = s.cutTop(16 * 13 + 4);
        return l;
    }
    std::vector<Button> toolButtons(const Layout& l) const {
        std::vector<Button> out;
        for (int i = 0; i < 11; ++i) { Button b; b.r = {l.tools.x, l.tools.y + i * 26, l.tools.w, 24}; b.label = kToolNames[i]; b.toggled = static_cast<int>(tool_) == i; out.push_back(b); }
        return out;
    }
    std::vector<Button> actionButtons(const Layout& l) const {
        const int y = l.tools.y + 11 * 26 + 8;
        return {Button{Rect{l.tools.x, y, l.tools.w, 24}, filled_ ? "Filled" : "Hollow", true, false}, Button{Rect{l.tools.x, y + 26, l.tools.w / 2 - 2, 24}, "Zoom +"}, Button{Rect{l.tools.x + l.tools.w / 2 + 2, y + 26, l.tools.w / 2 - 2, 24}, "Zoom -"}};
    }
    std::vector<Button> zoomButtons(const Layout& l) const {
        auto cols = columns(l.zoomRow, 3, 4);
        static const char* names[3] = {"Small", "Medium", "Large"};
        std::vector<Button> out;
        for (int i = 0; i < 3; ++i) { Button b; b.r = cols[static_cast<size_t>(i)]; b.label = names[i]; b.toggled = zoom_ == i; out.push_back(b); }
        return out;
    }
    Rect swatch(const Layout& l, int i) const {   // palette entry i (0..255) or -1 for the transparent swatch
        if (i < 0) return {l.palette.x, l.palette.bottom() + 4, 60, 18};
        return {l.palette.x + 2 + (i % 16) * 13, l.palette.y + 2 + (i / 16) * 13, 12, 12};
    }
    bool pickColor(const Layout& l, int x, int y) {
        if (swatch(l, -1).contains(x, y)) { color_ = -1; return true; }
        if (!l.palette.contains(x, y)) return false;
        const int cx = (x - l.palette.x - 2) / 13, cy = (y - l.palette.y - 2) / 13;
        if (cx >= 0 && cx < 16 && cy >= 0 && cy < 16) color_ = cy * 16 + cx;
        return true;
    }
    Pt pixelAt(const Layout& l, int x, int y) const {
        if (!l.canvas.contains(x, y)) return {-1, -1};
        const int px = (x - l.canvas.x) / mag_ + scrollX_, py = (y - l.canvas.y) / mag_ + scrollY_;
        if (px < 0 || py < 0 || px >= img_.width || py >= img_.height) return {-1, -1};
        return {px, py};
    }
    Pt snapped(Pt p) const { if (!snap_) return p; return {p.first / gridStep_ * gridStep_, p.second / gridStep_ * gridStep_}; }

    void put(int x, int y, int color) {
        if (x < 0 || y < 0 || x >= img_.width || y >= img_.height) return;
        const size_t o = static_cast<size_t>(y) * static_cast<size_t>(img_.width) + static_cast<size_t>(x);
        if (color < 0) img_.opaque[o] = 0; else { img_.opaque[o] = 1; img_.index[o] = static_cast<uint8_t>(color); }
    }
    void floodFill(int x, int y) {
        const size_t w = static_cast<size_t>(img_.width);
        auto same = [&](size_t a, size_t b) { return img_.opaque[a] == img_.opaque[b] && (!img_.opaque[a] || img_.index[a] == img_.index[b]); };
        const size_t from = static_cast<size_t>(y) * w + static_cast<size_t>(x);
        const bool tOpaque = color_ >= 0;
        if (img_.opaque[from] == (tOpaque ? 1 : 0) && (!tOpaque || img_.index[from] == color_)) return;
        std::vector<Pt> stack{{x, y}};
        const Sprite ref = img_;
        std::vector<char> seen(img_.index.size(), 0);
        while (!stack.empty()) {
            const Pt p = stack.back(); stack.pop_back();
            if (p.first < 0 || p.second < 0 || p.first >= img_.width || p.second >= img_.height) continue;
            const size_t o = static_cast<size_t>(p.second) * w + static_cast<size_t>(p.first);
            if (seen[o] || !same(o, from)) continue;
            // compare with the untouched copy so the fill does not stop at already-painted pixels
            if (!(ref.opaque[o] == ref.opaque[from] && (!ref.opaque[o] || ref.index[o] == ref.index[from]))) continue;
            seen[o] = 1;
            put(p.first, p.second, color_);
            stack.push_back({p.first + 1, p.second}); stack.push_back({p.first - 1, p.second}); stack.push_back({p.first, p.second + 1}); stack.push_back({p.first, p.second - 1});
        }
    }
    void pasteAt(int x0, int y0) {
        for (int y = 0; y < clip_.height; ++y)
            for (int x = 0; x < clip_.width; ++x) {
                const size_t o = static_cast<size_t>(y) * static_cast<size_t>(clip_.width) + static_cast<size_t>(x);
                if (clip_.opaque[o]) put(x0 + x, y0 + y, clip_.index[o]);
            }
    }
    void rotateClip() {   // 90 degrees counter-clockwise
        Sprite r; r.width = clip_.height; r.height = clip_.width;
        r.index.assign(clip_.index.size(), 0); r.opaque = r.index;
        for (int y = 0; y < clip_.height; ++y)
            for (int x = 0; x < clip_.width; ++x) {
                const size_t src = static_cast<size_t>(y) * static_cast<size_t>(clip_.width) + static_cast<size_t>(x);
                const size_t dst = static_cast<size_t>(clip_.width - 1 - x) * static_cast<size_t>(r.width) + static_cast<size_t>(y);
                r.index[dst] = clip_.index[src]; r.opaque[dst] = clip_.opaque[src];
            }
        clip_ = r;
    }
    void flipClip(bool horizontal) {
        Sprite r = clip_;
        for (int y = 0; y < clip_.height; ++y)
            for (int x = 0; x < clip_.width; ++x) {
                const size_t src = static_cast<size_t>(y) * static_cast<size_t>(clip_.width) + static_cast<size_t>(x);
                const size_t dst = horizontal ? static_cast<size_t>(y) * static_cast<size_t>(clip_.width) + static_cast<size_t>(clip_.width - 1 - x)
                                              : static_cast<size_t>(clip_.height - 1 - y) * static_cast<size_t>(clip_.width) + static_cast<size_t>(x);
                r.index[dst] = clip_.index[src]; r.opaque[dst] = clip_.opaque[src];
            }
        clip_ = r;
    }
    void exportBmp() {
        Bmp8 b;
        b.width = img_.width; b.height = img_.height; b.palette = env_.palette;
        b.index.assign(img_.index.size(), 0);
        for (size_t i = 0; i < b.index.size(); ++i) b.index[i] = img_.opaque[i] ? img_.index[i] : 0;
        try {
            const std::string path = env_.gameDir + "/SCURKART/" + std::to_string(id_ + 500 * zoom_) + ".BMP";
            b.save(path);
            host_.setMessage("Exported " + path);
        } catch (const std::exception& e) { host_.setMessage(e.what()); }
    }

    // ---- strokes --------------------------------------------------------------------------------------------------------------------------------------
    void beginStroke(int x, int y) {
        const Layout l = layout();
        Pt p = pixelAt(l, x, y);
        if (p.first < 0) return;
        p = snapped(p);
        if (tool_ == Tool::Eyedropper) { const size_t o = static_cast<size_t>(p.second) * static_cast<size_t>(img_.width) + static_cast<size_t>(p.first); color_ = img_.opaque[o] ? img_.index[o] : -1; return; }
        pushUndo();
        drawing_ = true; start_ = cur_ = p; lastY_ = y;
        if (tool_ == Tool::Brush) put(p.first, p.second, color_);
        else if (tool_ == Tool::Eraser) put(p.first, p.second, -1);
        else if (tool_ == Tool::Fill) { floodFill(p.first, p.second); drawing_ = false; commit(); }
        else if (tool_ == Tool::Paste) { if (hasClip_) pasteAt(p.first, p.second); drawing_ = false; commit(); }
    }
    void continueStroke(int x, int y) {
        const Layout l = layout();
        Pt p = pixelAt(l, x, y);
        lastY_ = y;
        if (p.first < 0) return;
        p = snapped(p);
        if (tool_ == Tool::Brush || tool_ == Tool::Eraser) {
            std::vector<Pt> seg;
            lineSegments(seg, cur_.first, cur_.second, p.first, p.second);
            for (const Pt& q : seg) put(q.first, q.second, tool_ == Tool::Brush ? color_ : -1);
        }
        cur_ = p;
    }
    void endStroke(int x, int y) {
        const Layout l = layout();
        Pt p = pixelAt(l, x, y);
        if (p.first >= 0) cur_ = snapped(p);
        drawing_ = false;
        if (tool_ == Tool::Line || (tool_ >= Tool::Square && tool_ <= Tool::LeftWall))
            for (const Pt& q : shapePoints(tool_, start_.first, start_.second, cur_.first, cur_.second, filled_)) put(q.first, q.second, color_);
        commit();
    }

    // ---- drawing helpers ------------------------------------------------------------------------------------------------------------------------------
    void drawTools(SDL_Renderer* r, const Layout& l) {
        for (const Button& b : toolButtons(l)) drawButton(r, b);
        for (const Button& b : actionButtons(l)) drawButton(r, b);
        clippedText(r, l.tools.x, l.tools.y + 11 * 26 + 8 + 60, l.tools.w, "Mag x" + std::to_string(mag_), theme::kTextDim);
    }
    void drawCanvas(SDL_Renderer* r, const Layout& l) {
        fill(r, l.canvas, Color{30, 34, 44});
        const SDL_Rect clip{l.canvas.x, l.canvas.y, l.canvas.w, l.canvas.h};
        SDL_SetRenderClipRect(r, &clip);
        const auto& pal = env_.shownPalette();
        // the picture, live (not through the texture cache): preview of an unfinished shape on top
        Sprite show = img_;
        if (drawing_ && (tool_ == Tool::Line || (tool_ >= Tool::Square && tool_ <= Tool::LeftWall))) {
            Sprite keep = img_;
            const Sprite saved = img_;
            for (const Pt& q : shapePoints(tool_, start_.first, start_.second, cur_.first, cur_.second, filled_)) put(q.first, q.second, color_);
            show = img_;
            img_ = saved;
        }
        // checkerboard behind the transparent pixels, then the opaque ones as magnified squares
        for (int y = 0; y < show.height; ++y)
            for (int x = 0; x < show.width; ++x) {
                const int sx = l.canvas.x + (x - scrollX_) * mag_, sy = l.canvas.y + (y - scrollY_) * mag_;
                if (sx + mag_ < l.canvas.x || sy + mag_ < l.canvas.y || sx > l.canvas.right() || sy > l.canvas.bottom()) continue;
                const size_t o = static_cast<size_t>(y) * static_cast<size_t>(show.width) + static_cast<size_t>(x);
                if (show.opaque[o]) { const uint32_t c = pal[show.index[o]]; fill(r, sx, sy, mag_, mag_, static_cast<Uint8>(c >> 16), static_cast<Uint8>(c >> 8), static_cast<Uint8>(c)); }
                else { const bool dark = ((x + y) & 1) != 0; fill(r, sx, sy, mag_, mag_, dark ? 44 : 56, dark ? 48 : 60, dark ? 60 : 72); }
            }
        if (grid_ && mag_ >= 4) {
            for (int x = 0; x <= show.width; x += gridStep_) fill(r, l.canvas.x + (x - scrollX_) * mag_, l.canvas.y, 1, show.height * mag_, 255, 255, 255, 50);
            for (int y = 0; y <= show.height; y += gridStep_) fill(r, l.canvas.x, l.canvas.y + (y - scrollY_) * mag_, show.width * mag_, 1, 255, 255, 255, 50);
        }
        if (clipRegion_) frame(r, l.canvas.x - scrollX_ * mag_ - 1, l.canvas.y - scrollY_ * mag_ - 1, show.width * mag_ + 2, show.height * mag_ + 2, 230, 200, 80);
        SDL_SetRenderClipRect(r, nullptr);
    }
    void drawSide(SDL_Renderer* r, const Layout& l) {
        for (const Button& b : zoomButtons(l)) drawButton(r, b);
        const auto& pal = env_.shownPalette();
        fill(r, l.palette, theme::kField);
        for (int i = 0; i < 256; ++i) {
            const Rect s = swatch(l, i);
            const uint32_t c = pal[static_cast<size_t>(i)];
            fill(r, s, Color{static_cast<uint8_t>(c >> 16), static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c)});
            if (i == color_) frame(r, s.x - 1, s.y - 1, s.w + 2, s.h + 2, 255, 255, 255);
        }
        const Rect t = swatch(l, -1);
        fill(r, t, color_ < 0 ? theme::kButtonOn : theme::kButton);
        frame(r, t, theme::kButtonBorder);
        text(r, t.x + 6, t.y + 5, "Clear", theme::kButtonText);
        if (color_ >= 0) { const uint32_t c = pal[static_cast<size_t>(color_)]; fill(r, t.right() + 8, t.y, 40, 18, static_cast<Uint8>(c >> 16), static_cast<Uint8>(c >> 8), static_cast<Uint8>(c)); text(r, t.right() + 54, t.y + 5, "#" + std::to_string(color_), theme::kTextDim); }
        // the object list of the current group
        Rect lr = l.side;
        lr.cutTop(24 + 4 + 16 * 13 + 4 + 28);
        list_.r = lr;
        list_.rowH = 16;
        drawListBox(r, list_);
    }
    void rebuildListIfGroupChanged() {
        if (listBuilt_ && listGroup_ == env_.group && lastRev_ == env_.working.revision) return;
        listBuilt_ = true; listGroup_ = env_.group; lastRev_ = env_.working.revision;
        list_.items.clear(); listIds_.clear();
        for (const ObjectInfo* o : env_.catalog.inGroup(env_.group)) { list_.items.push_back(env_.working.name(o->id, o->name)); listIds_.push_back(o->id); }
        list_.selected = -1;
        for (size_t i = 0; i < listIds_.size(); ++i) if (listIds_[i] == id_) list_.selected = static_cast<int>(i);
        list_.scroll = std::clamp(list_.scroll, 0, list_.maxScroll());
    }
};

}  // namespace

std::unique_ptr<Editor> makePaint(Env& env, ResourceEditors& host) { return std::make_unique<Paint>(env, host); }

}  // namespace sc2k::scurk
