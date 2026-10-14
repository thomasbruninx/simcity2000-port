#include "app/file_browser.h"

#include <algorithm>
#include <filesystem>

#include "app/ui_draw.h"

namespace sc2k {

using namespace ui;
namespace fs = std::filesystem;

void FileBrowser::open(const std::string& title, const std::string& dir, const std::string& name) {
    title_ = title; dir_ = dir; result_ = -2; active_ = true;
    name_.text = name; name_.maxLen = 60;
    name_.accept = [](char ch) { return ch >= 32 && ch < 127 && ch != '/' && ch != '\\' && ch != ':'; };
    refresh();
}

void FileBrowser::refresh() {
    entries_.clear();
    std::error_code ec;
    if (fs::path(dir_).has_parent_path() && fs::path(dir_) != fs::path(dir_).root_path()) entries_.push_back({"..", true});
    std::vector<Entry> dirs, files;
    for (const auto& e : fs::directory_iterator(dir_, fs::directory_options::skip_permission_denied, ec)) {
        const std::string n = e.path().filename().string();
        if (n.empty() || n[0] == '.') continue;
        std::error_code ec2;
        if (e.is_directory(ec2)) dirs.push_back({n, true});
        else {
            std::string ext = e.path().extension().string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".sc2" || ext == ".scn") files.push_back({n, false});
        }
    }
    auto byName = [](const Entry& a, const Entry& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    entries_.insert(entries_.end(), dirs.begin(), dirs.end());
    entries_.insert(entries_.end(), files.begin(), files.end());
    list_.items.clear();
    for (const Entry& e : entries_) list_.items.push_back(e.dir ? "[ " + e.name + " ]" : e.name);
    list_.scroll = 0;
    list_.selected = -1;
}

std::string FileBrowser::path() const {
    std::string n = name_.text;
    std::string low = n;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (low.size() < 4 || (low.substr(low.size() - 4) != ".sc2" && low.substr(low.size() - 4) != ".scn")) n += ".sc2";
    return (fs::path(dir_) / n).string();
}

void FileBrowser::accept() {
    if (name_.text.empty()) return;
    result_ = 0;
    active_ = false;
}

FileBrowser::Widgets FileBrowser::layout(int w, int h) const {
    Widgets l;
    l.panel = centered(Rect{0, 0, w, h}, std::min(600, w - 20), std::min(460, h - 20));
    Rect in = l.panel.inset(14, 0);
    in.cutTop(34);                       // title
    l.pathLine = in.cutTop(20);
    in.cutTop(8);
    Rect bottom = in.cutBottom(36);      // buttons
    Rect nameRow = in.cutBottom(30);
    in.cutBottom(8);
    l.list = list_;
    l.list.r = in;
    l.list.rowH = 18;
    nameRow.cutLeft(86);
    l.name = name_;
    l.name.r = nameRow.inset(0, 3);
    auto cols = columns(bottom.cutRight(200), 2, 14);
    l.save.r = cols[0]; l.save.label = "Save";
    l.cancel.r = cols[1]; l.cancel.label = "Cancel";
    return l;
}

void FileBrowser::draw(SDL_Renderer* r, int w, int h) {
    if (!active_) return;
    const Widgets l = layout(w, h);
    fill(r, 0, 0, w, h, 0, 0, 0, 140);
    drawPanel(r, l.panel, title_);
    fill(r, l.pathLine, theme::kField);
    clippedText(r, l.pathLine.x + 6, l.pathLine.y + 6, l.pathLine.w - 12, ellipsizeStart(dir_, l.pathLine.w - 12), Color{190, 220, 160});
    drawListBox(r, l.list);
    text(r, l.panel.x + 14, l.name.r.y + 6, "File name:", theme::kTextDim);
    drawTextField(r, l.name);
    const int nameW = debugWidth(ellipsizeStart(l.name.text + "_", std::max(8, l.name.r.w - 12)));
    if (nameW + 6 + 40 < l.name.r.w) text(r, l.name.r.x + 6 + nameW, l.name.r.y + (l.name.r.h - kDebugLineH) / 2, ".sc2", theme::kTextDim);   // the extension is added automatically
    drawButton(r, l.save, Color{40, 80, 56});
    drawButton(r, l.cancel, Color{80, 40, 40});
}

void FileBrowser::mouseDown(int x, int y, int w, int h) {
    if (!active_) return;
    Widgets l = layout(w, h);
    if (l.save.hit(x, y)) { accept(); return; }
    if (l.cancel.hit(x, y)) { result_ = -1; active_ = false; return; }
    if (l.list.press(x, y)) {
        const int k = l.list.selected;
        if (k < 0 || k >= static_cast<int>(entries_.size())) return;
        const Entry e = entries_[static_cast<size_t>(k)];
        if (e.dir) {
            dir_ = (e.name == ".." ? fs::path(dir_).parent_path() : fs::path(dir_) / e.name).string();
            refresh();
        } else {
            name_.text = fs::path(e.name).stem().string();   // choose an existing file to overwrite
            list_.selected = k;
        }
    }
}

bool FileBrowser::key(SDL_Keycode k) {
    if (!active_) return false;
    if (k == SDLK_ESCAPE) { result_ = -1; active_ = false; }
    else if (k == SDLK_RETURN) accept();
    else if (k == SDLK_BACKSPACE) name_.backspace();
    return true;
}

void FileBrowser::textInput(const std::string& utf8) { if (active_) name_.textInput(utf8); }

void FileBrowser::wheel(int dy) { if (active_) list_.wheel(dy * 2); }

}  // namespace sc2k
