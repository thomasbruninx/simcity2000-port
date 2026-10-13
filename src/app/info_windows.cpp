#include "app/info_windows.h"

#include <algorithm>
#include <cstdio>

#include "app/ui_draw.h"

namespace sc2k {
using namespace ui;

namespace {
constexpr uint32_t kSeriesColor[16] = {0xFFFFFF, 0x80FF80, 0x80C0FF, 0xFFE060, 0xFF8080, 0xA0A0A0, 0xC080FF, 0xFF4040, 0xFFC000, 0x40C0FF, 0x60FF60, 0xFFA0FF, 0xFF8040, 0xB0B040, 0x40E0B0, 0xE0E0E0};
const char* kAdvisorNames[8] = {"Finance", "Police", "Fire", "Health", "Education", "Transit", "Environment", "Planning"};   // port labels
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

// ---- advisor messages ----
int InfoWindows::adviceString(int id) {
    switch (id) {
        case 0x10: return 301;   // crime too high
        case 0x3D: return 299;   // crime low
        case 0x11: return 325;   // traffic too high
        case 0x3E: return 326;   // traffic very low
        case 0x12: return 308;   // pollution
        case 0x13: return 322;   // education poor
        case 0x40: return 321;   // education fine
        case 0x14: return 316;   // health poor
        case 0x41: return 315;   // health fine
        default: return 0;
    }
}
int InfoWindows::advisorOf(int id) {
    switch (id) {
        case 0x10: case 0x3D: return 1;
        case 0x11: case 0x3E: return 5;
        case 0x12: return 6;
        case 0x13: case 0x40: return 4;
        case 0x14: case 0x41: return 3;
        default: return 7;
    }
}

void InfoWindows::postMessage(int id, int arg) {
    if (!adviceString(id)) return;
    const int prio = id;   // the original orders by a per-message priority table (0x4CC634); the order of ids is used here
    for (auto it = queue_.begin(); it != queue_.end(); ++it) if (it->id == id) { queue_.erase(it); break; }   // a repeated message only refreshes its entry
    AdvisorMessage m{id, arg, prio};
    auto it = queue_.begin();
    while (it != queue_.end() && it->priority >= prio) ++it;
    queue_.insert(it, m);
    if (queue_.size() > 6) queue_.pop_back();   // the queue holds 6 entries
}

// ---- drawing ----
void InfoWindows::draw(SDL_Renderer* r, int sw, int sh) {
    if (open == Which::None) return;
    if (!portraitsTried_) {
        portraitsTried_ = true;
        try {
            for (int id = 197; id <= 204; ++id) portraits_.push_back(std::make_unique<IconStrip>(r, Bmp8::fromDib(res_.bitmap(id)), 64, false));
        } catch (const std::exception&) { portraits_.clear(); }
    }
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
    text(r, x + 20, y + 6, "The advisors speak up when the simulation notices a problem (queue of 6, most urgent first).", 170, 180, 200);
    int row = 0;
    for (const AdvisorMessage& m : queue_) {
        const int by = y + 30 + row * 62;
        const int adv = advisorOf(m.id);
        fill(r, x + 14, by, kW - 28, 58, 40, 46, 62);
        if (static_cast<size_t>(adv) < portraits_.size()) portraits_[static_cast<size_t>(adv)]->drawSized(r, 0, x + 20, by + 2, 44, 54);
        text(r, x + 80, by + 8, kAdvisorNames[adv], 255, 255, 190);
        std::string t = res_.string(adviceString(m.id));
        if (t.size() > 66) { text(r, x + 80, by + 26, t.substr(0, 66)); text(r, x + 80, by + 40, t.substr(66, 66)); }
        else text(r, x + 80, by + 30, t);
        ++row;
    }
    if (queue_.empty()) text(r, x + 30, y + 40, res_.string(294));   // "I have no advice at this time."
}

void InfoWindows::drawNewspaper(SDL_Renderer* r, int x, int y) {
    const CityState& s = sim_.state();
    const int paper = (s.startYear + s.day / 300 + static_cast<int>(s.population10 % 7)) % 6;
    const std::string name = res_.string(360 + paper);
    fill(r, x + 20, y + 4, kW - 40, 340, 236, 232, 214);
    frame(r, x + 20, y + 4, kW - 40, 340, 70, 60, 40);
    text(r, x + kW / 2 - static_cast<int>(name.size()) * 12 / 2, y + 16, "THE " + name, 20, 16, 10);
    static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    text(r, x + 36, y + 34, std::string(months[s.month()]) + " " + std::to_string(s.startYear + s.day / 300), 60, 50, 30);
    text(r, x + 36, y + 60, "City population: " + std::to_string(s.population10) + "    Funds: " + money(s.treasury), 20, 16, 10);
    // opinion survey (the exe's survey lines); the percentages are the port's own scaling of the history indicators
    const CityState& st = s;
    auto pct = [](long long v, long long full) { return static_cast<int>(std::clamp<long long>(v * 100 / full, 0, 100)); };
    const int traffic = pct(st.graph[4 * 52], 128), pollute = pct(st.graph[5 * 52], 128), crime = pct(st.graph[7 * 52], 128), unemp = pct(st.c9610, 64);
    const int taxes = pct((st.budget[kRes].fundingPercent + st.budget[kCom].fundingPercent + st.budget[kInd].fundingPercent) / 3, 20);
    const int educate = 100 - pct(st.cb4cc, 100), health = 100 - pct(st.ca428, 100);
    const int ids[7] = {367, 368, 369, 370, 371, 372, 373};
    const int vals[7] = {traffic, pollute, crime, taxes, unemp, educate, health};
    text(r, x + 36, y + 96, res_.string(357), 20, 16, 10);   // "Opinion Poll"
    for (int i = 0; i < 7; ++i) {
        std::string t = res_.string(ids[i]);
        const size_t p = t.find("%d");
        if (p != std::string::npos) t.replace(p, 2, std::to_string(vals[i]));
        for (size_t q; (q = t.find("%%")) != std::string::npos;) t.erase(q, 1);   // wsprintf escape
        text(r, x + 36, y + 116 + i * 16, t, 40, 30, 20);
    }
    const int approval = 100 - (traffic + pollute + crime + taxes + unemp + educate + health) / 7;
    std::string mayor = res_.string(353);
    const size_t p = mayor.find("%d");
    if (p != std::string::npos) mayor.replace(p, 2, std::to_string(approval));
    for (size_t q; (q = mayor.find("%%")) != std::string::npos;) mayor.erase(q, 1);
    text(r, x + 36, y + 240, mayor, 20, 16, 10);
    if (!queue_.empty()) text(r, x + 36, y + 270, "Advisors: " + res_.string(adviceString(queue_.front().id)).substr(0, 60), 60, 40, 30);
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
