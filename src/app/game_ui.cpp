#include "app/game_ui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "save/error.h"
#include "simulation/bonds.h"

namespace sc2k {
namespace {
constexpr int kTopBar = 28, kGroupW = 150, kGroupH = 26, kItemW = 290, kItemH = 17, kBottomBar = 22;
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

GameUi::Rect GameUi::groupRect(int i) const { return {0, kTopBar + i * kGroupH, kGroupW, kGroupH}; }
GameUi::Rect GameUi::itemRect(int i) const { return {kGroupW + 2, kTopBar + std::max(0, group_) * kGroupH + i * kItemH, kItemW, kItemH}; }

bool GameUi::overUi(int x, int y) const {
    if (modal_ || budget_.open || info_.open != InfoWindows::Which::None) return true;
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
    if (modal_) {
        const int n = static_cast<int>(modal_->options.size());
        const int bx = (width_ - 520) / 2, by = (height_ - (70 + 22 * n)) / 2;
        for (int i = 0; i < n; ++i)
            if (Rect{bx + 20, by + 50 + 22 * i, 480, 20}.contains(x, y)) modal_->result = i;
        return;
    }
    if (budget_.mouseDown(x, y, button, width_, height_)) return;
    if (queryOpen_ && button == 1) queryOpen_ = false;
    if (button != 1) { scrolling_ = true; return; }
    if (info_.mouseDown(x, y, width_, height_)) return;
    if (y < kTopBar && x >= width_ - kTopButtonCount * kTopButtonStep - 4) {
        const int k = (x - (width_ - kTopButtonCount * kTopButtonStep - 4)) / kTopButtonStep;
        if (k == 0 || k == 1) { cam_.rotation = (cam_.rotation + (k == 0 ? 3 : 1)) & 3; cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, width_ / 2, height_ / 2); }
        else if (k == 2) cam_.zoom = std::max(0, cam_.zoom - 1);
        else if (k == 3) cam_.zoom = std::min(2, cam_.zoom + 1);
        else if (k == 4) cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, width_ / 2, height_ / 2);
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
        }
        return;
    }
    menuOpen_ = false;
    const ToolItem* t = tool();
    int a, b;
    if (!renderer_.pickTile(sim_.world(), cam_, x, y, a, b)) return;
    if (!t) { queryTile(a, b); return; }
    switch (t->kind) {
        case ToolKind::Bulldoze: dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; bulldozeAt(a, b); break;
        case ToolKind::TerrainEdit:
            dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b;
            editStartAlt_ = editLastAlt_ = sim_.world().altm.at(a, b) & 0x1F; editStartY_ = y; editPrevA_ = editPrevB_ = -1; sprayMs_ = 0;
            editSample(a, b, y);
            break;
        case ToolKind::Landscape: dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; landscapeAt(a, b); break;
        case ToolKind::Dispatch: placeDispatch(sim_.context(), ts_, t->sub, a, b) ? say("Dispatched") : say("Can't dispatch there"); break;
        case ToolKind::Terrain: dragging_ = true; startA_ = lastA_ = a; startB_ = lastB_ = b; levelTarget_ = sim_.world().altm.at(a, b) & 0x1F; terrainAt(a, b); break;
        case ToolKind::Zone:
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
    if (button != 1) { scrolling_ = false; return; }
    if (dragging_) { mouseMove(x, y); finishDrag(); }
    dragging_ = false;
}

void GameUi::mouseMove(int x, int y) {
    const int dx = x - mouseX_, dy = y - mouseY_;
    mouseX_ = x; mouseY_ = y;
    if (scrolling_) { cam_.scrollX += dx; cam_.scrollY += dy; return; }
    int a, b;
    if (overUi(x, y) || !renderer_.pickTile(sim_.world(), cam_, x, y, a, b)) { hoverA_ = hoverB_ = -1; return; }
    hoverA_ = a; hoverB_ = b;
    if (dragging_ && tool() && tool()->kind == ToolKind::TerrainEdit) {
        if (tool()->sub >= 3) editSample(a, b, y);
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
    if (dy > 0) cam_.zoom = std::min(2, cam_.zoom + 1);
    else if (dy < 0) cam_.zoom = std::max(0, cam_.zoom - 1);
}

bool GameUi::keyDown(SDL_Keycode k, bool ctrl) {
    if (modal_) {
        if (k == SDLK_ESCAPE) modal_->result = -1;
        else if (k == SDLK_RETURN) modal_->result = 0;
        else if (k == SDLK_BACKSPACE && modal_->textInput && !modal_->text.empty()) modal_->text.pop_back();
        return true;
    }
    if (queryOpen_ && k == SDLK_ESCAPE) { queryOpen_ = false; return true; }
    if (ctrl && k == SDLK_N) { newCityDialog(); return true; }
    if (ctrl && k == SDLK_S) { say(saveCity("sc2k-city.sc2") ? "Saved sc2k-city.sc2" : "Could not save"); return true; }
    if (budget_.open) { if (k == SDLK_ESCAPE || k == SDLK_B) budget_.open = false; return true; }
    if (info_.open != InfoWindows::Which::None) { if (k == SDLK_ESCAPE) info_.open = InfoWindows::Which::None; return true; }
    if (k == SDLK_G) { info_.open = InfoWindows::Which::Graphs; return true; }
    if (k == SDLK_B) { budget_.open = true; return true; }
    if (k == SDLK_ESCAPE) {
        if (menuOpen_) menuOpen_ = false;
        else if (group_ >= 0) { group_ = item_ = -1; dragging_ = false; }
        else return false;
    } else if (k == SDLK_SPACE) clock_.speed = clock_.speed == GameSpeed::Paused ? GameSpeed::Llama : GameSpeed::Paused;
    else if (k >= SDLK_1 && k <= SDLK_5) clock_.speed = static_cast<GameSpeed>(k - SDLK_1 + 1);
    else if (k == SDLK_EQUALS || k == SDLK_KP_PLUS) cam_.zoom = std::min(2, cam_.zoom + 1);
    else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) cam_.zoom = std::max(0, cam_.zoom - 1);
    else if (k == SDLK_R) { cam_.rotation = (cam_.rotation + 1) & 3; cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, width_ / 2, height_ / 2); }
    else if (k == SDLK_U) layers.underground = !layers.underground;
    else if (k == SDLK_P) layers.powerOverlay = !layers.powerOverlay;
    else if (k == SDLK_O) layers.waterOverlay = !layers.waterOverlay;
    return true;
}

void GameUi::newCity(int era, int difficulty) {
    if (defaultMapPath.empty()) { say("No default map available"); return; }
    CityFile map = CityFile::load(defaultMapPath);
    sim_.startNewCity(map, era, difficulty);
    base_ = map;
    avail_ = ToolAvailability::fromState(sim_.state(), sim_.tables());
    cam_.rotation = 0;
    cam_.centerOn(sim_.state().centerA2 / 2, sim_.state().centerB2 / 2, width_ / 2, height_ / 2);
    group_ = item_ = -1; dragging_ = false; queryOpen_ = false;
    say("New city started");
}

void GameUi::newCityDialog() {
    Modal year; year.title = "Starting year"; year.options = {"1900", "1950", "2000", "2050"};
    const int era = runModal(year);
    if (era < 0) return;
    Modal diff; diff.title = "Difficulty"; diff.options = {"Easy ($20,000)", "Medium ($10,000)", "Hard ($10K bond)"};
    const int d = runModal(diff);
    if (d < 0) return;
    newCity(era, d);
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
    if (t->sub == 2) plantForest(c, ts_, a, b);
    else if (t->sub == 3) placeStream(c, ts_, a, b);
    else if (!landscapeTool(c, ts_, t->sub, a, b) && message_.empty()) say(sim_.state().treasury < unitCost(*t) ? "Not enough funds" : "Can't do that here");
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
    SimContext& c = sim_.context();
    const int funds = sim_.state().treasury;
    bool ok = false;
    if (t->kind == ToolKind::TerrainEdit) {
        if (t->sub == 4) terrainLevelEnd(c, editPrevA_, editPrevB_);
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
        return;
    }
    if (t->kind != ToolKind::Building) return;
    const int cost = unitCost(*t);
    if (sim_.state().treasury < cost) { say("Not enough funds (" + moneyText(cost) + ")"); return; }
    message_.clear();
    if (placeBuilding(sim_.context(), ts_, t->cat, t->sub, a, b)) say(std::string(t->name) + " built for " + moneyText(cost));
    else if (message_.empty()) say(std::string("Can't build the ") + t->name + " here");
}

void GameUi::queryTile(int a, int b) {
    query_ = ::sc2k::queryTile(sim_.context(), a, b);
    queryOpen_ = true;
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

bool GameUi::saveCity(const std::string& path) {
    try {
        const std::vector<uint8_t> bytes = sim_.toCity(base_).serialize();
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
        const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        std::fclose(f);
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
    return avail_.allows(t.cat, t.sub);
}

void GameUi::scheduledEvent(int index) {
    avail_.applyEvent(index, sim_.state());
    say(index <= 11 ? "A new tool has been invented!" : "A new arcology design is available!");
}

void GameUi::postMessage(int id, int arg) { info_.postMessage(id, arg); }
void GameUi::showDialog(int id) { toolMessage(id); }

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
    drawSigns(r);
    const CityState& s = sim_.state();
    // top bar
    fill(r, 0, 0, width_, kTopBar, 20, 24, 32);
    char top[200];
    std::snprintf(top, sizeof top, "%s %d   Pop %u   Funds %s   R%d C%d I%d   [%s]", kMonths[s.month()], s.startYear + s.day / 300, s.population10, moneyText(s.treasury).c_str(),
                  s.demand[0], s.demand[1], s.demand[2], kSpeedNames[static_cast<int>(clock_.speed)]);
    text(r, 6, 10, top);
    std::string right = message_;
    if (dragging_ && previewCost_ > 0) right = "Cost " + moneyText(previewCost_);
    const int buttonsX = width_ - kTopButtonCount * kTopButtonStep - 4;
    for (int k = 0; k < kTopButtonCount; ++k) drawIcon(r, 1, kTopButtons[k], buttonsX + k * kTopButtonStep + 2, 2);
    if (!right.empty()) {
        const size_t room = static_cast<size_t>(std::max(0, (buttonsX - 500 - 8) / 8));
        if (right.size() > room) right.resize(room);
        text(r, std::max(500, buttonsX - static_cast<int>(right.size()) * 8 - 8), 10, right, 255, 220, 120);
    }

    // tool groups
    const auto& cat = toolCatalog();
    for (size_t g = 0; g < cat.size(); ++g) {
        const Rect rc = groupRect(static_cast<int>(g));
        const bool sel = static_cast<int>(g) == group_;
        fill(r, rc.x, rc.y, rc.w, rc.h, sel ? 60 : 34, sel ? 70 : 38, sel ? 100 : 48);
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
    text(r, 270, height_ - kBottomBar + 7, "Space pause  1-5 speed  +/- wheel zoom  R rotate  U/P/O views  right-drag scroll  B budget  G graphs  Ctrl+S save  Esc", 150, 160, 180);
    budget_.draw(r, width_, height_);
    info_.draw(r, width_, height_);
    if (queryOpen_) drawQuery(r);
    if (modal_) drawModal(r);
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

}  // namespace sc2k
