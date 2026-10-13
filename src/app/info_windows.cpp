#include "app/info_windows.h"

#include <algorithm>
#include <cstdio>

#include <cstring>

#include "app/ui_draw.h"
#include "resources/bmp.h"

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
    gameDir_ = gameDir;
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
    if (open != Which::Newspaper) articleOpen_ = false;
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
        case Which::Newspaper: drawNewspaper(r, x, y + 34); if (articleOpen_) drawArticle(r, x, y); break;
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
// The painter 0x4780E2 of the original: three page styles of 11 rectangles (tables at 0x4CC640 + 0xB0 * style, 640x400 page) with the font height
// (table 0x4E91D0) and alignment 0 left / 1 centre / 2 right (table 0x4E9188) of each. Rect roles: 0 masthead, 1 date, 2 motto, 3 picture, 4 lead headline,
// 5 opinion box, 6 weather box, 7..10 headlines of stories 1..4.
const Rc kRects[3][11] = {
    {{0, 6, 640, 36}, {5, 16, 218, 31}, {315, 16, 635, 31}, {243, 76, 456, 176}, {0, 36, 640, 76}, {0, 300, 213, 400}, {426, 300, 640, 400}, {0, 76, 213, 300}, {426, 76, 640, 300}, {213, 176, 320, 400}, {320, 176, 426, 400}},
    {{0, 37, 640, 73}, {421, 53, 635, 68}, {5, 53, 218, 68}, {178, 186, 434, 286}, {0, 0, 640, 37}, {0, 300, 128, 400}, {0, 73, 128, 300}, {128, 73, 640, 186}, {128, 287, 384, 400}, {384, 186, 512, 400}, {512, 186, 640, 400}},
    {{0, 0, 384, 30}, {0, 0, 635, 12}, {0, 12, 635, 24}, {128, 30, 256, 240}, {0, 30, 128, 400}, {256, 30, 384, 135}, {512, 24, 640, 339}, {384, 24, 512, 400}, {128, 240, 256, 400}, {256, 135, 384, 400}, {512, 339, 640, 400}}};
const int kAlign[33] = {1, 0, 2, 1, 1, 0, 0, 1, 1, 1, 1, 1, 2, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0};
const int kHeight[33] = {24, 12, 12, 0, 32, 12, 12, 10, 10, 10, 10, 24, 12, 12, 0, 30, 12, 12, 10, 10, 10, 10, 24, 10, 10, 1, 10, 0, 0, 10, 10, 10, 10};

// One-line text in `rc` (0x47B670): returns false when it does not fit; on success the rect's top moves down by 1.25 * height (0x47B5B0 callers).
struct Painter {
    SDL_Renderer* r;
    TextRenderer& font;
    CrtRand& rng;
    int ox, oy;
    bool line(Rc& rc, const std::string& t, int px, int align, bool draw = true) {
        if (px < 1) return false;
        const int w = font.width(t, px);
        if (w > rc.r - rc.l - 4 || rc.t + px * 5 / 4 > rc.b + px / 4) return false;
        if (draw) {
            const int x = align == 1 ? rc.l + (rc.r - rc.l - w) / 2 : align == 2 ? rc.r - 2 - w : rc.l + 2;
            font.draw(t, ox + x, oy + rc.t, px, 0, 0, 0);
        }
        rc.t += px * 5 / 4;
        return true;
    }
    // 0x47B5B0 callers: the height, then 4/5 and 2/3 of it
    bool fit3(Rc& rc, const std::string& t, int px, int align) {
        return line(rc, t, px, align) || line(rc, t, px * 4 / 5, align) || line(rc, t, px * 2 / 3, align);
    }
    // 0x47B670 for large text: shrinks 10/12, 2/3, 1/2 of the height, centring it vertically in the band
    bool fitBig(Rc rc, const std::string& t, int px, int align) {
        if (line(rc, t, px, align)) return true;
        for (int q : {px * 10 / 12, px * 2 / 3, px / 2}) {
            Rc c = rc;
            c.t += std::max(0, (rc.b - rc.t - q * 5 / 4) / 2);
            if (line(c, t, q, align)) return true;
        }
        return false;
    }
    // 0x47B160: the greeked columns that stand for the article text: rows of 3-pixel bars with a short last line per paragraph
    void greek(Rc rc) {
        if (rc.t >= rc.b) return;
        const int rows = (rc.b - rc.t) / 5;
        auto bar = [&](int l, int t, int rr) {   // GDI Rectangle() with the default pen and brush: a black outline around one white row
            if (rr - l < 2) return;
            fill(r, ox + l, oy + t, rr - l, 3, 0, 0, 0);
            fill(r, ox + l + 1, oy + t + 1, rr - l - 2, 1, 255, 255, 255);
        };
        int row = 0;
        while (row < rows) {
            bar(rc.l + 13, row * 5 + rc.t + 2, rc.r - 3);
            int more = rng.next() % 6;
            while (more-- > 0) { if (++row >= rows) break; bar(rc.l + 3, row * 5 + rc.t + 2, rc.r - 3); }
            if (row + 1 < rows) {
                const int shorten = rng.next() % std::max(1, (rc.r - rc.l) * 2 / 3);
                bar(rc.l + 3, (row + 1) * 5 + rc.t + 2, rc.r - shorten - 3);
            }
            row += 2;
        }
    }
    // 0x47B370: the headline of a column story (split in two lines and shrunk by 6/7 until it fits, 0x47B460), then the greeked story under it in
    // columns of at most 130 pixels
    void story(Rc rc, const std::string& t, int px, int align) {
        const Rc whole = rc;
        if (!line(rc, t, px, align)) {
            size_t cut = t.size() / 2, best = std::string::npos;
            for (size_t i = 0; i < t.size(); ++i) if (t[i] == ' ' && (best == std::string::npos || std::llabs(static_cast<long long>(i) - static_cast<long long>(t.size() / 2)) < std::llabs(static_cast<long long>(best) - static_cast<long long>(t.size() / 2)))) best = i;
            if (best != std::string::npos) cut = best;
            const std::string a = t.substr(0, cut), b = cut < t.size() ? t.substr(cut + (t[cut] == ' ' ? 1 : 0)) : std::string();
            for (; px >= 6; px = px * 6 / 7) {
                Rc probe = rc;
                if (!line(probe, a, px, align, false) || !line(probe, b, px, align, false)) continue;
                line(rc, a, px, align);
                line(rc, b, px, align);
                break;
            }
        }
        const int total = whole.r - whole.l, cols = total / 0x82 + 1;
        for (int k = 0; k < cols; ++k) greek(Rc{whole.l + k * total / cols, rc.t, whole.l + (k + 1) * total / cols, whole.b});
    }
};
}  // namespace

SDL_Texture* InfoWindows::picture(SDL_Renderer* r, int number) {
    if (auto it = pictures_.find(number); it != pictures_.end()) return it->second.get();
    SDL_Texture* tex = nullptr;
    try {
        const Bmp8 b = Bmp8::load(gameDir_ + "/BITMAPS/" + std::to_string(number) + ".BMP");
        std::vector<uint32_t> px(b.index.size());
        for (size_t i = 0; i < px.size(); ++i) px[i] = b.palette[b.index[i]];
        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, b.width, b.height);
        if (tex) SDL_UpdateTexture(tex, nullptr, px.data(), b.width * 4);
    } catch (const std::exception&) {}
    pictures_.emplace(number, std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>(tex, &SDL_DestroyTexture));
    return tex;
}

void InfoWindows::drawNewspaper(SDL_Renderer* r, int x0, int y0) {
    SimContext& c = sim_.context();
    CityState& s = c.s;
    if (!font_) font_ = std::make_unique<TextRenderer>(r);
    const std::array<uint8_t, 30>& pages = news_.pages();
    const int pg = page_ % 6;
    const int style = pages[static_cast<size_t>(pg) * 5 + 1] % 3;
    const Rc* R = kRects[style];
    const int* A = kAlign + style * 11;
    const int* H = kHeight + style * 11;
    const int topic = pages[static_cast<size_t>(pg) * 5 + 3], weatherTitle = pages[static_cast<size_t>(pg) * 5 + 4];
    const uint32_t seed = session_ + static_cast<uint32_t>(pg) * 500u + static_cast<uint32_t>(s.day / 25);
    CrtRand layoutRng(seed ^ 0x5eed);
    (void)seed;
    Painter P{r, *font_, layoutRng, x0, y0 + 4};
    const int ox = x0, oy = y0 + 4;
    fill(r, ox, oy, 640, 400, 255, 255, 255);   // the view's default (white) background
    // the two rules of the masthead band
    for (int y : {R[0].t - 2, R[0].b - 2}) if (y >= 0) fill(r, ox + R[0].l + 3, oy + y, R[0].r - R[0].l - 7, 1, 0, 0, 0);

    const int month = s.month(), years = s.day / 300, season = ((month + 1) % 12) / 3;
    static const char* months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    // masthead: "The " for the first half of the pages, then the paper's name
    const std::string name = res_.string(0x168 + pages[static_cast<size_t>(pg) * 5]);
    {
        Rc rc = R[0];
        P.fit3(rc, (pg < 3 ? res_.string(0x178) : std::string()) + name, H[0], A[0]);
    }
    // date line: weekday number, month and year (0x4780E2: string 0x177 + n + ", " + month + " " + year)
    {
        Rc rc = R[1];
        const int n = (month + s.startYear + years + season) % 7 + 1;
        P.fit3(rc, res_.string(0x177) + " " + std::to_string(n) + ", " + months[month] + " " + std::to_string(s.startYear + years), H[1], A[1]);
    }
    // motto / price: string 377 + 5 * paper + 50-year steps
    {
        Rc rc = R[2];
        const int step = std::min(4, (s.startYear - 1900) / 50 + years / 50);
        P.fit3(rc, res_.string(377 + pages[static_cast<size_t>(pg) * 5 + 2] % 3 * 5 + std::max(0, step)), H[2], A[2]);
    }
    auto headline = [&](int k) { return storyText(k, false); };
    // lead headline
    if (H[4] < 16) P.story(R[4], headline(4), H[4], A[4]);   // small lead type: a column story like the others
    else P.fitBig(R[4], headline(4), H[4], A[4]);
    // picture of the lead story (BITMAPS/400..411.BMP chosen by the story's message id, table 0x4E9418) or a greeked box
    if (H[3] < 1) {
        static const int16_t pictureOf[80] = {0, 0, 404, 404, 405, 402, 406, 0, 0, 0, 0, 404, 406, 408, 0, 0, 410, 401, 411, 0, 0, 0, 408, 0, 408, 409, 400, 406, 408, 408, 408, 411, 411, 409, 403, 411, 410, 404, 402, 403, 407, 0, 0, 0, 0, 411, 403, 410, 408, 403, 403, 403, 411, 411, 403, 403, 403, 403, 411, 410, 401, 411, 404, 407, 407, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int num = news_.entries()[0].id >= 0 && news_.entries()[0].id < 80 ? pictureOf[news_.entries()[0].id] : 0;
        if (num < 400 || num > 411) num = 400 + (seed % 12);
        if (SDL_Texture* t = picture(r, num)) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(t, &tw, &th);
            const SDL_FRect d{static_cast<float>(ox + R[3].l + (R[3].r - R[3].l - tw) / 2), static_cast<float>(oy + R[3].t), tw, th};
            SDL_RenderTexture(r, t, nullptr, &d);
        }
    } else {
        P.greek(R[3]);
    }
    // headlines of stories 1..4 with their greeked columns
    for (int k = 1; k <= 4; ++k) P.story(R[6 + k], headline(6 + k), H[6 + k], A[6 + k]);
    // opinion box (rect 5): title, a filler story, then the mayor's rating or the poll's four top complaints
    {
        Rc rc = R[5];
        if (H[5] > 0) {
            P.fit3(rc, res_.string(354 + std::min(5, topic)), H[5], A[5]);
            P.fit3(rc, headline(5), H[5], A[5]);
            auto fmt = [&](std::string t, int v) {
                const size_t p = t.find("%d");
                if (p != std::string::npos) t.replace(p, 2, std::to_string(v));
                for (size_t q; (q = t.find("%%")) != std::string::npos;) t.erase(q, 1);
                return t;
            };
            if (topic <= 2) {
                if (s.approval != 0) P.fit3(rc, fmt(res_.string(0x161), s.approval), H[5], A[5]);
            } else if (topic <= 4) {
                for (int i = 0; i < 4 && s.pollCount[static_cast<size_t>(i)] != 0; ++i)
                    P.fit3(rc, fmt(res_.string(0x16F + s.pollOrder[static_cast<size_t>(i)]), s.pollCount[static_cast<size_t>(i)]), H[5], A[5]);
            }
        }
        P.greek(rc);
    }
    // weather box (rect 6)
    {
        Rc rc = R[6];
        if (H[6] > 0) {
            P.fit3(rc, res_.string(347 + std::min(5, weatherTitle)), H[6], A[6]);
            P.fit3(rc, headline(6), H[6], A[6]);
            P.fit3(rc, std::to_string(s.c9df8 - 100) + "\xB0" "F " + std::to_string(s.wind) + "mph " + std::to_string(s.cloudiness) + "mm", H[6], A[6]);
        }
        P.greek(rc);
    }
}

// The text of the story shown in rect `k` of the page (0x4780E2 seeds the C rand with session + page * 500 + day / 25 + 7 * k before the headline,
// and 0x47A050 does the same before the headline and again before the article): k 4 = lead, 5 = filler opinion, 6 = weather, 7..10 = stories 1..4.
std::string InfoWindows::storyText(int k, bool article) {
    static const int entryOf[11] = {0, 0, 0, 0, 0, 8, 7, 1, 2, 3, 4};
    if (!newsData_.loaded) return "(DATA/DATA_USA.DAT not found)";
    NewsRoom::Context ctx;
    ctx.cityName = cityName.empty() ? std::string("the city") : cityName;
    ctx.label = [&](int idx) { const char* p = reinterpret_cast<const char*>(&sim_.world().xlab[static_cast<size_t>(idx & 0xFF) * 25]); return std::string(p, strnlen(p, 24)); };
    const uint32_t seed = session_ + static_cast<uint32_t>(page_ % 6) * 500u + static_cast<uint32_t>(sim_.context().s.day / 25) + static_cast<uint32_t>(k) * 7u;
    CrtRand rg(seed);
    const std::string head = news_.headline(entryOf[k], rg, ctx);
    if (!article) return head;
    CrtRand ra(seed);
    std::string text = head + " -\n" + news_.article(entryOf[k], ra, ctx);
    for (char& ch : text) {   // 0x47A5C0: the Mac-roman curly quotes become plain ones
        const unsigned char u = static_cast<unsigned char>(ch);
        if (u == 0xD4 || u == 0xD5) ch = '\'';
        else if (u == 0xD2 || u == 0xD3) ch = '"';
    }
    return text;
}

void InfoWindows::drawArticle(SDL_Renderer* r, int x, int y) {
    if (!font_) font_ = std::make_unique<TextRenderer>(r);
    const int bw = 460, bx = x + (kW - bw) / 2;
    // word-wrap in pixels; the type shrinks until the box fits the paper
    std::vector<std::string> lines;
    int px = 17;
    for (;; px -= 2) {
    lines.clear();
    for (size_t pos = 0; pos <= articleText_.size();) {
        size_t nl = articleText_.find('\n', pos);
        if (nl == std::string::npos) nl = articleText_.size();
        std::string para = articleText_.substr(pos, nl - pos), cur;
        for (size_t i = 0; i < para.size();) {
            size_t j = para.find(' ', i);
            if (j == std::string::npos) j = para.size();
            const std::string word = para.substr(i, j - i);
            if (!cur.empty() && font_->width(cur + " " + word, px) > bw - 40) { lines.push_back(cur); cur.clear(); }
            cur += (cur.empty() ? "" : " ") + word;
            i = j + 1;
        }
        lines.push_back(cur);
        pos = nl + 1;
    }
    if (px <= 11 || 70 + static_cast<int>(lines.size()) * (px + 4) <= kH - 50) break;
    }
    const int lh = px + 4, bh = 70 + static_cast<int>(lines.size()) * lh, by = y + 25 + std::max(0, (kH - 50 - bh) / 2);
    fill(r, bx, by, bw, bh, 244, 240, 226);
    frame(r, bx, by, bw, bh, 20, 18, 14);
    frame(r, bx + 3, by + 3, bw - 6, bh - 6, 20, 18, 14);
    int ty = by + 14;
    for (const std::string& l : lines) { font_->draw(l, bx + 20, ty, px, 20, 18, 14); ty += lh; }
    fill(r, bx + bw / 2 - 30, by + bh - 34, 60, 22, 214, 208, 188);
    frame(r, bx + bw / 2 - 30, by + bh - 34, 60, 22, 20, 18, 14);
    font_->draw("OK", bx + bw / 2 - font_->width("OK", 15) / 2, by + bh - 31, 15, 20, 18, 14);
}

bool InfoWindows::closeArticle() { const bool was = articleOpen_; articleOpen_ = false; return was; }


bool InfoWindows::mouseDown(int mx, int my, int sw, int sh) {
    if (open == Which::None) return false;
    if (articleOpen_) { articleOpen_ = false; return true; }   // the message box: any click is its OK (also outside the paper)
    const int x = ox(sw), y = oy(sh);
    if (mx < x || my < y || mx >= x + kW || my >= y + kH) return true;
    if (my < y + 26) {
        if (mx > x + kW - 70) open = Which::None;
        for (int i = 0; i < 4; ++i) if (mx >= x + 110 + i * 100 && mx < x + 202 + i * 100) open = static_cast<Which>(i + 1);
        return true;
    }
    if (open == Which::Newspaper && my >= y + 38) {   // clicking a story (rects 4..10 of the page) opens its text (0x477FF0 -> 0x47A050)
        const int style = news_.pages()[static_cast<size_t>(page_ % 6) * 5 + 1] % 3;
        for (int k = 4; k <= 10; ++k) {
            const Rc& q = kRects[style][k];
            if ((k == 5 || k == 6) && style == 2) continue;
            if (mx - x >= q.l && mx - x < q.r && my - (y + 38) >= q.t && my - (y + 38) < q.b) { articleText_ = storyText(k, true); articleOpen_ = true; break; }
        }
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
