#include "app/info_windows.h"

#include <algorithm>
#include <cstdio>

#include <cstring>

#include "app/ui_draw.h"

namespace sc2k {
using namespace ui;

namespace {
constexpr uint32_t kSeriesColor[16] = {0xFFFFFF, 0x80FF80, 0x80C0FF, 0xFFE060, 0xFF8080, 0xA0A0A0, 0xC080FF, 0xFF4040, 0xFFC000, 0x40C0FF, 0x60FF60, 0xFFA0FF, 0xFF8040, 0xB0B040, 0x40E0B0, 0xE0E0E0};
const char* kAdvisorNames[8] = {"Economist", "City Council", "Treasurer", "Police Chief", "Fire Chief", "Health Minister", "Education Chief", "Transportation"};   // port labels (the exe has the portraits, not the titles)
}  // namespace

InfoWindows::InfoWindows(Simulation& sim, const PeResources& res) : sim_(sim), res_(res) {
    // series names: the caption buttons 264..279 of the original's graph dialog (152)
    seriesNames_.assign(16, "");
    if (const auto* d = res.dialog(152))
        for (const auto& c : d->controls)
            if (c.id >= 264 && c.id <= 279) seriesNames_[static_cast<size_t>(c.id - 264)] = c.text;
    static const char* fallback[16] = {"City Size", "Residents", "Commerce", "Industry", "Traffic", "Pollution", "Value", "Crime", "Power %", "Water %", "Health", "Education", "Unemployment", "GNP", "Nat'n Pop.", "Fed Rate"};
    for (size_t i = 0; i < 16; ++i) if (seriesNames_[i].empty()) seriesNames_[i] = fallback[i];
}

// ---- news queue ----
void InfoWindows::initNews(const std::string& gameDir) {
    newsData_ = NewsData::load(gameDir, sim_.tables().newsDigraph);
    news_.setData(&newsData_);
    session_ = static_cast<uint32_t>(SDL_GetTicks()) & 0xFFFF;   // the original seeds the page layout with GetTickCount (DAT_004E9140)
    CrtRand r(session_);
    news_.reset(r);
}

void InfoWindows::postMessage(int id, int arg) {
    news_.post(id, arg);
    if (id == 3 || id == 4 || id == 5 || id == 0x24) popup_ = true;   // 0x47BA90 opens the news dialog for these
}

// ---- drawing ----
void InfoWindows::draw(SDL_Renderer* r, int sw, int sh) {
    if (open == Which::None) { last_ = Which::None; return; }
    if (!portraitsTried_) {
        portraitsTried_ = true;
        try {
            for (int id = 197; id <= 204; ++id) portraits_.push_back(std::make_unique<IconStrip>(r, Bmp8::fromDib(res_.bitmap(id)), 64, false));
        } catch (const std::exception&) { portraits_.clear(); }
    }
    if (open == Which::Newspaper && last_ != Which::Newspaper) {   // opening the paper runs the opinion poll (it draws from the shared rand like the original)
        runOpinionPoll(sim_.context());
        ++newsOpened_;
        page_ = newsOpened_ - 1;
    }
    last_ = open;
    const int x = ox(sw), y = oy(sh);
    fill(r, 0, 0, sw, sh, 0, 0, 0, 100);
    fill(r, x, y, kW, kH, 30, 34, 46);
    frame(r, x, y, kW, kH, 200, 200, 220);
    const char* title = open == Which::Graphs ? "Graphs" : open == Which::City ? "City" : open == Which::Advisors ? "Advisors" : "Newspaper";
    text(r, x + 12, y + 10, title, 255, 255, 190);
    text(r, x + kW - 60, y + 10, "[Close]", 255, 255, 190);
    const char* tabs[4] = {"Graphs", "City", "Advisors", "Newspaper"};
    for (int i = 0; i < 4; ++i) {
        const bool on = static_cast<int>(open) == i + 1;
        fill(r, x + 110 + i * 100, y + 6, 92, 18, on ? 70 : 40, on ? 90 : 46, on ? 120 : 56);
        text(r, x + 118 + i * 100, y + 11, tabs[i]);
    }
    switch (open) {
        case Which::Graphs: drawGraphs(r, x, y + 34); break;
        case Which::City: drawCity(r, x, y + 34); break;
        case Which::Advisors: drawAdvisors(r, x, y + 34); break;
        case Which::Newspaper: drawNewspaper(r, x, y + 34); break;
        default: break;
    }
}

void InfoWindows::drawGraphs(SDL_Renderer* r, int x, int y) {
    const CityState& s = sim_.state();
    const int gx = x + 16, gy = y + 8, gw = 600, gh = 260;
    fill(r, gx, gy, gw, gh, 12, 14, 20);
    frame(r, gx, gy, gw, gh, 90, 90, 110);
    const int first = period_ == 0 ? 0 : period_ == 1 ? 0xC : 0x20, count = period_ == 0 ? 12 : 20;
    for (int k = 0; k < 16; ++k) {
        if (!((shown_ >> k) & 1u)) continue;
        // series 0..3 share a scale, 4..7 share one, the others have their own (the history keeps the running maxima)
        const int32_t mx = std::max<int32_t>(1, s.graphMax[static_cast<size_t>(k)]);
        const uint32_t c = kSeriesColor[k];
        int px = 0, py = 0;
        for (int i = 0; i < count; ++i) {
            const int32_t v = s.graph[static_cast<size_t>(k) * 52 + static_cast<size_t>(first + count - 1 - i)];   // oldest on the left
            const int xx = gx + 6 + i * (gw - 12) / std::max(1, count - 1);
            const int yy = gy + gh - 6 - static_cast<int>(static_cast<long long>(std::max<int32_t>(0, v)) * (gh - 12) / mx);
            if (i) { SDL_SetRenderDrawColor(r, Uint8(c >> 16), Uint8(c >> 8), Uint8(c), 255); SDL_RenderLine(r, float(px), float(py), float(xx), float(yy)); }
            px = xx; py = yy;
        }
    }
    const char* periods[3] = {"1 Year", "10 Years", "100 Yrs"};
    for (int i = 0; i < 3; ++i) {
        fill(r, gx + 8 + i * 80, gy + gh + 8, 72, 16, i == period_ ? 70 : 40, i == period_ ? 100 : 46, i == period_ ? 70 : 56);
        text(r, gx + 14 + i * 80, gy + gh + 12, periods[i]);
    }
    for (int k = 0; k < 16; ++k) {
        const int cx = gx + (k / 4) * 150, cy = gy + gh + 36 + (k % 4) * 14;
        const bool on = (shown_ >> k) & 1u;
        const uint32_t c = kSeriesColor[k];
        fill(r, cx, cy, 10, 10, Uint8(c >> 16), Uint8(c >> 8), Uint8(c));
        if (!on) fill(r, cx + 2, cy + 2, 6, 6, 12, 14, 20);
        text(r, cx + 16, cy + 1, seriesNames_[static_cast<size_t>(k)]);
    }
}

void InfoWindows::drawCity(SDL_Renderer* r, int x, int y) {
    const CityState& s = sim_.state();
    const int month = s.month();
    static const char* months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    int line = 0;
    auto row = [&](const std::string& a, const std::string& b) { text(r, x + 24, y + 14 + line * 18, a, 190, 200, 220); text(r, x + 260, y + 14 + line * 18, b); ++line; };
    row("Date", std::string(months[month]) + " " + std::to_string(s.startYear + s.day / 300));
    row("Population", std::to_string(s.population10));
    row("Residential population", std::to_string(s.catPop[0] * 10));
    row("Commercial workers", std::to_string(s.catPop[1] * 10));
    row("Industrial workers", std::to_string(s.catPop[2] * 10));
    row("Funds", money(s.treasury));
    row("Bonds outstanding", std::to_string(s.debt));
    row("Power supply used", std::to_string(s.powerPct) + " %");
    row("Water supply used", std::to_string(s.waterPct) + " %");
    row("Unemployment", std::to_string(s.c9610) + " %");
    row("Education quotient", std::to_string(s.cb4cc));
    row("Life expectancy", std::to_string(s.ca428));
    row("Demand: residential / commercial / industrial", std::to_string(s.demand[0]) + " / " + std::to_string(s.demand[1]) + " / " + std::to_string(s.demand[2]));
    row("National population", std::to_string(s.nationalPop));
    row("Federal rate level", std::to_string(s.rateLevel));
    row("City class (milestone)", std::to_string(s.milestone));
}

void InfoWindows::drawAdvisors(SDL_Renderer* r, int x, int y) {
    text(r, x + 20, y + 6, "Your advisors (advice selection of the original, FUN_00419E10)", 170, 180, 200);
    SimContext& c = sim_.context();
    const uint32_t keep = c.rng.state();   // the health advisor draws a rand(); the window must not disturb the simulation
    for (int adv = 0; adv < kAdvisorCount; ++adv) {
        const int by = y + 22 + adv * 46;
        fill(r, x + 14, by, kW - 28, 43, 40, 46, 62);
        if (static_cast<size_t>(adv) < portraits_.size()) portraits_[static_cast<size_t>(adv)]->drawSized(r, 0, x + 20, by + 1, 38, 41);
        text(r, x + 72, by + 6, kAdvisorNames[adv], 255, 255, 190);
        const std::string t = res_.string(adviceString(advisorAdvice(c, adv)));
        if (t.size() > 66) { text(r, x + 72, by + 20, t.substr(0, 66)); text(r, x + 72, by + 32, t.substr(66, 66)); }
        else text(r, x + 72, by + 24, t);
    }
    c.rng.seed(keep);
}

namespace {
std::vector<std::string> wrap(const std::string& s, size_t width) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\n') { lines.push_back(cur); cur.clear(); ++i; while (i < s.size() && s[i] == ' ') ++i; continue; }
        size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\n') ++j;
        const std::string word = s.substr(i, j - i);
        if (!cur.empty() && cur.size() + 1 + word.size() > width) { lines.push_back(cur); cur.clear(); }
        if (!cur.empty()) cur += ' ';
        cur += word;
        i = j;
        while (i < s.size() && s[i] == ' ') ++i;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}
}  // namespace

void InfoWindows::drawNewspaper(SDL_Renderer* r, int x, int y) {
    SimContext& c = sim_.context();
    CityState& s = c.s;
    const std::array<uint8_t, 30>& pages = news_.pages();
    const int pg = page_ % 6;
    const std::string name = res_.string(0x168 + pages[static_cast<size_t>(pg) * 5]);
    fill(r, x + 20, y + 4, kW - 40, 420, 236, 232, 214);
    frame(r, x + 20, y + 4, kW - 40, 420, 70, 60, 40);
    text(r, x + kW / 2 - static_cast<int>(name.size() + 4) * 12 / 2, y + 12, "THE " + name, 20, 16, 10);
    static const char* months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    text(r, x + 36, y + 30, std::string(months[s.month()]) + " " + std::to_string(s.startYear + s.day / 300) + "    " + res_.string(377 + (pages[static_cast<size_t>(pg) * 5 + 2] % 4)), 60, 50, 30);
    NewsRoom::Context ctx;
    ctx.cityName = sim_.state().scenario ? std::string("the city") : std::string("the city");
    ctx.label = [&](int idx) { const char* p = reinterpret_cast<const char*>(&sim_.world().xlab[static_cast<size_t>(idx & 0xFF) * 25]); return std::string(p, strnlen(p, 24)); };
    // the city's name: the first label of the file is not it; CNAM is shown by the file loader, the port keeps "the city" unless the game gives one
    if (!cityName.empty()) ctx.cityName = cityName;
    const uint32_t seed = session_ + static_cast<uint32_t>(pg) * 500u + static_cast<uint32_t>(s.day / 25);
    if (newsData_.loaded) {
        CrtRand rg(seed + 0x1c);
        const std::string head = news_.headline(0, rg, ctx);
        text(r, x + 36, y + 52, head, 10, 8, 4);
        CrtRand ra(seed + 0x1c);
        news_.headline(0, ra, ctx);   // same rule choice and word cache as the headline
        CrtRand rb(seed + 0x1c);
        const std::string body = news_.article(0, rb, ctx);
        int ty = y + 76;
        for (const std::string& line : wrap(body, 56)) { if (ty > y + 190) break; text(r, x + 36, ty, line, 40, 30, 20); ty += 12; }
        for (int k = 1; k <= 4; ++k) {
            CrtRand rs(static_cast<uint32_t>(k + 6) * 7u + seed);
            const std::string h = news_.headline(k, rs, ctx);
            const int col = (k - 1) % 2, row = (k - 1) / 2;
            text(r, x + 36 + col * 290, y + 200 + row * 14, "* " + h.substr(0, 40), 30, 24, 16);
        }
    } else {
        text(r, x + 36, y + 60, "(DATA/DATA_USA.DAT not found: no stories)", 120, 60, 40);
    }
    // the opinion poll of the day (0x4729F0): the four most common complaints and the mayor's approval
    text(r, x + 36, y + 240, res_.string(357), 20, 16, 10);   // "Opinion Poll"
    int line = 0;
    for (int i = 0; i < 4 && s.pollCount[static_cast<size_t>(i)] != 0; ++i, ++line) {
        std::string t = res_.string(0x16F + s.pollOrder[static_cast<size_t>(i)]);
        const size_t p = t.find("%d");
        if (p != std::string::npos) t.replace(p, 2, std::to_string(s.pollCount[static_cast<size_t>(i)]));
        for (size_t q; (q = t.find("%%")) != std::string::npos;) t.erase(q, 1);
        text(r, x + 36, y + 260 + line * 16, t, 40, 30, 20);
    }
    if (s.approval != 0) {
        std::string mayor = res_.string(353);
        const size_t p = mayor.find("%d");
        if (p != std::string::npos) mayor.replace(p, 2, std::to_string(s.approval));
        for (size_t q; (q = mayor.find("%%")) != std::string::npos;) mayor.erase(q, 1);
        text(r, x + 36, y + 330, mayor, 20, 16, 10);
    }
    text(r, x + 36, y + 360, "City population: " + std::to_string(s.population10) + "    Funds: " + money(s.treasury), 20, 16, 10);
}

bool InfoWindows::mouseDown(int mx, int my, int sw, int sh) {
    if (open == Which::None) return false;
    const int x = ox(sw), y = oy(sh);
    if (mx < x || my < y || mx >= x + kW || my >= y + kH) return true;
    if (my < y + 26) {
        if (mx > x + kW - 70) open = Which::None;
        for (int i = 0; i < 4; ++i) if (mx >= x + 110 + i * 100 && mx < x + 202 + i * 100) open = static_cast<Which>(i + 1);
        return true;
    }
    if (open == Which::Graphs) {
        const int gx = x + 16, gy = y + 34 + 8, gh = 260;
        for (int i = 0; i < 3; ++i) if (mx >= gx + 8 + i * 80 && mx < gx + 80 + i * 80 && my >= gy + gh + 8 && my < gy + gh + 24) period_ = i;
        for (int k = 0; k < 16; ++k) {
            const int cx = gx + (k / 4) * 150, cy = gy + gh + 36 + (k % 4) * 14;
            if (mx >= cx && mx < cx + 130 && my >= cy && my < cy + 12) shown_ ^= 1u << k;
        }
    }
    return true;
}

}  // namespace sc2k
