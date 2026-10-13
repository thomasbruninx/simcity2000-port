#include "app/title_screen.h"

#include <algorithm>
#include <dirent.h>

#include "app/ui_draw.h"
#include "save/city.h"

namespace sc2k {
using namespace ui;

namespace {
std::string chunkText(const CityFile& c, uint8_t marker) {
    for (const auto& x : c.extras)
        if (x.chunk.tag == "TEXT" && x.chunk.stored.size() > 4 && x.chunk.stored[0] == marker) {
            std::string t(x.chunk.stored.begin() + 4, x.chunk.stored.end());
            while (!t.empty() && t.back() == 0) t.pop_back();
            for (char& ch : t) if (ch == '\r') ch = '\n';
            return t;
        }
    return {};
}
std::vector<std::string> listFiles(const std::string& dir, const std::string& ext) {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (const dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) out.push_back(n);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}
std::vector<std::string> wrapText(const std::string& s, size_t width) {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flush = [&] { if (!word.empty()) { if (!cur.empty() && cur.size() + 1 + word.size() > width) { lines.push_back(cur); cur.clear(); } if (!cur.empty()) cur += ' '; cur += word; word.clear(); } };
    for (char ch : s) {
        if (ch == ' ') flush();
        else if (ch == '\n') { flush(); lines.push_back(cur); cur.clear(); }
        else word.push_back(ch);
    }
    flush();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}
}  // namespace

TitleScreen::TitleScreen(const std::string& gameDir, const PeResources& res, const std::array<uint32_t, 256>& gamePalette) : dir_(gameDir), res_(res), palette_(gamePalette) {
    // labels of the original's main menu (dialog 103)
    menuLabels_ = {"Start New City", "Edit New Map", "Load Scenario", "Load Saved City", "Quit"};
    if (const auto* d = res.dialog(103)) {
        auto text = [&](int id) { for (const auto& c : d->controls) if (c.id == id) return c.text; return std::string(); };
        const int ids[5] = {112, 113, 114, 111, 115};
        for (int i = 0; i < 5; ++i) if (!text(ids[i]).empty()) menuLabels_[static_cast<size_t>(i)] = text(ids[i]);
    }
    scan();
}

TitleScreen::~TitleScreen() {
    if (titleTex_) SDL_DestroyTexture(titleTex_);
    for (SDL_Texture* t : pictures_) if (t) SDL_DestroyTexture(t);
}

void TitleScreen::scan() {
    for (const std::string& n : listFiles(dir_ + "/SCENARIO", ".SCN")) {
        try {
            const CityFile c = CityFile::load(dir_ + "/SCENARIO/" + n);
            ScenarioEntry e;
            e.path = dir_ + "/SCENARIO/" + n;
            e.name = c.name().empty() ? n : c.name();
            e.shortText = chunkText(c, 0x80); e.longText = chunkText(c, 0x81);
            for (const auto& x : c.extras)
                if (x.chunk.tag == "PICT" && x.chunk.stored.size() >= 8 + 65 * 66) e.picture.assign(x.chunk.stored.begin() + 8, x.chunk.stored.begin() + 8 + 65 * 66);
            scenarios_.push_back(std::move(e));
        } catch (const std::exception&) {}
    }
    cities_ = listFiles(dir_ + "/CITIES", ".SC2");
    for (const std::string& n : listFiles(dir_, ".SC2")) cities_.push_back("../" + n);
}

void TitleScreen::ensureTextures(SDL_Renderer* r) {
    if (!triedTitle_) {
        triedTitle_ = true;
        try {
            const Bmp8 b = Bmp8::load(dir_ + "/BITMAPS/TITLESCR.BMP");
            std::vector<uint32_t> px(static_cast<size_t>(b.width) * static_cast<size_t>(b.height));
            for (size_t i = 0; i < px.size(); ++i) px[i] = b.palette[b.index[i]];
            titleTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, b.width, b.height);
            if (titleTex_) SDL_UpdateTexture(titleTex_, nullptr, px.data(), b.width * 4);
            titleW_ = b.width; titleH_ = b.height;
        } catch (const std::exception&) {}
    }
    if (pictures_.size() != scenarios_.size()) {
        pictures_.assign(scenarios_.size(), nullptr);
        for (size_t i = 0; i < scenarios_.size(); ++i) {
            const auto& pic = scenarios_[i].picture;
            if (pic.empty()) continue;
            std::vector<uint32_t> px(65 * 65);
            for (int y = 0; y < 65; ++y) for (int x = 0; x < 65; ++x) px[static_cast<size_t>(y * 65 + x)] = palette_[pic[static_cast<size_t>(y * 66 + x)]] | 0xFF000000u;
            SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 65, 65);
            if (t) { SDL_UpdateTexture(t, nullptr, px.data(), 65 * 4); SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST); }
            pictures_[i] = t;
        }
    }
}

TitleScreen::Rect TitleScreen::menuButton(int i, int w, int h) const { return {w - 330, h / 2 - 110 + i * 48, 260, 38}; }
TitleScreen::Rect TitleScreen::button(int which, int w, int h) const { return {w / 2 - 170 + which * 190, h - 70, 160, 34}; }

void TitleScreen::draw(SDL_Renderer* r, int w, int h) {
    ensureTextures(r);
    fill(r, 0, 0, w, h, 8, 12, 24);
    if (titleTex_ && mode_ == Mode::Menu) {
        const float scale = std::min(static_cast<float>(w - 380) / static_cast<float>(titleW_), static_cast<float>(h - 40) / static_cast<float>(titleH_));
        const SDL_FRect dst{20.f, (static_cast<float>(h) - titleH_ * scale) / 2.f, titleW_ * scale, titleH_ * scale};
        SDL_RenderTexture(r, titleTex_, nullptr, &dst);
    }
    if (mode_ == Mode::Menu) {
        for (int i = 0; i < 5; ++i) {
            const Rect b = menuButton(i, w, h);
            fill(r, b.x, b.y, b.w, b.h, 40, 56, 96); frame(r, b.x, b.y, b.w, b.h, 190, 200, 230);
            text(r, b.x + 16, b.y + 15, menuLabels_[static_cast<size_t>(i)], 255, 255, 220);
        }
        text(r, w - 330, h / 2 + 150, "SimCity 2000 (port) - needs your own game files", 150, 160, 190);
        return;
    }
    const bool scen = mode_ == Mode::Scenarios;
    text(r, 60, 70, scen ? "Select Scenario" : "Load Saved City", 255, 255, 190);
    const size_t n = scen ? scenarios_.size() : cities_.size();
    for (size_t i = 0; i < n; ++i) {
        const int y = 110 + static_cast<int>(i) * 20;
        const bool sel = static_cast<int>(i) == selected_;
        fill(r, 60, y, 300, 18, sel ? 70 : 20, sel ? 90 : 28, sel ? 140 : 44);
        text(r, 68, y + 5, scen ? scenarios_[i].name : cities_[i], 235, 235, 235);
    }
    if (scen && selected_ >= 0 && static_cast<size_t>(selected_) < scenarios_.size()) {
        const ScenarioEntry& e = scenarios_[static_cast<size_t>(selected_)];
        if (static_cast<size_t>(selected_) < pictures_.size() && pictures_[static_cast<size_t>(selected_)]) {
            const SDL_FRect dst{400.f, 110.f, 65.f * 3, 65.f * 3};
            SDL_RenderTexture(r, pictures_[static_cast<size_t>(selected_)], nullptr, &dst);
        }
        text(r, 620, 110, e.name, 255, 255, 190);
        int y = 134;
        for (const std::string& l : wrapText(e.shortText, 70)) { text(r, 620, y, l, 220, 220, 230); y += 12; }
        y += 10;
        for (const std::string& l : wrapText(e.longText, 70)) { text(r, 620, y, l, 160, 170, 190); y += 12; }
    }
    const Rect ok = button(0, w, h), cancel = button(1, w, h);
    fill(r, ok.x, ok.y, ok.w, ok.h, 40, 80, 56); frame(r, ok.x, ok.y, ok.w, ok.h, 190, 230, 200);
    text(r, ok.x + 20, ok.y + 13, scen ? "Select Scenario" : "Load City", 255, 255, 220);
    fill(r, cancel.x, cancel.y, cancel.w, cancel.h, 80, 40, 40); frame(r, cancel.x, cancel.y, cancel.w, cancel.h, 230, 190, 190);
    text(r, cancel.x + 50, cancel.y + 13, "Cancel", 255, 255, 220);
}

void TitleScreen::activateMenu(int i) {
    switch (i) {
        case 0: action = Action::NewCity; break;
        case 1: action = Action::NewMap; break;
        case 2: mode_ = Mode::Scenarios; selected_ = 0; break;
        case 3: mode_ = Mode::Cities; selected_ = 0; break;
        default: action = Action::Quit; break;
    }
}

void TitleScreen::choose() {
    if (mode_ == Mode::Scenarios && selected_ >= 0 && static_cast<size_t>(selected_) < scenarios_.size()) { chosenPath = scenarios_[static_cast<size_t>(selected_)].path; action = Action::LoadScenario; }
    else if (mode_ == Mode::Cities && selected_ >= 0 && static_cast<size_t>(selected_) < cities_.size()) {
        const std::string& n = cities_[static_cast<size_t>(selected_)];
        chosenPath = n.rfind("../", 0) == 0 ? dir_ + "/" + n.substr(3) : dir_ + "/CITIES/" + n;
        action = Action::LoadCity;
    }
}

void TitleScreen::mouseDown(int x, int y, int w, int h) {
    if (mode_ == Mode::Menu) {
        for (int i = 0; i < 5; ++i) if (menuButton(i, w, h).in(x, y)) { activateMenu(i); return; }
        return;
    }
    const size_t n = mode_ == Mode::Scenarios ? scenarios_.size() : cities_.size();
    for (size_t i = 0; i < n; ++i) if (Rect{60, 110 + static_cast<int>(i) * 20, 300, 18}.in(x, y)) { selected_ = static_cast<int>(i); return; }
    if (button(0, w, h).in(x, y)) choose();
    if (button(1, w, h).in(x, y)) mode_ = Mode::Menu;
}

void TitleScreen::keyDown(SDL_Keycode k) {
    if (mode_ == Mode::Menu) {
        if (k == SDLK_N) activateMenu(0);
        else if (k == SDLK_E) activateMenu(1);
        else if (k == SDLK_S) activateMenu(2);
        else if (k == SDLK_L) activateMenu(3);
        else if (k == SDLK_Q || k == SDLK_ESCAPE) activateMenu(4);
        return;
    }
    const int n = static_cast<int>(mode_ == Mode::Scenarios ? scenarios_.size() : cities_.size());
    if (k == SDLK_ESCAPE) mode_ = Mode::Menu;
    else if (k == SDLK_DOWN && selected_ + 1 < n) ++selected_;
    else if (k == SDLK_UP && selected_ > 0) --selected_;
    else if (k == SDLK_RETURN) choose();
}

}  // namespace sc2k
