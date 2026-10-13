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
    menuLabels_ = {"Start New City", "Edit New Map", "Load Scenario", "Load Saved City", "Settings", "Quit"};
    if (const auto* d = res.dialog(103)) {
        auto text = [&](int id) { for (const auto& c : d->controls) if (c.id == id) return c.text; return std::string(); };
        const int ids[4] = {112, 113, 114, 111};
        for (int i = 0; i < 4; ++i) if (!text(ids[i]).empty()) menuLabels_[static_cast<size_t>(i)] = text(ids[i]);
        if (!text(115).empty()) menuLabels_[5] = text(115);
    }
    try { palette_ = Bmp8::load(gameDir + "/BITMAPS/PAL_MAC.BMP").palette; } catch (const std::exception&) {}   // the scenario pictures use the Macintosh palette
    settings = AudioSettings::load();
    fonts_ = AudioSettings::discoverSoundfonts(gameDir);
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
    cities_.clear();
    {   // games saved from the pause menu come first
        const std::string p = AudioSettings::settingsPath();
        const std::string saves = p.substr(0, p.rfind('/')) + "/saves";
        for (const std::string& n : listFiles(saves, ".sc2")) cities_.push_back("saves/" + n);
    }
    for (const std::string& n : listFiles(dir_ + "/CITIES", ".SC2")) cities_.push_back(n);
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

TitleScreen::Rect TitleScreen::menuButton(int i, int w, int h) const { return {w - 330, h / 2 - 140 + i * 48, 260, 38}; }
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
        for (int i = 0; i < 6; ++i) {
            const Rect b = menuButton(i, w, h);
            fill(r, b.x, b.y, b.w, b.h, 40, 56, 96); frame(r, b.x, b.y, b.w, b.h, 190, 200, 230);
            text(r, b.x + 16, b.y + 15, menuLabels_[static_cast<size_t>(i)], 255, 255, 220);
        }
        text(r, w - 330, h / 2 + 170, "SimCity 2000 (port) - needs your own game files", 150, 160, 190);
        return;
    }
    if (mode_ == Mode::Settings) { drawSettings(r, w, h); return; }
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

namespace {
std::string baseName(const std::string& p) { const size_t k = p.rfind('/'); return k == std::string::npos ? p : p.substr(k + 1); }
}

// layout of the settings screen: synth buttons, SoundFont list, volume, test / back
void TitleScreen::drawSettings(SDL_Renderer* r, int w, int h) {
    text(r, 60, 60, "Settings - music", 255, 255, 190);
    text(r, 60, 100, "Music synthesizer", 200, 210, 230);
    const bool fluid = settings.synth == AudioSettings::Synth::FluidSynth;
    const bool haveFluid = fluidSynthAvailable();
    fill(r, 60, 120, 260, 30, !fluid ? 70 : 28, !fluid ? 100 : 36, !fluid ? 150 : 56); frame(r, 60, 120, 260, 30, 190, 200, 230);
    text(r, 76, 131, "Built-in synthesizer", 255, 255, 220);
    fill(r, 340, 120, 260, 30, fluid ? 70 : 28, fluid ? 100 : 36, fluid ? 150 : 56); frame(r, 340, 120, 260, 30, haveFluid ? 190 : 90, haveFluid ? 200 : 90, haveFluid ? 230 : 100);
    text(r, 356, 131, haveFluid ? "FluidSynth + SoundFont" : "FluidSynth (not in this build)", haveFluid ? 255 : 130, haveFluid ? 255 : 130, haveFluid ? 220 : 140);
    text(r, 60, 180, "SoundFont (.sf2) for FluidSynth", 200, 210, 230);
    if (fonts_.empty()) text(r, 60, 204, "none found - put a .sf2 into the resources folder next to the game", 230, 150, 120);
    for (size_t i = 0; i < fonts_.size() && i < 8; ++i) {
        const bool sel = settings.soundfont == fonts_[i] || (settings.soundfont.empty() && i == 0);
        fill(r, 60, 200 + static_cast<int>(i) * 22, 1100, 20, sel ? 70 : 20, sel ? 100 : 28, sel ? 150 : 44);
        text(r, 68, 206 + static_cast<int>(i) * 22, baseName(fonts_[i]) + "   (" + fonts_[i] + ")", 235, 235, 235);
    }
    text(r, 60, 400, "Music volume", 200, 210, 230);
    fill(r, 60, 420, 40, 26, 40, 56, 96); frame(r, 60, 420, 40, 26, 190, 200, 230); text(r, 76, 429, "-");
    fill(r, 108, 420, 120, 26, 20, 28, 44); text(r, 130, 429, std::to_string(settings.volume) + " %", 255, 255, 220);
    fill(r, 236, 420, 40, 26, 40, 56, 96); frame(r, 236, 420, 40, 26, 190, 200, 230); text(r, 252, 429, "+");
    fill(r, 60, 470, 160, 34, 40, 80, 56); frame(r, 60, 470, 160, 34, 190, 230, 200); text(r, 84, 483, "Play test tune", 255, 255, 220);
    fill(r, 240, 470, 160, 34, 80, 40, 40); frame(r, 240, 470, 160, 34, 230, 190, 190); text(r, 292, 483, "Back", 255, 255, 220);
    if (!status_.empty()) text(r, 60, 530, status_, 200, 220, 160);
    text(r, 60, h - 30, "Settings are saved to " + AudioSettings::settingsPath(), 130, 140, 160);
    (void)w;
}

void TitleScreen::settingsClick(int x, int y, int w, int h) {
    (void)w; (void)h;
    auto apply = [&] { if (applySettings) status_ = applySettings(settings); settings.save(); };
    if (Rect{60, 120, 260, 30}.in(x, y)) { settings.synth = AudioSettings::Synth::Builtin; apply(); return; }
    if (Rect{340, 120, 260, 30}.in(x, y)) {
        if (!fluidSynthAvailable()) { status_ = "This build was made without FluidSynth (install fluid-synth and rebuild)."; return; }
        settings.synth = AudioSettings::Synth::FluidSynth;
        if (settings.soundfont.empty() && !fonts_.empty()) settings.soundfont = fonts_.front();
        apply();
        return;
    }
    for (size_t i = 0; i < fonts_.size() && i < 8; ++i)
        if (Rect{60, 200 + static_cast<int>(i) * 22, 1100, 20}.in(x, y)) { settings.soundfont = fonts_[i]; if (settings.synth == AudioSettings::Synth::FluidSynth) apply(); else settings.save(); return; }
    if (Rect{60, 420, 40, 26}.in(x, y)) { settings.volume = std::max(0, settings.volume - 10); apply(); return; }
    if (Rect{236, 420, 40, 26}.in(x, y)) { settings.volume = std::min(100, settings.volume + 10); apply(); return; }
    if (Rect{60, 470, 160, 34}.in(x, y)) { if (testMusic) testMusic(); return; }
    if (Rect{240, 470, 160, 34}.in(x, y)) { settings.save(); leave(); return; }
}

void TitleScreen::activateMenu(int i) {
    switch (i) {
        case 0: action = Action::NewCity; break;
        case 1: action = Action::NewMap; break;
        case 2: mode_ = Mode::Scenarios; selected_ = 0; break;
        case 3: mode_ = Mode::Cities; selected_ = 0; break;
        case 4: mode_ = Mode::Settings; status_.clear(); break;
        default: action = Action::Quit; break;
    }
}

void TitleScreen::choose() {
    if (mode_ == Mode::Scenarios && selected_ >= 0 && static_cast<size_t>(selected_) < scenarios_.size()) { chosenPath = scenarios_[static_cast<size_t>(selected_)].path; action = Action::LoadScenario; }
    else if (mode_ == Mode::Cities && selected_ >= 0 && static_cast<size_t>(selected_) < cities_.size()) {
        const std::string& n = cities_[static_cast<size_t>(selected_)];
        if (n.rfind("saves/", 0) == 0) { const std::string p = AudioSettings::settingsPath(); chosenPath = p.substr(0, p.rfind('/')) + "/" + n; }
        else chosenPath = n.rfind("../", 0) == 0 ? dir_ + "/" + n.substr(3) : dir_ + "/CITIES/" + n;
        action = Action::LoadCity;
    }
}

void TitleScreen::mouseDown(int x, int y, int w, int h) {
    if (mode_ == Mode::Menu) {
        for (int i = 0; i < 6; ++i) if (menuButton(i, w, h).in(x, y)) { activateMenu(i); return; }
        return;
    }
    if (mode_ == Mode::Settings) { settingsClick(x, y, w, h); return; }
    const size_t n = mode_ == Mode::Scenarios ? scenarios_.size() : cities_.size();
    for (size_t i = 0; i < n; ++i) if (Rect{60, 110 + static_cast<int>(i) * 20, 300, 18}.in(x, y)) { selected_ = static_cast<int>(i); return; }
    if (button(0, w, h).in(x, y)) choose();
    if (button(1, w, h).in(x, y)) leave();
}

void TitleScreen::keyDown(SDL_Keycode k) {
    if (mode_ == Mode::Menu) {
        if (k == SDLK_N) activateMenu(0);
        else if (k == SDLK_E) activateMenu(1);
        else if (k == SDLK_S) activateMenu(2);
        else if (k == SDLK_L) activateMenu(3);
        else if (k == SDLK_C) activateMenu(4);
        else if (k == SDLK_Q || k == SDLK_ESCAPE) activateMenu(5);
        return;
    }
    if (mode_ == Mode::Settings) { if (k == SDLK_ESCAPE) { settings.save(); leave(); } return; }
    const int n = static_cast<int>(mode_ == Mode::Scenarios ? scenarios_.size() : cities_.size());
    if (k == SDLK_ESCAPE) leave();
    else if (k == SDLK_DOWN && selected_ + 1 < n) ++selected_;
    else if (k == SDLK_UP && selected_ > 0) --selected_;
    else if (k == SDLK_RETURN) choose();
}

}  // namespace sc2k
