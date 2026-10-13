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
    auto row = [&](const std::string& a, const std::string& b) { text(r, x + 24, y + 8 + line * 14, a, 190, 200, 220); text(r, x + 360, y + 8 + line * 14, b); ++line; };
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
    row("Demand R / C / I", std::to_string(s.demand[0]) + " / " + std::to_string(s.demand[1]) + " / " + std::to_string(s.demand[2]));
    row("National population", std::to_string(s.nationalPop));
    row("Federal rate level", std::to_string(s.rateLevel));
    row("City class (milestone)", std::to_string(s.milestone));
    // the population window (dialog 128): residents by age class, their health and their education (the job-market arrays of the economy model, 20 classes of 5 years)
    static const int tabStrings[3] = {415, 416, 417};
    const int cy = y + 8 + line * 14 + 6;
    for (int m = 0; m < 3; ++m) {
        fill(r, x + 24 + m * 110, cy, 100, 16, m == ageMode_ ? 90 : 50, m == ageMode_ ? 100 : 60, m == ageMode_ ? 150 : 80);
        text(r, x + 32 + m * 110, cy + 4, res_.string(tabStrings[m]));
    }
    text(r, x + 360, cy + 4, res_.string(419));   // "Resident Age"
    uint32_t value[20];
    const uint32_t pop = s.population10;
    for (size_t i = 0; i < 20; ++i) {
        const uint32_t n = s.jobsA[i];
        uint32_t v = 0;
        if (pop != 0) {
            if (ageMode_ == 0) { v = n * 600u / pop; if (n != 0 && v == 0) v = 1; }
            else if (ageMode_ == 1) v = n == 0 ? 0 : s.jobsC[i] / n;
            else v = n == 0 ? 0 : s.jobsB[i] * 15u / (n * 25u);
        }
        value[i] = v;
    }
    uint32_t top = 1;
    for (uint32_t v : value) top = std::max(top, v);
    const int by = cy + 22, bh = 80, bw = 26;
    fill(r, x + 24, by, 20 * bw + 8, bh + 16, 22, 26, 38);
    for (int i = 0; i < 20; ++i) {
        const int h = static_cast<int>(value[static_cast<size_t>(i)] * static_cast<uint32_t>(bh) / top);
        fill(r, x + 28 + i * bw, by + bh - h, bw - 3, h, ageMode_ == 0 ? 80 : (ageMode_ == 1 ? 80 : 200), ageMode_ == 0 ? 160 : (ageMode_ == 1 ? 200 : 180), ageMode_ == 0 ? 220 : (ageMode_ == 1 ? 120 : 60));
        if (i % 2 == 0) text(r, x + 28 + i * bw, by + bh + 4, std::to_string(i * 5), 150, 160, 180);
    }
    text(r, x + 20 * bw + 40, by + 6, "age (years)", 150, 160, 180);
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

namespace {
struct Rc { int l, t, r, b; };
// The three page layouts of the original painter (0x4780E2; rect tables at 0x4CC640 / 0x4CC6F0 / 0x4CC7A0 in the 640x400 page).
// Which rect plays which role is inferred from shapes and neighbours (STRONG INFERENCE).
struct Layout { Rc mast, date, motto, pic, head, article, poll, side[4]; };
const Layout kLayouts[3] = {
    {{0, 6, 640, 36}, {5, 16, 218, 31}, {315, 16, 635, 31}, {243, 76, 456, 176}, {0, 36, 640, 76}, {0, 76, 213, 300}, {0, 300, 213, 400},
     {{426, 76, 640, 300}, {213, 176, 320, 400}, {320, 176, 426, 400}, {426, 300, 640, 400}}},
    {{0, 0, 640, 37}, {5, 53, 218, 68}, {421, 53, 635, 68}, {178, 186, 434, 286}, {0, 37, 640, 53}, {128, 73, 640, 186}, {0, 300, 128, 400},
     {{0, 73, 128, 300}, {128, 287, 384, 400}, {384, 186, 512, 400}, {512, 186, 640, 400}}},
    {{0, 0, 384, 30}, {0, 0, 635, 12}, {0, 12, 635, 24}, {256, 30, 384, 135}, {128, 30, 256, 240}, {256, 135, 384, 400}, {0, 30, 128, 400},
     {{128, 240, 256, 400}, {384, 24, 512, 400}, {512, 24, 640, 339}, {512, 339, 640, 400}}},
};
// Draws wrapped text in a rect with the 8x8 font scaled to `scale`, shrinking (x4/5, x2/3 like the original's fit retries) until it fits.
void fitText(SDL_Renderer* r, const Rc& rc, const std::string& t, float scale, Uint8 cr, Uint8 cg, Uint8 cb, bool centre = false) {
    const int w = rc.r - rc.l - 4, h = rc.b - rc.t - 2;
    std::vector<std::string> lines;
    float sc = scale;
    for (;;) {
        const int cols = std::max(1, static_cast<int>(w / (8 * sc)));
        lines = wrap(t, static_cast<size_t>(cols));
        size_t longest = 0;
        for (const std::string& ln : lines) longest = std::max(longest, ln.size());
        if ((static_cast<int>(lines.size()) * (8 * sc + 2) <= h && longest <= static_cast<size_t>(cols)) || sc <= 1.0f) break;
        sc = std::max(1.0f, sc * (sc > 1.6f ? 0.8f : 0.67f));
    }
    SDL_SetRenderScale(r, sc, sc);
    const SDL_Rect clip{static_cast<int>(rc.l / sc), static_cast<int>(rc.t / sc), static_cast<int>((rc.r - rc.l) / sc), static_cast<int>((rc.b - rc.t) / sc)};
    SDL_SetRenderClipRect(r, &clip);
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    float y = rc.t + 1;
    for (const std::string& ln : lines) {
        if (y + 8 * sc > rc.b) break;
        const float tw = ln.size() * 8 * sc;
        const float x = rc.l + 2 + (centre ? (w - tw) / 2 : 0);
        SDL_RenderDebugText(r, x / sc, y / sc, ln.c_str());
        y += 8 * sc + 2;
    }
    SDL_SetRenderScale(r, 1, 1);
    SDL_SetRenderClipRect(r, nullptr);
}
}  // namespace

void InfoWindows::drawNewspaper(SDL_Renderer* r, int x0, int y0) {
    SimContext& c = sim_.context();
    CityState& s = c.s;
    const std::array<uint8_t, 30>& pages = news_.pages();
    const int pg = page_ % 6;
    const Layout& L = kLayouts[pages[static_cast<size_t>(pg) * 5 + 1] % 3];
    const int ox = x0, oy = y0 + 4;
    auto R = [&](const Rc& q) { return Rc{q.l + ox, q.t + oy, q.r + ox, q.b + oy}; };
    fill(r, ox, oy, 640, 400, 236, 232, 214);
    const std::string name = res_.string(0x168 + pages[static_cast<size_t>(pg) * 5]);
    static const char* months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    fitText(r, R(L.mast), "THE " + name, 3.0f, 20, 16, 10, true);
    fitText(r, R(L.date), std::string(months[s.month()]) + " " + std::to_string(s.startYear + s.day / 300), 1.0f, 60, 50, 30);
    fitText(r, R(L.motto), res_.string(377 + (pages[static_cast<size_t>(pg) * 5 + 2] % 4)), 1.0f, 60, 50, 30, true);
    for (const Rc* q : {&L.head, &L.article, &L.poll, &L.pic}) { Rc b = R(*q); frame(r, b.l + 1, b.t, b.r - b.l - 2, b.b - b.t, 190, 184, 160); }
    NewsRoom::Context ctx;
    ctx.cityName = cityName.empty() ? std::string("the city") : cityName;
    ctx.label = [&](int idx) { const char* p = reinterpret_cast<const char*>(&sim_.world().xlab[static_cast<size_t>(idx & 0xFF) * 25]); return std::string(p, strnlen(p, 24)); };
    const uint32_t seed = session_ + static_cast<uint32_t>(pg) * 500u + static_cast<uint32_t>(s.day / 25);
    if (newsData_.loaded) {
        CrtRand rg(seed + 0x1c);
        fitText(r, R(L.head), news_.headline(0, rg, ctx), 3.0f, 10, 8, 4, true);
        CrtRand rb(seed + 0x1c);
        news_.headline(0, rb, ctx);   // same rule choice and word cache as the headline
        CrtRand rd(seed + 0x1c);
        fitText(r, R(L.article), news_.article(0, rd, ctx), 1.0f, 40, 30, 20);
        for (int k = 1; k <= 4; ++k) {
            CrtRand rs(static_cast<uint32_t>(k + 6) * 7u + seed);
            const std::string h = news_.headline(k, rs, ctx);
            fitText(r, R(L.side[k - 1]), h, 2.0f, 30, 24, 16);
        }
    } else {
        fitText(r, R(L.article), "(DATA/DATA_USA.DAT not found: no stories)", 1.0f, 120, 60, 40);
    }
    // the picture slot: a framed placeholder (the original draws an art strip chosen by the story)
    { Rc b = R(L.pic); fill(r, b.l + 2, b.t + 2, b.r - b.l - 4, b.b - b.t - 4, 200, 196, 176); }
    // the opinion poll of the day (0x4729F0): the four most common complaints and the mayor's approval
    std::string poll = res_.string(357) + " ";
    for (int i = 0; i < 4 && s.pollCount[static_cast<size_t>(i)] != 0; ++i) {
        std::string t = res_.string(0x16F + s.pollOrder[static_cast<size_t>(i)]);
        const size_t p = t.find("%d");
        if (p != std::string::npos) t.replace(p, 2, std::to_string(s.pollCount[static_cast<size_t>(i)]));
        for (size_t q; (q = t.find("%%")) != std::string::npos;) t.erase(q, 1);
        poll += t + " ";
    }
    if (s.approval != 0) {
        std::string mayor = res_.string(353);
        const size_t p = mayor.find("%d");
        if (p != std::string::npos) mayor.replace(p, 2, std::to_string(s.approval));
        for (size_t q; (q = mayor.find("%%")) != std::string::npos;) mayor.erase(q, 1);
        poll += mayor;
    }
    fitText(r, R(L.poll), poll, 1.0f, 40, 30, 20);
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
    if (open == Which::City) {
        for (int m = 0; m < 3; ++m) if (mx >= x + 24 + m * 110 && mx < x + 124 + m * 110 && my >= y + 8 + 16 * 14 + 6 && my < y + 8 + 16 * 14 + 22) ageMode_ = m;
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
