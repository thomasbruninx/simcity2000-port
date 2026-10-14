#include "app/file_browser.h"

#include <algorithm>
#include <filesystem>

#include "app/ui_draw.h"

namespace sc2k {

using namespace ui;
namespace fs = std::filesystem;

void FileBrowser::open(const std::string& title, const std::string& dir, const std::string& name) {
    title_ = title; dir_ = dir; name_ = name; scroll_ = 0; result_ = -2; active_ = true;
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
    scroll_ = 0;
}

std::string FileBrowser::path() const {
    std::string n = name_;
    std::string low = n;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (low.size() < 4 || (low.substr(low.size() - 4) != ".sc2" && low.substr(low.size() - 4) != ".scn")) n += ".sc2";
    return (fs::path(dir_) / n).string();
}

void FileBrowser::accept() {
    if (name_.empty()) return;
    result_ = 0;
    active_ = false;
}

namespace {
struct Layout { int x, y, w, h; };
Layout panel(int w, int h) { return {(w - 580) / 2, (h - 440) / 2, 580, 440}; }
}  // namespace

void FileBrowser::draw(SDL_Renderer* r, int w, int h) {
    if (!active_) return;
    const Layout p = panel(w, h);
    fill(r, 0, 0, w, h, 0, 0, 0, 140);
    fill(r, p.x, p.y, p.w, p.h, 28, 32, 44);
    frame(r, p.x, p.y, p.w, p.h, 200, 200, 220);
    text(r, p.x + 14, p.y + 12, title_, 255, 255, 190);
    std::string shown = dir_;
    if (shown.size() > 66) shown = "..." + shown.substr(shown.size() - 63);
    fill(r, p.x + 14, p.y + 32, p.w - 28, 18, 14, 16, 24);
    text(r, p.x + 20, p.y + 37, shown, 190, 220, 160);
    fill(r, p.x + 14, p.y + 58, p.w - 28, kRows * kRowH + 4, 16, 18, 28);
    for (int i = 0; i < kRows; ++i) {
        const size_t k = static_cast<size_t>(scroll_ + i);
        if (k >= entries_.size()) break;
        const Entry& e = entries_[k];
        const bool sel = !e.dir && e.name == name_;
        if (sel) fill(r, p.x + 16, p.y + 60 + i * kRowH, p.w - 32, kRowH, 60, 90, 140);
        text(r, p.x + 24, p.y + 65 + i * kRowH, (e.dir ? "[ " : "  ") + e.name + (e.dir ? " ]" : ""), e.dir ? 150 : 235, e.dir ? 200 : 235, e.dir ? 255 : 235);
    }
    // scroll bar
    if (entries_.size() > static_cast<size_t>(kRows)) {
        const int th = kRows * kRowH;
        const int bh = std::max(12, th * kRows / static_cast<int>(entries_.size()));
        const int by = p.y + 60 + (th - bh) * scroll_ / std::max(1, static_cast<int>(entries_.size()) - kRows);
        fill(r, p.x + p.w - 22, by, 6, bh, 110, 120, 150);
    }
    text(r, p.x + 14, p.y + 62 + kRows * kRowH + 12, "File name:", 200, 210, 230);
    fill(r, p.x + 100, p.y + 58 + kRows * kRowH + 8, p.w - 114, 20, 12, 14, 20);
    text(r, p.x + 106, p.y + 64 + kRows * kRowH + 8, name_ + "_.sc2", 255, 255, 255);
    fill(r, p.x + p.w - 200, p.y + p.h - 40, 86, 26, 40, 80, 56); frame(r, p.x + p.w - 200, p.y + p.h - 40, 86, 26, 190, 230, 200);
    text(r, p.x + p.w - 200 + 27, p.y + p.h - 31, "Save", 255, 255, 220);
    fill(r, p.x + p.w - 100, p.y + p.h - 40, 86, 26, 80, 40, 40); frame(r, p.x + p.w - 100, p.y + p.h - 40, 86, 26, 230, 190, 190);
    text(r, p.x + p.w - 100 + 19, p.y + p.h - 31, "Cancel", 255, 255, 220);
}

void FileBrowser::mouseDown(int x, int y, int w, int h) {
    if (!active_) return;
    const Layout p = panel(w, h);
    if (x >= p.x + p.w - 200 && x < p.x + p.w - 114 && y >= p.y + p.h - 40 && y < p.y + p.h - 14) { accept(); return; }
    if (x >= p.x + p.w - 100 && x < p.x + p.w - 14 && y >= p.y + p.h - 40 && y < p.y + p.h - 14) { result_ = -1; active_ = false; return; }
    if (x >= p.x + 14 && x < p.x + p.w - 14 && y >= p.y + 60 && y < p.y + 60 + kRows * kRowH) {
        const size_t k = static_cast<size_t>(scroll_ + (y - p.y - 60) / kRowH);
        if (k >= entries_.size()) return;
        const Entry e = entries_[k];
        if (e.dir) {
            fs::path np = e.name == ".." ? fs::path(dir_).parent_path() : fs::path(dir_) / e.name;
            dir_ = np.string();
            refresh();
        } else {
            name_ = fs::path(e.name).stem().string();   // choose an existing file to overwrite
        }
    }
}

bool FileBrowser::key(SDL_Keycode k) {
    if (!active_) return false;
    if (k == SDLK_ESCAPE) { result_ = -1; active_ = false; }
    else if (k == SDLK_RETURN) accept();
    else if (k == SDLK_BACKSPACE && !name_.empty()) name_.pop_back();
    return true;
}

void FileBrowser::textInput(const std::string& utf8) {
    if (!active_) return;
    for (char ch : utf8) if (ch >= 32 && ch < 127 && ch != '/' && ch != '\\' && ch != ':' && name_.size() < 60) name_.push_back(ch);
}

void FileBrowser::wheel(int dy) {
    if (!active_) return;
    const int maxScroll = std::max(0, static_cast<int>(entries_.size()) - kRows);
    scroll_ = std::clamp(scroll_ - dy * 2, 0, maxScroll);
}

}  // namespace sc2k
