#include "app/game_ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "save/error.h"
#include "simulation/bonds.h"

namespace sc2k {
namespace {
constexpr int kMenuBar = MenuBar::kHeight, kTopBar = 28 + MenuBar::kHeight, kGroupW = 150, kGroupH = 26, kItemW = 290, kItemH = 17, kBottomBar = 22;
// icon of a toolbar group / top-bar button: strip 1 = main toolbar (resource 2, 23 px cells), strip 2 = terrain strip (resource 207, 19 px cells)
struct IconRef { int strip, cell; };
IconRef groupIcon(const std::string& name) {
    static const std::pair<const char*, IconRef> table[] = {
        {"Bulldozer", {1, 0}}, {"Landscape", {1, 1}}, {"Dispatch", {1, 2}}, {"Terrain", {2, 0}}, {"Terrain editor", {2, 6}}, {"Zones", {1, 9}}, {"Power", {1, 3}}, {"Water", {1, 4}}, {"Roads", {1, 6}}, {"Rail", {1, 7}},
        {"Education", {1, 12}}, {"Safety", {1, 13}}, {"Recreation", {1, 14}}, {"Rewards", {1, 5}}, {"Sign", {1, 15}}, {"Query", {1, 16}}};
    for (const auto& e : table) if (name == e.first) return e.second;
    return {0, -1};
}
// buttons at the right end of the top bar: rotate left / right, zoom out / in, centre, budget
constexpr int kTopButtons[10] = {17, 18, 19, 20, 21, 23, 25, 16, 22, 27};   // rotate x2, zoom x2, centre, city, graphs, advisors, newspaper, budget
constexpr int kTopButtonStep = 27, kTopButtonCount = 10;
constexpr const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr const char* kSpeedNames[6] = {"", "Paused", "Turtle", "Llama", "Cheetah", "Swallow"};

void fill(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca = 255) {
    SDL_SetRenderDrawBlendMode(r, ca < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(r, &f);
}
void frame(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderRect(r, &f);
}
void text(SDL_Renderer* r, int x, int y, const std::string& s, Uint8 cr = 235, Uint8 cg = 235, Uint8 cb = 235) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, static_cast<float>(x), static_cast<float>(y), s.c_str());
}
}  // namespace

GameUi::GameUi(Simulation& sim, MapRenderer& renderer, Camera& cam, Clock& clock, const PeResources& resources, CityFile base)
    : sim_(sim), renderer_(renderer), cam_(cam), clock_(clock), res_(resources), base_(std::move(base)), budget_(sim.state(), sim.tables(), resources), info_(sim, resources), avail_(ToolAvailability::fromState(sim.state(), sim.tables())) {
    info_.onMapClick = [this](int a, int b) { centerView(a, b); };
    info_.viewCenter = [this](int& a, int& b) { return renderer_.pickTile(sim_.world(), cam_, mapW() / 2, mapH() / 2, a, b); };
    menu_.checked = [this](int id) { return menuChecked(id); };
    menu_.enabled = [this](int id) { return menuEnabled(id); };
    buildMenu();
    budget_.onBond = [this](int what) {
        message_.clear();
        const bool ok = what == 0 ? issueBond(sim_.context()) : repayOldestBond(sim_.context());
        if (ok) say(what == 0 ? "Bond issued" : "Bond repaid");
        else if (message_.empty()) say("No bond was issued or repaid");
    };
}

std::string GameUi::moneyText(int v) const {
    char buf[32];
    std::snprintf(buf, sizeof buf, "$%d", std::abs(v));
    std::string digits = buf + 1, out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return (v < 0 ? "-$" : "$") + out;
}

const ToolItem* GameUi::tool() const {
    const auto& cat = toolCatalog();
    if (group_ < 0 || group_ >= static_cast<int>(cat.size())) return nullptr;
    const auto& g = cat[static_cast<size_t>(group_)];
    if (item_ < 0 || item_ >= static_cast<int>(g.items.size())) return nullptr;
    return &g.items[static_cast<size_t>(item_)];
}

int GameUi::unitCost(const ToolItem& t) const { return toolCost(sim_.context(), t.cat, t.sub); }

void GameUi::say(const std::string& s) { message_ = s; messageMs_ = 6000; }

bool GameUi::selectTool(const std::string& name) {
    const auto& cat = toolCatalog();
    for (size_t g = 0; g < cat.size(); ++g)
        for (size_t i = 0; i < cat[g].items.size(); ++i)
            if (name == cat[g].items[i].name || (std::strstr(cat[g].items[i].name, name.c_str()) == cat[g].items[i].name)) {
                group_ = static_cast<int>(g); item_ = static_cast<int>(i); menuOpen_ = false;
                return cat[g].items[i].kind != ToolKind::Unavailable;
            }
    return false;
}

bool GameUi::openMenu(const std::string& groupName) {
    const auto& cat = toolCatalog();
    for (size_t g = 0; g < cat.size(); ++g)
        if (groupName == cat[g].name) { group_ = static_cast<int>(g); item_ = std::max(item_, 0); menuOpen_ = true; return true; }
    return false;
}

bool GameUi::groupAllowed(size_t g) const {   // the map editor offers only the terrain tools
    if (!mapEdit_) return true;
    const std::string n = toolCatalog()[g].name;
    return n == "Landscape" || n == "Terrain editor" || n == "Terrain";
}

GameUi::Rect GameUi::groupRect(int i) const { return {0, kTopBar + i * kGroupH, kGroupW, kGroupH}; }
GameUi::Rect GameUi::itemRect(int i) const { return {kGroupW + 2, kTopBar + std::max(0, group_) * kGroupH + i * kItemH, kItemW, kItemH}; }

bool GameUi::overUi(int x, int y) const {
    if (modal_ || budget_.open || info_.open != InfoWindows::Which::None) return true;
    if (mapEdit_ && editPanel().contains(x, y)) return true;
    if (y < kTopBar || y >= height_ - kBottomBar) return true;
    const int groups = static_cast<int>(toolCatalog().size());
    if (x < kGroupW && y < kTopBar + groups * kGroupH) return true;
    if (menuOpen_ && group_ >= 0) {
        const int n = static_cast<int>(toolCatalog()[static_cast<size_t>(group_)].items.size());
        if (itemRect(0).x <= x && x < itemRect(0).x + kItemW && y >= itemRect(0).y && y < itemRect(0).y + n * kItemH) return true;
    }
    return false;
}

void GameUi::update(int dtMs) {
    if (mapEdit_) sim_.state().treasury = 99999999;   // editing the map is free
    if (pauseOpen_) return;
    if (info_.popupDue() && !headless && info_.open == InfoWindows::Which::None && !modal_) info_.open = InfoWindows::Which::Newspaper;   // milestone / new tool stories pop the paper up
    const int month = sim_.state().month();
    if (month != lastMonth_) {   // Newspaper > Subscription: the paper arrives in April and August (0x412FE0)
        if (lastMonth_ >= 0 && info_.subscription && (month == 3 || month == 7) && !headless && info_.open == InfoWindows::Which::None && !modal_) info_.open = InfoWindows::Which::Newspaper;
        lastMonth_ = month;
    }
    const ThingState& th = sim_.context().things;
    const bool emergency = th.monsters > 0 || th.tornados > 0 || th.fireMessageKind >= 0;
    if (emergency && !emergency_) { startDispatch(sim_.context(), ts_); say("Emergency! The dispatch tool is available."); }
    emergency_ = emergency;
    if (dragging_ && tool() && tool()->kind == ToolKind::TerrainEdit && tool()->sub <= 2) {   // the original's loop keeps spraying while the button is down
        sprayMs_ += dtMs;
        while (sprayMs_ >= 25) { sprayMs_ -= 25; editSample(lastA_, lastB_, 0); }
    }
    if (messageMs_ > 0) { messageMs_ -= dtMs; if (messageMs_ <= 0) message_.clear(); }
}

// ---- input -------------------------------------------------------------------------------------------------------------

void GameUi::mouseDown(int x, int y, int button) {
    mouseX_ = x; mouseY_ = y;
    if (browser_.active()) { if (button == 1) browser_.mouseDown(x, y, width_, height_); return; }
    if (pauseOpen_ && !modal_) {
        for (int i = 0; i < 5; ++i) if (Rect{width_ / 2 - 130, height_ / 2 - 130 + i * 48, 260, 38}.contains(x, y)) { pauseClick(i); return; }
        return;
    }
    if (modal_) {
        const int n = static_cast<int>(modal_->options.size());
        const int bx = (width_ - 520) / 2, by = (height_ - (70 + 22 * n)) / 2;
        for (int i = 0; i < n; ++i)
            if (Rect{bx + 20, by + 50 + 22 * i, 480, 20}.contains(x, y)) modal_->result = i;
        return;
    }
    if (button == 1) {   // the menu bar of the original
        int cmd = 0;
        if (menu_.mouseDown(x, y, cmd)) { if (cmd) menuCommand(cmd); return; }
    }
    if (budget_.mouseDown(x, y, button, width_, height_)) return;
    if (queryOpen_ && button == 1) queryOpen_ = false;
    if (button != 1) { scrolling_ = true; return; }
    if (info_.mouseDown(x, y, width_, height_)) return;
    if (mapEdit_ && mapEditorMouse(x, y, true)) return;
    if (y >= kMenuBar && y < kTopBar && x >= width_ - kTopButtonCount * kTopButtonStep - 4) {
        const int k = (x - (width_ - kTopButtonCount * kTopButtonStep - 4)) / kTopButtonStep;
        if (k == 0 || k == 1) { cam_.rotation = (cam_.rotation + (k == 0 ? 3 : 1)) & 3; cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, mapW() / 2, mapH() / 2); }
        else if (k == 2) setZoomLevel(zoomLevel_ - 1);
        else if (k == 3) setZoomLevel(zoomLevel_ + 1);
        else if (k == 4) cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, mapW() / 2, mapH() / 2);
        else if (k == 5) info_.open = InfoWindows::Which::City;
        else if (k == 6) info_.open = InfoWindows::Which::Graphs;
        else if (k == 7) info_.open = InfoWindows::Which::Advisors;
        else if (k == 8) info_.open = InfoWindows::Which::Newspaper;
        else if (k == 9) budget_.open = true;
        return;
    }
    if (overUi(x, y)) {
        const auto& cat = toolCatalog();
        for (size_t g = 0; g < cat.size(); ++g)
            if (groupRect(static_cast<int>(g)).contains(x, y)) {
                if (!groupAllowed(g)) { say("Not while editing the map"); return; }
                group_ = static_cast<int>(g);
                item_ = std::max(0, item_ < static_cast<int>(cat[g].items.size()) ? item_ : 0);
                if (cat[g].items.size() == 1) { item_ = 0; menuOpen_ = false; } else menuOpen_ = true;
                return;
            }
        if (menuOpen_ && group_ >= 0) {
            const int n = static_cast<int>(cat[static_cast<size_t>(group_)].items.size());
            for (int i = 0; i < n; ++i)
                if (itemRect(i).contains(x, y)) {
                    if (!available(cat[static_cast<size_t>(group_)].items[static_cast<size_t>(i)])) say(cat[static_cast<size_t>(group_)].items[static_cast<size_t>(i)].kind == ToolKind::Unavailable ? "Not ported yet" : "Not invented yet");
                    else { item_ = i; menuOpen_ = false; }
                    return;
                }
        }
        if (y >= height_ - kBottomBar) {   // view buttons
            if (x < 100) layers.underground = !layers.underground;
            else if (x < 180) { layers.powerOverlay = !layers.powerOverlay; }
            else if (x < 260) { layers.waterOverlay = !layers.waterOverlay; }
            else if (x < 340) { layers.streets = !layers.streets; }
        }
        return;
    }
    menuOpen_ = false;
    const ToolItem* t = tool();
    int a, b;
    if (!renderer_.pickTile(sim_.world(), cam_, mapPx(x), mapPx(y), a, b)) return;
    if (!t) { queryTile(a, b); return; }
    switch (t->kind) {
        case ToolKind::Bulldoze: playSound(0x1FC, -1); dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; bulldozeAt(a, b); break;
        case ToolKind::TerrainEdit:
            dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b;
            editStartAlt_ = editLastAlt_ = sim_.world().altm.at(a, b) & 0x1F; editStartY_ = mapPx(y); editPrevA_ = editPrevB_ = -1; sprayMs_ = 0;
            editSample(a, b, mapPx(y));
            playSound(0x1FC, -1);   // 0x442040 -> 0x443400 / 0x4432B0 / 0x4442A0 / 0x4423E0: the loop plays while the mouse button is held (stopped by 0x424AF0)
            break;
        case ToolKind::Landscape: dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; landscapeAt(a, b); break;
        case ToolKind::Dispatch:
            if (placeDispatch(sim_.context(), ts_, t->sub, a, b)) { say("Dispatched"); static const int snd[3] = {0x1FA, 0x1FD, 0x20D}; playSound(snd[t->sub % 3]); }   // 0x44F4D0 / 0x44F6E0 / 0x44F8F0
            else { say("Can't dispatch there"); playSound(0x1F5); }
            break;
        case ToolKind::Terrain: playSound(0x1FC, -1); dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; levelTarget_ = sim_.world().altm.at(a, b) & 0x1F; terrainAt(a, b); break;
        case ToolKind::Zone: if (t->cat == 0) playSound(0x1FC, -1); [[fallthrough]];   // de-zone is a mode of the bulldozer drag 0x442760 (loop 0x1FC); zoning itself plays its sound at the end
        case ToolKind::Highway:
        case ToolKind::Network: dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; updatePreview(); break;
        case ToolKind::Sign: signAt(a, b); break;
        case ToolKind::Building:
        case ToolKind::Special:
        case ToolKind::Query: applyClick(a, b); break;
        case ToolKind::Unavailable: break;
    }
}

void GameUi::mouseUp(int x, int y, int button) {
    mouseX_ = x; mouseY_ = y;
    sliderDrag_ = -1;
    if (button != 1) { scrolling_ = false; return; }
    if (dragging_) { mouseMove(x, y); finishDrag(); }
    dragging_ = false;
}

void GameUi::mouseMove(int x, int y) {
    menu_.mouseMove(x, y);
    if (sliderDrag_ >= 0) { mapEditorMouse(x, y, false); mouseX_ = x; mouseY_ = y; return; }
    const int dx = x - mouseX_, dy = y - mouseY_;
    mouseX_ = x; mouseY_ = y;
    if (scrolling_) {   // the map moves in its own (magnified) pixels
        scrollRemX_ += dx / mag_; scrollRemY_ += dy / mag_;
        const int ix = static_cast<int>(scrollRemX_), iy = static_cast<int>(scrollRemY_);
        cam_.scrollX += ix; cam_.scrollY += iy; scrollRemX_ -= ix; scrollRemY_ -= iy;
        return;
    }
    int a, b;
    if (overUi(x, y) || !renderer_.pickTile(sim_.world(), cam_, mapPx(x), mapPx(y), a, b)) { hoverA_ = hoverB_ = -1; return; }
    hoverA_ = a; hoverB_ = b;
    if (dragging_ && tool() && tool()->kind == ToolKind::TerrainEdit) {
        if (tool()->sub >= 3) editSample(a, b, mapPx(y));
        lastA_ = a; lastB_ = b;
        return;
    }
    if (!dragging_ || (a == lastA_ && b == lastB_)) return;
    const ToolItem* t = tool();
    if (!t) return;
    if (t->kind == ToolKind::Bulldoze || t->kind == ToolKind::Terrain || t->kind == ToolKind::Landscape) {   // every tile the cursor crosses
        int ca = lastA_, cb = lastB_;
        while (ca != a || cb != b) {
            if (std::abs(a - ca) >= std::abs(b - cb)) ca += a > ca ? 1 : -1; else cb += b > cb ? 1 : -1;
            if (t->kind == ToolKind::Bulldoze) bulldozeAt(ca, cb); else if (t->kind == ToolKind::Terrain) terrainAt(ca, cb); else landscapeAt(ca, cb);
        }
    }
    lastA_ = a; lastB_ = b;
    if (t->kind != ToolKind::Bulldoze && t->kind != ToolKind::Terrain && t->kind != ToolKind::Landscape && t->kind != ToolKind::TerrainEdit) updatePreview();
}

void GameUi::wheel(int, int, int dy) {
    if (browser_.active()) { browser_.wheel(dy); return; }
    if (dy > 0) setZoomLevel(zoomLevel_ + 1);
    else if (dy < 0) setZoomLevel(zoomLevel_ - 1);
}

bool GameUi::keyDown(SDL_Keycode k, bool ctrl) {
    if (browser_.active()) { browser_.key(k); return true; }
    if (pauseOpen_ && !modal_) {
        if (k == SDLK_ESCAPE) pauseOpen_ = false;
        else if (k == SDLK_S) pauseClick(1);
        else if (k == SDLK_L) pauseClick(2);
        else if (k == SDLK_C) pauseClick(3);
        else if (k == SDLK_Q) pauseClick(4);
        else if (k == SDLK_R || k == SDLK_RETURN) pauseOpen_ = false;
        return true;
    }
    if (modal_) {
        if (k == SDLK_ESCAPE) modal_->result = -1;
        else if (k == SDLK_RETURN) modal_->result = 0;
        else if (k == SDLK_BACKSPACE && modal_->textInput && !modal_->text.empty()) modal_->text.pop_back();
        return true;
    }
    if (queryOpen_ && k == SDLK_ESCAPE) { queryOpen_ = false; return true; }
    if (queryOpen_ && micro_.valid && k == SDLK_N) { renameQueried(); return true; }
    if (ctrl && k == SDLK_N) { newCityDialog(); return true; }
    if (ctrl && k == SDLK_O && onTitle) { onTitle(); return true; }
    if (ctrl && ts_.shift && k == SDLK_D) { enableDebugMenu(); return true; }
    if (k >= SDLK_A && k <= SDLK_Z && !ctrl && !modal_) {   // the cheat word of the original: "priscilla" attaches the Debug menu
        static const std::string word = "priscilla";
        const std::string next = cheatTyped_ + static_cast<char>('a' + (k - SDLK_A));
        if (next.size() >= 2 && word.compare(0, next.size(), next) == 0) {   // swallowed, so the word does not rotate the view and so on
            cheatTyped_ = next;
            if (next == word) { cheatTyped_.clear(); layers.powerOverlay = !layers.powerOverlay; enableDebugMenu(); }   // the first letter 'p' had toggled the power overlay
            return true;
        }
        cheatTyped_ = next == "p" ? next : std::string();
    }
    if (ctrl && k == SDLK_S) { saveDialog(); return true; }
    if (ctrl && !modal_ && (k == SDLK_I || k == SDLK_H || k == SDLK_M)) { menuCommand(k == SDLK_I ? 32787 : k == SDLK_H ? 32789 : 32783); return true; }
    if (budget_.open) { if (k == SDLK_ESCAPE || k == SDLK_B) budget_.open = false; return true; }
    if (info_.open != InfoWindows::Which::None) { if (k == SDLK_ESCAPE && !info_.closeArticle()) info_.open = InfoWindows::Which::None; return true; }
    if (k == SDLK_M && audio) { audio->setMuted(!audio->muted()); say(audio->muted() ? "Sound off" : "Sound on"); return true; }
    if (k == SDLK_G) { info_.open = InfoWindows::Which::Graphs; return true; }
    if (k == SDLK_B) { budget_.open = true; return true; }
    if (k == SDLK_ESCAPE) {
        if (menuOpen_) menuOpen_ = false;
        else if (group_ >= 0) { group_ = item_ = -1; dragging_ = false; }
        else pauseOpen_ = true;   // Escape with nothing to cancel opens the pause menu
    } else if (k == SDLK_SPACE) clock_.speed = clock_.speed == GameSpeed::Paused ? GameSpeed::Llama : GameSpeed::Paused;
    else if (k >= SDLK_1 && k <= SDLK_5) clock_.speed = static_cast<GameSpeed>(k - SDLK_1 + 1);
    else if (k == SDLK_EQUALS || k == SDLK_KP_PLUS) setZoomLevel(zoomLevel_ + 1);
    else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) setZoomLevel(zoomLevel_ - 1);
    else if (k == SDLK_R) { cam_.rotation = (cam_.rotation + 1) & 3; cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, mapW() / 2, mapH() / 2); }
    else if (k == SDLK_U) layers.underground = !layers.underground;
    else if (k == SDLK_T) layers.streets = !layers.streets;
    else if (k == SDLK_P) layers.powerOverlay = !layers.powerOverlay;
    else if (k == SDLK_O) layers.waterOverlay = !layers.waterOverlay;
    return true;
}

void GameUi::newCity(int era, int difficulty, const TerrainParams* terrain) {
    if (defaultMapPath.empty()) { say("No default map available"); return; }
    CityFile map = CityFile::load(defaultMapPath);
    sim_.startNewCity(map, era, difficulty, terrain);
    base_ = map;
    savePath_.clear();
    info_.cityName = map.name();
    avail_ = ToolAvailability::fromState(sim_.state(), sim_.tables());
    cam_.rotation = 0;
    if (terrain) cam_.centerOn(0x40, 0x40, mapW() / 2, mapH() / 2);   // 0x412050(view, 0x40, 0x40) at the end of the generator
    else cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, mapW() / 2, mapH() / 2);
    group_ = item_ = -1; dragging_ = false; queryOpen_ = false;
    say("New city started");
}

void GameUi::loadCity(const CityFile& f) {
    base_ = f;
    savePath_.clear();
    avail_ = ToolAvailability::fromState(sim_.state(), sim_.tables());
    info_.cityName = f.name();
    queryOpen_ = false; dragging_ = false; group_ = item_ = -1; menuOpen_ = false;
    message_.clear();
}

void GameUi::setCityNameAll(const std::string& name) {   // the paper's '=' token, the saved CNAM and the default file name
    info_.cityName = name;
    base_.setName(name);
}

void GameUi::terrainProgress(int stage) {
    progressText_ = res_.string(801 + stage);
    if (!headless && pump) pump();   // redraw with the step name, like the generator's progress window
}

void GameUi::enterMapEditor() {
    if (defaultMapPath.empty()) { say("No default map available"); return; }
    CityFile map = CityFile::load(defaultMapPath);
    sim_.replaceCity(map);
    base_ = map;
    group_ = item_ = -1; dragging_ = false; queryOpen_ = false; menuOpen_ = false;
    mapEdit_ = true;
    sim_.state().treasury = 99999999;
    makeTerrain();
}

void GameUi::makeTerrain() {   // MAKE: a new random landscape from the sliders
    progressText_.clear();
    generateTerrain(sim_.context(), editParams_);
    cam_.rotation = 0;
    cam_.centerOn(0x40, 0x40, mapW() / 2, mapH() / 2);
    progressText_.clear();
    say(res_.string(413));   // "Editing Terrain..."
}

void GameUi::finishMapEditor() {   // DONE: the edited landscape becomes the new city (start year and difficulty, dialog 101)
    Modal year; year.title = "Starting year"; year.options = {"1900", "1950", "2000", "2050"};
    const int era = runModal(year);
    if (era < 0) return;
    Modal diff; diff.title = "Difficulty"; diff.options = {"Easy ($20,000)", "Medium ($10,000)", "Hard ($10K bond)"};
    const int d = runModal(diff);
    if (d < 0) return;
    std::string name = "New City";
    if (!askText("Name your city", name) || name.empty()) name = "New City";
    CityFile edited = sim_.toCity(base_);
    mapEdit_ = false;
    sim_.startNewCity(edited, era, d, nullptr);
    base_ = edited;
    avail_ = ToolAvailability::fromState(sim_.state(), sim_.tables());
    cam_.rotation = 0;
    cam_.centerOn(0x40, 0x40, mapW() / 2, mapH() / 2);
    group_ = item_ = -1; dragging_ = false; queryOpen_ = false;
    setCityNameAll(name);
    say("New city started");
}

bool GameUi::mapEditorMouse(int x, int y, bool press) {
    const Rect p = editPanel();
    if (!p.contains(x, y) && sliderDrag_ < 0) return false;
    const int trackX = p.x + 16, trackW = 196;
    int* vals[3] = {&editParams_.hills, &editParams_.water, &editParams_.trees};
    for (int i = 0; i < 3; ++i) {
        const Rect row{trackX - 4, p.y + 56 + i * 34, trackW + 8, 24};
        if ((press && row.contains(x, y)) || sliderDrag_ == i) {
            sliderDrag_ = i;
            *vals[i] = std::clamp((x - trackX) * 47 / trackW, 0, 47);
            return true;
        }
    }
    if (!press) return true;
    if (Rect{p.x + 16, p.y + 160, 100, 18}.contains(x, y)) editParams_.coast = !editParams_.coast;
    else if (Rect{p.x + 120, p.y + 160, 100, 18}.contains(x, y)) editParams_.river = !editParams_.river;
    else if (Rect{p.x + 16, p.y + 192, 90, 26}.contains(x, y)) makeTerrain();
    else if (Rect{p.x + 120, p.y + 192, 90, 26}.contains(x, y)) finishMapEditor();
    return true;
}

void GameUi::drawMapEditor(SDL_Renderer* r) {
    const Rect p = editPanel();
    fill(r, p.x, p.y, p.w, p.h, 28, 32, 44);
    frame(r, p.x, p.y, p.w, p.h, 200, 200, 220);
    const auto* dlg = res_.dialog(104);
    text(r, p.x + 16, p.y + 10, dlg && !dlg->controls.empty() ? "Terrain Generation" : "Terrain Generation", 255, 255, 190);
    text(r, p.x + 16, p.y + 28, progressText_.empty() ? res_.string(413) : progressText_, 190, 220, 160);
    static const char* names[3] = {"Hills", "Water", "Trees"};
    const int vals[3] = {editParams_.hills, editParams_.water, editParams_.trees};
    for (int i = 0; i < 3; ++i) {
        const int y = p.y + 56 + i * 34;
        text(r, p.x + 16, y, std::to_string(vals[i] * 100 / 47) + "% " + names[i], 235, 235, 235);
        fill(r, p.x + 16, y + 16, 196, 4, 80, 90, 120);
        fill(r, p.x + 16 + vals[i] * 196 / 47 - 3, y + 11, 7, 14, 230, 230, 140);
    }
    auto check = [&](int x, int y, bool on, const std::string& label) {
        frame(r, x, y + 2, 12, 12, 200, 200, 220);
        if (on) fill(r, x + 3, y + 5, 6, 6, 120, 230, 120);
        text(r, x + 18, y + 4, label, 235, 235, 235);
    };
    check(p.x + 16, p.y + 160, editParams_.coast, res_.string(791));
    check(p.x + 120, p.y + 160, editParams_.river, res_.string(792));
    fill(r, p.x + 16, p.y + 192, 90, 26, 40, 80, 56); frame(r, p.x + 16, p.y + 192, 90, 26, 190, 230, 200);
    text(r, p.x + 16 + 45 - 16, p.y + 201, res_.string(793), 255, 255, 220);
    fill(r, p.x + 120, p.y + 192, 90, 26, 80, 60, 40); frame(r, p.x + 120, p.y + 192, 90, 26, 230, 210, 190);
    text(r, p.x + 120 + 45 - 16, p.y + 201, res_.string(794), 255, 255, 220);
    text(r, p.x + 16, p.y + 228, "Use the tools on the left", 150, 160, 180);
}

void GameUi::newCityDialog(bool randomMap) {
    if (randomMap) { enterMapEditor(); return; }
    // the original: "Edit New Map" (terrain generation dialog 104) or the flat default map, then the start year and difficulty (dialog 101)
    Modal mapChoice; mapChoice.title = "New city: map"; mapChoice.options = {"Flat default map", "Random terrain (hills, water, trees)"};
    const int m = randomMap ? 1 : runModal(mapChoice);
    if (m < 0) return;
    TerrainParams tp;
    if (m == 1) {
        Modal shore; shore.title = "Landform"; shore.options = {"Inland", "Coast", "River", "Coast and river"};
        const int l = runModal(shore);
        if (l < 0) return;
        tp.coast = (l & 1) != 0; tp.river = (l & 2) != 0;
        auto level = [&](const char* title, int& out) {
            Modal q; q.title = title; q.options = {"0 %", "25 %", "50 %", "75 %", "100 %"};
            const int v = runModal(q);
            if (v < 0) return false;
            out = v * 47 / 4;
            return true;
        };
        if (!level("Hills", tp.hills) || !level("Water", tp.water) || !level("Trees", tp.trees)) return;
    }
    Modal year; year.title = "Starting year"; year.options = {"1900", "1950", "2000", "2050"};
    const int era = runModal(year);
    if (era < 0) return;
    Modal diff; diff.title = "Difficulty"; diff.options = {"Easy ($20,000)", "Medium ($10,000)", "Hard ($10K bond)"};
    const int d = runModal(diff);
    if (d < 0) return;
    std::string name = "New City";
    if (!askText("Name your city", name) || name.empty()) name = "New City";   // the start dialog 101 has a name field
    newCity(era, d, m == 1 ? &tp : nullptr);
    setCityNameAll(name);
}

// ---- tools -------------------------------------------------------------------------------------------------------------

void GameUi::editSample(int pa, int pb, int pointerY) {   // one iteration of a terrain-editor mouse loop
    const ToolItem* t = tool();
    SimContext& c = sim_.context();
    switch (t->sub) {
        case 1: terrainSprayRaise(c, ts_, startA_, startB_, pa, pb); break;
        case 2: ts_.dozerSub = 3; terrainSprayLower(c, ts_, pa, pb); break;
        case 3: terrainStretch(c, ts_, startA_, startB_, editStartAlt_, editLastAlt_, editStartY_ - pointerY, cam_.zoom); break;
        default:
            if (terrainLevelSample(c, ts_, pa, pb, editPrevA_, editPrevB_, editStartAlt_)) { editPrevA_ = pa; editPrevB_ = pb; }
            break;
    }
}

void GameUi::landscapeAt(int a, int b) {
    const ToolItem* t = tool();
    SimContext& c = sim_.context();
    // 0x441DF0 / 0x442040: trees and the editor's forest play 0x1F7, water and the editor's stream play 0x1FF (once per tile)
    if (t->sub == 2) { plantForest(c, ts_, a, b); playSound(0x1F7); }
    else if (t->sub == 3) { placeStream(c, ts_, a, b); playSound(0x1FF); }
    else if (!landscapeTool(c, ts_, t->sub, a, b)) { if (message_.empty()) say(sim_.state().treasury < unitCost(*t) ? "Not enough funds" : "Can't do that here"); }
    else playSound(t->sub == 1 ? 0x1FF : 0x1F7);
}

void GameUi::terrainAt(int a, int b) {
    const ToolItem* t = tool();
    ts_.dozerSub = t->sub;
    SimContext& c = sim_.context();
    bool ok;
    if (t->sub == 1) ok = levelTerrain(c, ts_, a, b, levelTarget_);
    else if (t->sub == 2) ok = raiseTerrain(c, ts_, a, b);
    else ok = lowerTerrain(c, ts_, a, b, true);
    if (!ok && message_.empty()) say("Can't change the terrain there");
}

void GameUi::bulldozeAt(int a, int b) {
    ts_.underground = layers.underground;
    const int before = sim_.state().treasury;
    if (!bulldozeTile(sim_.context(), ts_, a, b) && message_.empty()) say(sim_.state().treasury < toolCost(sim_.context(), 0, 0) ? "Not enough funds" : "Nothing to bulldoze here");
    (void)before;
}

std::pair<int, int> GameUi::footprint(const ToolItem& t, int a, int b, int& size) const {
    size = sim_.tables().toolBuildingSize[static_cast<size_t>(t.cat * 12 + t.sub)];
    if (size == 255 || size < 1) size = 1;
    const int sz = size - 1;
    if (sz > 1) return {a - 1, b - 1};
    return {a, b};
}

void GameUi::updatePreview() {
    const ToolItem* t = tool();
    preview_ = PathPreview{};
    previewCost_ = 0;
    if (!t || !dragging_) return;
    SimContext& c = sim_.context();
    if (t->kind == ToolKind::Highway) {
        ToolState scratch = ts_;
        World& w = sim_.world();
        const int ua = startA_ & ~1, ub = startB_ & ~1;
        bool wet = false;
        for (int x = ua; x <= ua + 1; ++x) for (int y = ub; y <= ub + 1; ++y) wet |= inMap(x, y) && (w.xbit.at(x, y) & xbit::kWater);
        if (wet) return;
        preview_ = previewHighway(c, scratch, ua, ub, lastA_, lastB_);
        previewCost_ = preview_.cost;
    } else if (t->kind == ToolKind::Network) {
        World& w = sim_.world();
        const bool surface = t->net != NetworkKind::Subway && t->net != NetworkKind::Pipe;
        if (surface && (w.xbit.at(startA_, startB_) & xbit::kWater) && w.xter.at(startA_, startB_) < 0x40) return;   // a click on the shore starts a bridge, no path
        ToolState scratch = ts_;
        preview_ = previewNetwork(c, scratch, t->net, startA_, startB_, lastA_, lastB_, unitCost(*t));
        previewCost_ = preview_.cost;
    } else if (t->kind == ToolKind::Zone) {
        previewCost_ = zoneRectCost(c, startA_, startB_, lastA_, lastB_, t->zone, unitCost(*t));
    }
}

void GameUi::finishDrag() {
    const ToolItem* t = tool();
    if (!t) return;
    stopSound(0x1FC);   // the drag loop of bulldozer / roads / rails / lines / pipes (0x424AD0 .. 0x424AF0)
    SimContext& c = sim_.context();
    const int funds = sim_.state().treasury;
    bool ok = false;
    if (t->kind == ToolKind::TerrainEdit) {
        if (t->sub == 4) terrainLevelEnd(c, editPrevA_, editPrevB_);
        stopSound(0x1FC);
        return;
    }
    if (t->kind == ToolKind::Highway) {
        ok = buildHighway(c, ts_, startA_, startB_, lastA_, lastB_);
        if (ok) say("Built for " + moneyText(funds - sim_.state().treasury));
        else say(previewCost_ > funds ? "Not enough funds (" + moneyText(previewCost_) + ")" : "Can't build a highway there");
    } else if (t->kind == ToolKind::Network) {
        ok = buildNetwork(c, ts_, t->net, t->sub, startA_, startB_, lastA_, lastB_);
        if (ok) say("Built for " + moneyText(funds - sim_.state().treasury));
        else say(previewCost_ > funds ? "Not enough funds (" + moneyText(previewCost_) + ")" : "Can't build that here");
    } else if (t->kind == ToolKind::Zone) {
        const int cost = zoneRectCost(c, startA_, startB_, lastA_, lastB_, t->zone, unitCost(*t));
        ok = paintZone(c, ts_, startA_, startB_, lastA_, lastB_, t->zone, unitCost(*t));
        if (ok) say("Zoned for " + moneyText(cost));
        else say(cost > funds ? "Not enough funds (" + moneyText(cost) + ")" : "Nothing to zone there");
    }
    // 0x43EBC0 after the drag: zones 0x1F7 (the port / airport zones 500), roads, rails, pipes, subways and highways 500, power lines 0x202; refused: 0x1F5
    if (t->kind == ToolKind::Zone && t->cat != 0) playSound(ok ? (t->cat == 8 ? 500 : 0x1F7) : 0x1F5);
    else if (t->kind == ToolKind::Network) playSound(ok ? (t->net == NetworkKind::Power ? 0x202 : 500) : 0x1F5);
    else if (t->kind == ToolKind::Highway) playSound(ok ? 500 : 0x1F5);
    preview_ = PathPreview{};
    previewCost_ = 0;
}

void GameUi::applyClick(int a, int b) {
    const ToolItem* t = tool();
    if (!t) return;
    if (t->kind == ToolKind::Query) { queryTile(a, b); return; }
    if (t->kind == ToolKind::Special) {
        SimContext& c = sim_.context();
        message_.clear();
        bool ok = false;
        if (t->cat == 3) ok = placeHydroDam(c, ts_, a, b);
        else if (t->cat == 6 && t->sub == 2) ok = buildTunnel(c, ts_, a, b, t->sub);
        else if (t->cat == 6) ok = placeOnramp(c, ts_, a, b, t->sub);
        else ok = placeRailLink(c, ts_, a, b);
        if (ok) say(std::string(t->name) + " built");
        else if (message_.empty()) say(std::string("Can't build: ") + t->name);
        playSound(ok ? 500 : 0x1F5);   // 0x43EBC0: 500 = built, 0x1F5 = refused
        return;
    }
    if (t->kind != ToolKind::Building) return;
    if (!available(*t)) { say(t->cat == 5 ? "This reward is not available yet (the city is too small, or it is already built)" : "Not invented yet"); return; }
    const int cost = unitCost(*t);
    if (sim_.state().treasury < cost) { say("Not enough funds (" + moneyText(cost) + ")"); return; }
    message_.clear();
    const bool built = placeBuilding(sim_.context(), ts_, t->cat, t->sub, a, b);
    if (built) say(std::string(t->name) + " built for " + moneyText(cost));
    else if (message_.empty()) say(std::string("Can't build the ") + t->name + " here");
    if (!built) playSound(0x1F5);
    else {   // success sound by category / subtype, 0x43EBC0 cases 3..14
        int snd = 500;
        switch (t->cat) {
            case 5: snd = 0x201; break;
            case 6: if (t->sub == 4) snd = 0x209; break;
            case 7: if (t->sub == 2) snd = 0x20C; break;
            case 12: snd = 0x20B; break;
            case 13: snd = t->sub == 1 ? 0x1FD : t->sub == 3 ? 0x20A : 0x1FA; break;
            case 14: snd = t->sub == 2 ? 0x20F : 0x201; break;
            default: break;
        }
        playSound(snd);
        if (t->cat == 14) playMusic(0x271A);   // 0x424B30: the reward fanfare
    }
}

void GameUi::queryTile(int a, int b) {
    pokeHelicopter(sim_.context(), a, b);
    query_ = ::sc2k::queryTile(sim_.context(), a, b);
    micro_ = microsimInfo(sim_.context(), a, b);
    if (!micro_.valid) playSound(0x1F9);   // 0x43EBC0 case 0x11: the query click
    queryOpen_ = true;
}

void GameUi::renameQueried() {
    std::string name = micro_.name;
    if (!askText("Building name", name)) return;
    if (renameBuilding(sim_.context(), query_.a, query_.b, name)) micro_ = microsimInfo(sim_.context(), query_.a, query_.b);
}

const uint8_t* GameUi::labelTemplate(int id, bool custom) {   // 0x44FB2F builds the table: string 831 + n names building 0xC6 + n
    if (custom || id < 0xC6 || id > 0xFF) return nullptr;
    const std::string s = res_.string(831 + id - 0xC6);
    templateName_.fill(0);
    std::memcpy(templateName_.data(), s.data(), std::min<size_t>(s.size(), 24));
    return templateName_.data();
}

void GameUi::signAt(int a, int b) {
    SimContext& c = sim_.context();
    std::string text = signText(c, a, b);
    message_.clear();
    const int v = sim_.world().xtxt.at(a, b);
    if (v >= 0x33) { say(res_.string(0x108)); return; }
    const bool ok = askText("Enter sign text", text);
    if (placeSign(c, a, b, text, !ok)) say(ok && !text.empty() ? "Sign placed" : "Sign removed");
    else if (message_.empty()) say("Can't place a sign here");
}

bool GameUi::askText(const std::string& title, std::string& text) {
    Modal m;
    m.title = title;
    m.textInput = true;
    m.text = text;
    if (headless || !pump) return true;
    modal_ = &m;
    SDL_StartTextInput(SDL_GetKeyboardFocus());
    while (m.result == -2) if (!pump()) { m.result = -1; break; }
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    modal_ = nullptr;
    text = m.text;
    return m.result == 0;
}

void GameUi::textInput(const std::string& utf8) {
    if (browser_.active()) { browser_.textInput(utf8); return; }
    if (modal_ && modal_->textInput) for (char ch : utf8) if (ch >= 32 && ch < 127 && modal_->text.size() < 24) modal_->text.push_back(ch);
}

void GameUi::dragTool(int a0, int b0, int a1, int b1) {
    const ToolItem* t = tool();
    if (!t) return;
    startA_ = lastA_ = a0; startB_ = lastB_ = b0;
    if (t->kind == ToolKind::Building || t->kind == ToolKind::Query || t->kind == ToolKind::Special) { applyClick(a0, b0); return; }
    dragging_ = true;
    if (t->kind == ToolKind::Terrain) {
        levelTarget_ = sim_.world().altm.at(a0, b0) & 0x1F;
        int ca = a0, cb = b0;
        terrainAt(ca, cb);
        while (ca != a1 || cb != b1) {
            if (std::abs(a1 - ca) >= std::abs(b1 - cb)) ca += a1 > ca ? 1 : -1; else cb += b1 > cb ? 1 : -1;
            terrainAt(ca, cb);
        }
    } else if (t->kind == ToolKind::Bulldoze) {
        int ca = a0, cb = b0;
        bulldozeAt(ca, cb);
        while (ca != a1 || cb != b1) {
            if (std::abs(a1 - ca) >= std::abs(b1 - cb)) ca += a1 > ca ? 1 : -1; else cb += b1 > cb ? 1 : -1;
            bulldozeAt(ca, cb);
        }
    } else {
        lastA_ = a1; lastB_ = b1;
        updatePreview();
        finishDrag();
    }
    dragging_ = false;
}

std::string GameUi::savesDir() {
    const std::string p = AudioSettings::settingsPath();
    const std::string dir = p.substr(0, p.rfind('/')) + "/saves";
    for (size_t i = 1; i <= dir.size(); ++i) if (i == dir.size() || dir[i] == '/') mkdir(dir.substr(0, i).c_str(), 0755);
    return dir;
}

void GameUi::saveDialog() {
    if (!savePath_.empty()) { say(saveCity(savePath_) ? "Saved " + savePath_ : "Could not save " + savePath_); return; }
    saveAs();
}

void GameUi::saveAs() {
    std::string name = info_.cityName.empty() ? "city" : info_.cityName;
    for (char& c : name) if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';
    std::string dir = savePath_.empty() ? savesDir() : savePath_.substr(0, savePath_.rfind('/'));
    if (headless || !pump) {
        const std::string path = dir + "/" + name + ".sc2";
        say(saveCity(path) ? "Saved " + path : "Could not save " + path);
        return;
    }
    browser_.open("Save City As", dir, name);
    SDL_StartTextInput(SDL_GetKeyboardFocus());
    while (browser_.result() == -2) if (!pump()) { browser_.close(); break; }
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    if (browser_.result() != 0) return;
    const std::string path = browser_.path();
    say(saveCity(path) ? "Saved " + path : "Could not save " + path);
}

void GameUi::pauseClick(int i) {
    switch (i) {
        case 0: pauseOpen_ = false; break;
        case 1: pauseOpen_ = false; saveDialog(); break;
        case 2: pauseOpen_ = false; if (onPauseAction) onPauseAction(1); break;
        case 3: pauseOpen_ = false; if (onPauseAction) onPauseAction(2); break;
        default: pauseOpen_ = false; if (onPauseAction) onPauseAction(3); break;
    }
}

void GameUi::drawPause(SDL_Renderer* r) {
    fill(r, 0, 0, width_, height_, 0, 0, 0, 140);
    text(r, width_ / 2 - 40, height_ / 2 - 160, "Game paused", 255, 255, 190);
    static const char* labels[5] = {"Resume", "Save game", "Load game", "Settings", "Exit"};
    for (int i = 0; i < 5; ++i) {
        const Rect b{width_ / 2 - 130, height_ / 2 - 130 + i * 48, 260, 38};
        fill(r, b.x, b.y, b.w, b.h, 40, 56, 96);
        frame(r, b.x, b.y, b.w, b.h, 190, 200, 230);
        text(r, b.x + 16, b.y + 15, labels[i], 255, 255, 220);
    }
}

bool GameUi::saveCity(const std::string& path) {
    try {
        const std::vector<uint8_t> bytes = sim_.toCity(base_).serialize();
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
        const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        std::fclose(f);
        if (ok) savePath_ = path;
        return ok;
    } catch (const std::exception&) { return false; }
}

// ---- questions asked by the tool code ------------------------------------------------------------------------------------

int GameUi::runModal(Modal& m) {
    if (headless || !pump) return 0;
    modal_ = &m;
    while (m.result == -2) {
        if (!pump()) { m.result = -1; break; }
    }
    modal_ = nullptr;
    return m.result;
}

bool GameUi::confirm(int id, int amount) {
    Modal m;
    if (id == 0x6D || id == 0x6E) {
        const int fee = id == 0x6D ? 1000 : 1500;
        m.title = res_.string(id) + " to the neighbouring city for " + moneyText(fee) + "?";
    } else if (id == 0x121 || id == 0x123) {
        std::string t = res_.string(id);
        const size_t p = t.find("%d");
        if (p != std::string::npos) t.replace(p, 2, std::to_string(amount));
        for (size_t q; (q = t.find('\n')) != std::string::npos;) t.replace(q, 1, " - ");
        for (size_t q; (q = t.find('\r')) != std::string::npos;) t.erase(q, 1);
        m.title = t;
    } else if (id == 0x15A) m.title = "Build the tunnel for " + moneyText(amount) + "?";
    else m.title = "Are you sure?";
    m.options = {"Yes", "No"};
    return runModal(m) == 0;
}

int GameUi::chooseBridge(int length, int allowedMask) {
    Modal m;
    m.title = "Choose the crossing (" + std::to_string(length) + " tiles)";
    std::vector<int> types;
    for (int type = 0; type < 5; ++type) {
        if (!(allowedMask & (1 << type))) continue;
        int32_t perTile;
        std::memcpy(&perTile, &sim_.tables().bridgeData[static_cast<size_t>(4 * type)], 4);
        types.push_back(type);
        m.options.push_back(res_.string(247 + type) + "  " + moneyText(perTile * length));
    }
    if (types.empty()) return -1;
    m.options.push_back("Cancel");
    const int r = runModal(m);
    return r >= 0 && r < static_cast<int>(types.size()) ? types[static_cast<size_t>(r)] : -1;
}

void GameUi::toolMessage(int id) {
    std::string s = res_.string(id);
    say(s.empty() ? "(message " + std::to_string(id) + ")" : s);
}

bool GameUi::available(const ToolItem& t) const {
    if (t.kind == ToolKind::Unavailable) return false;
    if (t.kind == ToolKind::Query || t.kind == ToolKind::Bulldoze || t.kind == ToolKind::Terrain || t.kind == ToolKind::TerrainEdit || t.kind == ToolKind::Landscape) return true;
    if (t.kind == ToolKind::Dispatch) return emergency_;
    if (t.kind == ToolKind::Zone && t.cat == 0) return true;   // de-zone
    if (t.kind == ToolKind::Building && t.cat == 5) {   // rewards: granted by the city's growth (0x413FE0 -> 0x441800), one of each until it is demolished
        const int m = sim_.state().milestone;
        if (t.sub < 4) {
            static const int need[4] = {1, 2, 3, 5};   // city class after crossing 2,000 / 10,000 / 30,000 / 90,000 inhabitants
            if (m < need[t.sub] || sim_.state().census[static_cast<size_t>(toolBuildingId(5, t.sub)) & 0xFF] > 0) return false;
        } else if (m < 6) return false;               // arcologies need a city class 6 (120,000 inhabitants) and the invented design
    }
    return avail_.allows(t.cat, t.sub);
}

void GameUi::scheduledEvent(int index) {
    avail_.applyEvent(index, sim_.state());
    say(index <= 11 ? "A new tool has been invented!" : "A new arcology design is available!");
}

void GameUi::postMessage(int id, int arg) { info_.postMessage(id, arg); }
void GameUi::showDialog(int id) { toolMessage(id); }
ZoneHooks::StadiumChoice GameUi::chooseStadiumTeam(int takenMask) {   // dialog 190 "Pick a Team and Name It"
    StadiumChoice out;
    std::vector<int> sports;
    for (int k = 0; k < 5; ++k) if (takenMask == 0x1F || !(takenMask & (1 << k))) sports.push_back(k);
    if (headless || !pump) { out.sport = sports.empty() ? 0 : sports.front(); return out; }
    for (;;) {
        Modal m; m.title = "Pick a Team and Name It: team sport";
        for (int k : sports) m.options.push_back(res_.string(786 + k));
        const int r = runModal(m);
        if (r < 0) return out;
        std::string name;
        if (!askText("Team name", name)) continue;
        out.sport = sports[static_cast<size_t>(r)];
        out.name = name;
        return out;
    }
}
// Zoom levels: the three sprite sets of the original (8, 16, 32 px tiles) and then the large set magnified 1.5x, 2x, 3x, 4x
namespace { const struct { int sprites; float mag; } kZoomLevels[GameUi::kZoomLevelCount] = {{0, 1.f}, {1, 1.f}, {2, 1.f}, {2, 1.5f}, {2, 2.f}, {2, 3.f}, {2, 4.f}}; }

void GameUi::setZoomLevel(int level) {
    level = std::clamp(level, 0, kZoomLevelCount - 1);
    if (level == zoomLevel_ && cam_.zoom == kZoomLevels[level].sprites) return;
    int a = -1, b = -1;
    const bool have = renderer_.pickTile(sim_.world(), cam_, mapW() / 2, mapH() / 2, a, b);   // keep the tile in the middle of the view
    zoomLevel_ = level;
    cam_.zoom = kZoomLevels[level].sprites;
    mag_ = kZoomLevels[level].mag;
    if (have) cam_.centerOn(a, b, mapW() / 2, mapH() / 2);
}

void GameUi::scrollTo(int a, int b) { if (autoGoto_) centerView(a, b); }
void GameUi::gameOver(int kind) {   // 0x414470: strings 285 (won), 286 (time ran out), 287 (bankrupt)
    static const int kString[3] = {286, 285, 287};
    const std::string text = res_.string(kString[kind < 0 || kind > 2 ? 2 : kind]);
    if (headless || !pump) { say(text); return; }
    Modal m; m.title = text; m.options = {"OK"};
    runModal(m);
}
void GameUi::playSound(int id, int repeat) { if (audio) audio->playEffect(id, repeat); }
void GameUi::stopSound(int id) { if (audio) audio->stopEffect(id); }
void GameUi::yearlyBudget() {   // 0x4736C0: one of four tunes while the annual budget is on the desk (the original picks with the LFSR; the port must not disturb the simulation's sequence)
    static const int kTunes[4] = {10016, 10005, 10002, 10010};
    static unsigned pick = 0;
    if (autoBudget_) return;   // 0x4736C0: with Auto-Budget the dialog and its tune are skipped
    stopSound(-1);
    playMusic(kTunes[(sim_.state().day / 300 + pick++) % 4]);
    if (!headless) budget_.open = true;
}
void GameUi::playMusic(int id) { if (audio) audio->playMusic(id); }

// ---- drawing -----------------------------------------------------------------------------------------------------------

void GameUi::overlay(Framebuffer& fb) {
    const World& w = sim_.world();
    const ToolItem* t = tool();
    if (preview_.valid && t && t->kind == ToolKind::Highway) {
        for (const auto& p : preview_.tiles)
            for (int dx = 0; dx < 2; ++dx) for (int dy = 0; dy < 2; ++dy) renderer_.highlightTile(fb, w, cam_, p.first + dx, p.second + dy, 0xFFFFFF60u, 110);
    } else if (preview_.valid) {
        for (const auto& p : preview_.tiles) renderer_.highlightTile(fb, w, cam_, p.first, p.second, 0xFFFFFF60u, 110);
    } else if (dragging_ && t && (t->kind == ToolKind::Network || t->kind == ToolKind::Highway)) {
        renderer_.highlightTile(fb, w, cam_, startA_, startB_, 0xFFFF3030u, 140);   // path not possible
    }
    if (dragging_ && t && t->kind == ToolKind::Zone) {
        static const uint32_t colors[10] = {0xFF909090u, 0xFF40D040u, 0xFF40D040u, 0xFF4060FFu, 0xFF4060FFu, 0xFFE0D030u, 0xFFE0D030u, 0xFF909090u, 0xFFC040C0u, 0xFF40C0C0u};
        for (int a = std::min(startA_, lastA_); a <= std::max(startA_, lastA_); ++a)
            for (int b = std::min(startB_, lastB_); b <= std::max(startB_, lastB_); ++b)
                renderer_.highlightTile(fb, w, cam_, a, b, colors[t->zone % 10], 90);
    }
    if (hoverA_ >= 0 && t && !dragging_) {
        if (t->kind == ToolKind::Building) {
            int size;
            const auto o = footprint(*t, hoverA_, hoverB_, size);
            for (int a = o.first; a < o.first + size; ++a)
                for (int b = o.second; b < o.second + size; ++b)
                    if (inMap(a, b)) renderer_.highlightTile(fb, w, cam_, a, b, 0xFFFFFFFFu, 80);
        } else renderer_.highlightTile(fb, w, cam_, hoverA_, hoverB_, 0xFFFFFFFFu, 255, true);
    } else if (hoverA_ >= 0 && dragging_) renderer_.highlightTile(fb, w, cam_, hoverA_, hoverB_, 0xFFFFFFFFu, 255, true);
}

void GameUi::draw(SDL_Renderer* r) {
    if (pauseOpen_ && !modal_) { drawSigns(r); drawPause(r); return; }
    drawSigns(r);
    const CityState& s = sim_.state();
    // top bar
    fill(r, 0, 0, width_, kTopBar, 20, 24, 32);
    char top[200];
    std::snprintf(top, sizeof top, "%s %d   Pop %u   Funds %s", kMonths[s.month()], s.startYear + s.day / 300, s.population10, mapEdit_ ? "-" : moneyText(s.treasury).c_str());
    const int demandX = 6 + static_cast<int>(std::strlen(top)) * 8 + 24;   // the residential / commercial / industrial demand bars (green, blue, yellow) follow the numbers
    char speed[40];
    std::snprintf(speed, sizeof speed, "[%s]", kSpeedNames[static_cast<int>(clock_.speed)]);
    const int buttonsClip = width_ - kTopButtonCount * kTopButtonStep - 4;
    {   // narrow windows (large UI scales): the status line stops where the buttons start
        const SDL_Rect clip{0, kMenuBar, std::max(0, buttonsClip - 4), kTopBar - kMenuBar};
        SDL_SetRenderClipRect(r, &clip);
        text(r, 6, 10 + kMenuBar, top);
        static const Uint8 col[3][3] = {{0, 200, 0}, {60, 120, 255}, {240, 220, 0}};
        static const char* letters[3] = {"R", "C", "I"};
        const int mid = kMenuBar + 14;
        for (int i = 0; i < 3; ++i) {
            const int bx = demandX + i * 40;
            text(r, bx, mid - 4, letters[i], 200, 205, 220);
            fill(r, bx + 12, mid, 16, 1, 120, 125, 140);                                    // zero line
            const int h = std::clamp(s.demand[static_cast<size_t>(i)] * 13 / 2000, -13, 13);   // demand runs from -2000 to +2000
            if (h > 0) fill(r, bx + 12, mid - h, 16, h, col[i][0], col[i][1], col[i][2]);
            else if (h < 0) fill(r, bx + 12, mid + 1, 16, -h, col[i][0] / 2, col[i][1] / 2, col[i][2] / 2);
        }
        text(r, demandX + 124, 10 + kMenuBar, speed);
        SDL_SetRenderClipRect(r, nullptr);
    }
    std::string right = message_;
    if (dragging_ && previewCost_ > 0) right = "Cost " + moneyText(previewCost_);
    const int buttonsX = width_ - kTopButtonCount * kTopButtonStep - 4;
    for (int k = 0; k < kTopButtonCount; ++k) drawIcon(r, 1, kTopButtons[k], buttonsX + k * kTopButtonStep + 2, 2 + kMenuBar);
    if (!right.empty()) {
        const size_t room = static_cast<size_t>(std::max(0, (buttonsX - 500 - 8) / 8));
        if (right.size() > room) right.resize(room);
        text(r, std::max(500, buttonsX - static_cast<int>(right.size()) * 8 - 8), 10 + kMenuBar, right, 255, 220, 120);
    }

    // tool groups
    const auto& cat = toolCatalog();
    for (size_t g = 0; g < cat.size(); ++g) {
        const Rect rc = groupRect(static_cast<int>(g));
        const bool sel = static_cast<int>(g) == group_;
        fill(r, rc.x, rc.y, rc.w, rc.h, sel ? 60 : 34, sel ? 70 : 38, sel ? 100 : 48);
        if (!groupAllowed(g)) fill(r, rc.x, rc.y, rc.w, rc.h, 0, 0, 0, 150);
        frame(r, rc.x, rc.y, rc.w, rc.h, 12, 14, 20);
        const IconRef ic = groupIcon(cat[g].name);
        drawIcon(r, ic.strip, ic.cell, rc.x + 2, rc.y + 1);
        text(r, rc.x + 32, rc.y + 9, cat[g].name);
    }
    if (const ToolItem* t = tool()) {
        const int y = kTopBar + static_cast<int>(cat.size()) * kGroupH + 6;
        fill(r, 0, y, kGroupW, 34, 30, 34, 44);
        text(r, 4, y + 4, t->name, 255, 255, 160);
        const int c = unitCost(*t);
        if (c >= 0) text(r, 4, y + 18, moneyText(c) + (t->kind == ToolKind::Network || t->kind == ToolKind::Zone || t->kind == ToolKind::Terrain || t->kind == ToolKind::Bulldoze ? " / tile" : t->kind == ToolKind::Highway ? " / piece" : ""), 200, 220, 200);
    }
    if (menuOpen_ && group_ >= 0) {
        const auto& g = cat[static_cast<size_t>(group_)];
        const Rect first = itemRect(0);
        fill(r, first.x - 1, first.y - 1, kItemW + 2, static_cast<int>(g.items.size()) * kItemH + 2, 12, 14, 20);
        for (size_t i = 0; i < g.items.size(); ++i) {
            const Rect rc = itemRect(static_cast<int>(i));
            const bool sel = static_cast<int>(i) == item_;
            const bool dead = !available(g.items[i]);
            fill(r, rc.x, rc.y, rc.w, rc.h, sel ? 64 : 38, sel ? 76 : 42, sel ? 110 : 54);
            const Uint8 shade = dead ? 120 : 235;
            text(r, rc.x + 6, rc.y + 5, g.items[i].name, shade, shade, shade);
            const int c = toolCost(sim_.context(), g.items[i].cat, g.items[i].sub);
            if (c >= 0 && g.items[i].kind != ToolKind::Query) {
                const std::string ct = moneyText(c);
                text(r, rc.x + rc.w - static_cast<int>(ct.size()) * 8 - 6, rc.y + 5, ct, shade, shade, dead ? 120 : 160);
            }
        }
    }
    // bottom bar: view buttons
    fill(r, 0, height_ - kBottomBar, width_, kBottomBar, 20, 24, 32);
    auto button = [&](int x, int w, const char* label, bool on) {
        fill(r, x, height_ - kBottomBar + 2, w, kBottomBar - 4, on ? 70 : 40, on ? 100 : 46, on ? 70 : 56);
        text(r, x + 6, height_ - kBottomBar + 7, label);
    };
    button(4, 92, layers.underground ? "Underground" : "Surface", layers.underground);
    button(104, 72, "Power", layers.powerOverlay);
    button(184, 72, "Water", layers.waterOverlay);
    button(264, 72, "Streets", layers.streets);   // underground view: show / hide the roads and rails of the surface
    const SDL_Rect hintClip{346, height_ - kBottomBar, std::max(0, width_ - 346), kBottomBar};
    SDL_SetRenderClipRect(r, &hintClip);
    text(r, 346, height_ - kBottomBar + 7, "Space pause  1-5 speed  +/- wheel zoom  R rotate  U/P/O/T views  right-drag scroll  B budget  G graphs  Ctrl+S save  Esc", 150, 160, 180);
    SDL_SetRenderClipRect(r, nullptr);
    budget_.draw(r, width_, height_);
    info_.draw(r, width_, height_);
    if (mapEdit_) drawMapEditor(r);
    menu_.draw(r, width_);
    if (queryOpen_) drawQuery(r);
    if (modal_) drawModal(r);
    if (browser_.active()) browser_.draw(r, width_, height_);
}

void GameUi::drawIcon(SDL_Renderer* r, int strip, int cell, int x, int y) {
    if (strip == 0 || cell < 0) return;
    try {
        if (!toolbarIcons_) {
            toolbarIcons_ = std::make_unique<IconStrip>(r, Bmp8::fromDib(res_.bitmap(2)), 23, true);
            terrainIcons_ = std::make_unique<IconStrip>(r, Bmp8::fromDib(res_.bitmap(207)), 19, false);
            bookIcon_ = std::make_unique<IconStrip>(r, Bmp8::fromDib(res_.namedBitmap("BOOKU")), 30, false);
            budget_.bookIcon = bookIcon_.get();
        }
    } catch (const std::exception&) { strip = 0; }
    if (strip == 1 && toolbarIcons_) toolbarIcons_->draw(r, cell, x, y);
    else if (strip == 2 && terrainIcons_) terrainIcons_->draw(r, cell, x + 2, y + 2);
}

void GameUi::drawSigns(SDL_Renderer* r) {
    if (cam_.zoom == 0) return;
    const World& w = sim_.world();
    for (int a = 0; a < 128; ++a)
        for (int b = 0; b < 128; ++b) {
            const int v = w.xtxt.at(a, b);
            if (v == 0 || v >= 0x33) continue;
            int x, y;
            renderer_.tileScreenPos(w, cam_, a, b, x, y);
            x = static_cast<int>(x * mag_); y = static_cast<int>(y * mag_);
            if (x < -200 || y < 0 || x > width_ + 200 || y > height_) continue;
            const std::string label = signText(sim_.context(), a, b);
            if (label.empty()) continue;
            const int tw = static_cast<int>(label.size()) * 8 + 6;
            fill(r, x - tw / 2, y - 22, tw, 12, 250, 250, 220);
            frame(r, x - tw / 2, y - 22, tw, 12, 60, 40, 20);
            text(r, x - tw / 2 + 3, y - 20, label, 40, 20, 10);
        }
}

void GameUi::drawQuery(SDL_Renderer* r) {
    if (micro_.valid) {
        std::vector<std::string> lines;
        const auto str = [this](int id) { return res_.string(id); };
        for (int id : micro_.lineStrings) if (id >= 0) lines.push_back(formatMicrosimLine(micro_, res_.string(id), str));
        const int bw = 360, bh = 52 + 16 * static_cast<int>(lines.size()) + 22;
        const int bx = std::min(std::max(0, mouseX_ + 20), width_ - bw), by = std::min(std::max(kTopBar, mouseY_ - bh / 2), height_ - kBottomBar - bh);
        fill(r, bx, by, bw, bh, 250, 246, 210);
        frame(r, bx, by, bw, bh, 70, 50, 20);
        std::string name = micro_.name.empty() ? res_.string(query_.nameString) : micro_.name;
        text(r, bx + 10, by + 10, name, 20, 10, 0);
        text(r, bx + 10, by + 26, res_.string(query_.nameString), 110, 90, 60);
        int y = by + 46;
        for (const std::string& l : lines) { text(r, bx + 10, y, l, 40, 20, 0); y += 16; }
        text(r, bx + 10, y + 4, "N renames the building   Esc closes", 120, 100, 80);
        return;
    }
    const int bw = 360, bh = 40 + 14 * static_cast<int>(query_.lines.size()) + 20;
    int bx = std::min(std::max(0, mouseX_ + 20), width_ - bw), by = std::min(std::max(kTopBar, mouseY_ - bh / 2), height_ - kBottomBar - bh);
    fill(r, bx, by, bw, bh, 250, 246, 210);
    frame(r, bx, by, bw, bh, 70, 50, 20);
    std::string name = res_.string(query_.nameString);
    if (name.empty()) name = "(unnamed)";
    if (query_.overlay >= 1 && query_.overlay < 0x33) { const std::string sg = signText(sim_.context(), query_.a, query_.b); if (!sg.empty()) name += " - sign: " + sg; }
    text(r, bx + 10, by + 10, name, 20, 10, 0);
    int y = by + 28;
    for (const QueryLine& l : query_.lines) {
        std::string label = l.labelString >= 0 ? res_.string(l.labelString) : std::string();
        text(r, bx + 10, y, label, 60, 40, 20);
        std::string value;
        bool warn = false;
        for (const QueryPiece& p : l.value) {
            if (!value.empty()) value += " ";
            value += p.stringId >= 0 ? res_.string(p.stringId) : p.text;
            warn |= p.warn;
        }
        text(r, bx + 130, y, value, warn ? 200 : 20, warn ? 20 : 10, 0);
        y += 14;
    }
    text(r, bx + 10, y + 4, "(" + std::to_string(query_.a) + ", " + std::to_string(query_.b) + ")  Esc closes", 120, 100, 80);
}

void GameUi::drawModal(SDL_Renderer* r) {
    const int n = static_cast<int>(modal_->options.size());
    const int bx = (width_ - 520) / 2, by = (height_ - (70 + 22 * n)) / 2;
    fill(r, 0, 0, width_, height_, 0, 0, 0, 120);
    fill(r, bx, by, 520, 70 + 22 * n, 28, 32, 44);
    frame(r, bx, by, 520, 70 + 22 * n, 200, 200, 220);
    text(r, bx + 20, by + 22, modal_->title, 255, 255, 190);
    if (modal_->textInput) { fill(r, bx + 20, by + 48, 480, 20, 12, 14, 20); text(r, bx + 26, by + 54, modal_->text + "_", 255, 255, 255); }
    for (int i = 0; i < n; ++i) {
        fill(r, bx + 20, by + 50 + 22 * i, 480, 20, 60, 70, 100);
        text(r, bx + 28, by + 56 + 22 * i, modal_->options[static_cast<size_t>(i)]);
    }
}


// ---- menu bar -----------------------------------------------------------------------------------------------------------

void GameUi::buildMenu() {
    const auto it = res_.menus().find("#3");
    if (it == res_.menus().end()) return;
    PeResources::MenuItem root = it->second;
    if (!root.items.empty() && root.items[0].items.size() >= 2) {   // File: "Rename City..." before the separator and Exit
        PeResources::MenuItem rename; rename.id = 60001; rename.text = "&Rename City...";
        root.items[0].items.insert(root.items[0].items.end() - 2, rename);
    }
    if (debugMenu_) if (const auto dbg = res_.menus().find("#223"); dbg != res_.menus().end() && !dbg->second.items.empty()) {
        PeResources::MenuItem d = dbg->second.items.front();
        d.text = "De&bug";
        root.items.insert(root.items.end() - 1, d);   // before Help, like the original's menu position 6
    }
    menu_.load(root);
}

void GameUi::enableDebugMenu() {
    if (debugMenu_) return;
    debugMenu_ = true;
    buildMenu();
    say("Debug menu enabled");
}

void GameUi::debugCommand(int id) {   // resource 223; the handlers are at 0x4026CB .. 0x402635
    SimContext& c = sim_.context();
    CityState& s = sim_.state();
    const int ca = s.centerA2 / 2, cb = s.centerB2 / 2;
    auto rnd = [&](int m) { return static_cast<int>(static_cast<unsigned>(c.rng.next() & 0x7FFF) % static_cast<unsigned>(m)); };
    auto unlockAll = [&] {   // every dated event counts as fired: all power plants, airport, highways, subway, treatment plants and the arcologies
        s.eventYears.fill(0);
        avail_ = ToolAvailability::fromState(s, sim_.tables());
        playSound(0x201);
    };
    switch (id) {
        case 32908: say("SimCity 2000 port " __DATE__); break;                   // Show Version Info (the original shows _version.inf)
        case 32909: s.treasury += 500000; break;                                  // More Money
        case 32910: unlockAll(); s.treasury += 500000; break;                     // Add All Gifts (all events + the council's $500,000)
        case 32911: unlockAll(); break;                                           // Add All Inventions
        case 32926: requestDisaster(9, 0, 0); break;                              // Melt Down
        case 32927: requestDisaster(10, 0, 0); break;                             // Microwave
        case 32923: { const int y = rnd(0x7E) + 1; const int x = rnd(0x7E) + 1; requestDisaster(0xB, x, y); break; }   // Volcano
        case 32925: requestDisaster(0xC, ca, cb); break;                          // Fire Storm
        case 32922: requestDisaster(0xD, ca, cb); break;                          // Mass Riots
        case 32928: { const int y = rnd(0x7E) + 1; const int x = rnd(0x7E) + 1; requestDisaster(0xE, x, y); break; }   // Major Flood
        case 32924: requestDisaster(4, ca, cb); break;                            // Toxic Spill
        case 32931: info_.open = InfoWindows::Which::Graphs; break;               // Graph Kludge (rescales the graphs and opens them)
        default: break;
    }
}

bool GameUi::menuChecked(int id) const {
    switch (id) {
        case 32768: return clock_.speed == GameSpeed::Paused;
        case 32769: return clock_.speed == GameSpeed::Turtle;
        case 32770: return clock_.speed == GameSpeed::Llama;
        case 32771: return clock_.speed == GameSpeed::Cheetah;
        case 32934: return clock_.speed == GameSpeed::AfricanSwallow;
        case 32772: return autoBudget_;
        case 32773: return autoGoto_;
        case 32774: return soundOn_;
        case 32775: return musicOn_;
        case 32782: return sim_.state().cb4ec != 0;
        case 32791: return info_.subscription;
        case 32792: return info_.extraEditions;
        default: return false;
    }
}

bool GameUi::menuEnabled(int id) const {
    return id != 32917;   // Load Tile Set: the port has no alternative tile sets
}

void GameUi::requestDisaster(int kind, int a, int b) {
    ThingState& th = sim_.context().things;
    th.disasterRequest = kind;
    th.focusA = a; th.focusB = b;
}

void GameUi::menuCommand(int id) {   // the original's WM_COMMAND handlers (message map at 0x4D2C48)
    SimContext& c = sim_.context();
    const int ca = sim_.state().centerA2 / 2, cb = sim_.state().centerB2 / 2;
    auto rnd = [&](int m) { return static_cast<int>(static_cast<unsigned>(c.rng.next() & 0x7FFF) % static_cast<unsigned>(m)); };
    switch (id) {
        case 32801: if (onPauseAction) onPauseAction(1); break;                  // Load City
        case 32802: newCityDialog(false); break;                                  // New City
        case 32803: newCityDialog(true); break;                                   // Edit New Map
        case 32804: if (onPauseAction) onPauseAction(4); break;                  // Load Scenario
        case 32917: say("Tile sets are not supported by this port"); break;
        case 32805: saveDialog(); break;                                          // Save City
        case 32806: saveAs(); break;                                              // Save City As
        case 60001: { std::string name = info_.cityName; if (askText("City name", name) && !name.empty()) setCityNameAll(name); break; }   // Rename City (not in the original)
        case 57665: if (onPauseAction) onPauseAction(3); break;                  // Exit
        case 32768: clock_.speed = clock_.speed == GameSpeed::Paused ? GameSpeed::Llama : GameSpeed::Paused; break;
        case 32769: clock_.speed = GameSpeed::Turtle; break;
        case 32770: clock_.speed = GameSpeed::Llama; break;
        case 32771: clock_.speed = GameSpeed::Cheetah; break;
        case 32934: clock_.speed = GameSpeed::AfricanSwallow; break;
        case 32772: autoBudget_ = !autoBudget_; break;
        case 32773: autoGoto_ = !autoGoto_; break;
        case 32774: soundOn_ = !soundOn_; if (audio) audio->setEffectsEnabled(soundOn_); break;
        case 32775: musicOn_ = !musicOn_; if (audio) audio->setMusicEnabled(musicOn_); break;
        case 32776: { const int y = rnd(0x28) + cb - 0x14; const int x = rnd(0x28) + ca - 0x14; requestDisaster(1, x, y); break; }   // Fire
        case 32777: requestDisaster(2, ca, cb); break;                            // Flood
        case 32778: requestDisaster(0x12, sim_.context().things.focusA, sim_.context().things.focusB); break;   // Air Crash
        case 32779: { const int y = rnd(0x7E) + 1; const int x = rnd(0x7E) + 1; requestDisaster(7, x, y); break; }   // Tornado
        case 32780: { const int y = rnd(0x7E) + 1; const int x = rnd(0x7E) + 1; requestDisaster(6, x, y); break; }   // Earthquake
        case 32781: { const int y = rnd(0x20) + cb - 0xF; const int x = rnd(0x20) + ca - 0xF; requestDisaster(8, x, y); break; }   // Monster
        case 32919: requestDisaster(0x10, sim_.context().things.focusA, sim_.context().things.focusB); break;   // Hurricane
        case 32920: { const int y = rnd(0x20) + cb - 0xF; const int x = rnd(0x20) + ca - 0xF; requestDisaster(3, x, y); break; }   // Rioters
        case 32782: sim_.state().cb4ec = sim_.state().cb4ec == 0 ? 1 : 0; break;  // No Disasters
        case 32784: budget_.open = true; break;
        case 32785: budget_.open = true; budget_.openDetail(1); break;           // Ordinances
        case 32786: info_.open = InfoWindows::Which::City; break;                // Population
        case 32788: info_.open = InfoWindows::Which::Graphs; break;
        case 32787: info_.open = InfoWindows::Which::Industry; break;
        case 32789: info_.open = InfoWindows::Which::Neighbors; break;
        case 32783: info_.open = InfoWindows::Which::Map; break;
        case 32791: info_.subscription = !info_.subscription; break;
        case 32792: info_.extraEditions = !info_.extraEditions; break;
        case 57666: case 32893: case 57667: say("The help file is not available in this port"); break;
        case 32908: case 32909: case 32910: case 32911: case 32922: case 32923: case 32924: case 32925: case 32926: case 32927: case 32928: case 32931: debugCommand(id); break;
        case 57664: { Modal m; m.title = "SimCity 2000 (port) - an independent re-implementation; it needs your own copy of the game."; m.options = {"OK"}; runModal(m); break; }
        default: break;
    }
}

}  // namespace sc2k
