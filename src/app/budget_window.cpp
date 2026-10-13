#include "app/budget_window.h"

#include <algorithm>

#include "app/ui_draw.h"
#include "simulation/finance.h"

namespace sc2k {
using namespace ui;

namespace {
constexpr const char* kCategoryNames[16] = {"Residential tax", "Commercial tax", "Industrial tax", "Ordinances", "Bonds", "Police", "Fire", "Health", "Schools", "Colleges",
                                            "Roads", "Highways", "Bridges", "Rail", "Subway", "Tunnels"};
// checkbox control id of the ordinance dialog -> ordinance bit (the bit order of MISC 1000, documented by the community patch)
int bitOfCheckbox(int id) {
    switch (id) {
        case 148: return 0; case 151: return 1; case 149: return 2; case 150: return 3;
        default:
            if (id >= 156 && id <= 159) return 4 + (id - 156);
            if (id >= 164 && id <= 167) return 8 + (id - 164);
            if (id >= 172 && id <= 175) return 12 + (id - 172);
            if (id >= 180 && id <= 183) return 16 + (id - 180);
            return -1;
    }
}
}  // namespace

BudgetWindow::BudgetWindow(CityState& state, const GameTables& tables, const PeResources& res) : s_(state), t_(tables) {
    // rows: static texts of dialog 102 in template order (Property Taxes ... Transit Authority)
    std::vector<std::string> labels;
    if (const auto* d = res.dialog(102))
        for (const auto& c : d->controls)
            if (c.cls == "#130" && c.id == 0xFFFF && c.w >= 45 && c.x < 10 && !c.text.empty() && c.text.find('\n') == std::string::npos && c.text.find('$') == std::string::npos) labels.push_back(c.text);
    static const char* fallback[8] = {"Property Taxes", "City Ordinances", "Bond Payments", "Police Department", "Fire Department", "Health & Welfare", "Education", "Transit Authority"};
    auto label = [&](size_t i) { return i < labels.size() ? labels[i] : std::string(fallback[i]); };
    auto clean = [](std::string v) { for (size_t p; (p = v.find("&&")) != std::string::npos;) v.replace(p, 2, "&"); return v; };
    rows_ = {{clean(label(0)), {kRes, kCom, kInd}}, {clean(label(1)), {kOrdinance}}, {clean(label(2)), {kBond}}, {clean(label(3)), {kPolice}}, {clean(label(4)), {kFire}},
             {clean(label(5)), {kHealth}}, {clean(label(6)), {kSchool, kCollege}}, {clean(label(7)), {kRoad, kHighway, kBridge, kRail, kSubway, kTunnel}}};
    // ordinances: the dialog lists checkbox controls and name labels in the same template order
    if (const auto* d = res.dialog(119)) {
        std::vector<int> ids; std::vector<std::string> names;
        for (const auto& c : d->controls) {
            if (c.cls == "#128" && bitOfCheckbox(c.id) >= 0) ids.push_back(c.id);
            if (c.cls == "#130" && c.id == 0xFFFF && c.w >= 56 && c.x < 200 && c.y >= 16 && c.y <= 200 && !c.text.empty() && c.text.find('$') == std::string::npos) names.push_back(c.text);
        }
        // the template also contains group labels ("Finance", "Other", ...) of width >= 56 at the right; keep the first 20 entries that precede them
        for (size_t i = 0; i < ids.size() && i < names.size(); ++i) ordinances_.emplace_back(clean(names[i]), bitOfCheckbox(ids[i]));
    }
    std::sort(ordinances_.begin(), ordinances_.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
}

long long BudgetWindow::annual(int i) const { return budgetAnnual(s_, t_, i); }
long long BudgetWindow::toDate(int i) const { return budgetToDate(s_, t_, i); }
long long BudgetWindow::yearEnd(int i) const { return budgetYearEnd(s_, t_, i); }

int BudgetWindow::rate(int row) const {
    const auto& cats = rows_[static_cast<size_t>(row)].categories;
    long long sum = 0;
    for (int c : cats) sum += s_.budget[static_cast<size_t>(c)].fundingPercent;
    return static_cast<int>(sum / static_cast<long long>(cats.size()));
}

void BudgetWindow::adjustCategory(int category, int direction) {
    const bool tax = category <= kInd;
    int& f = s_.budget[static_cast<size_t>(category)].fundingPercent;
    f = std::clamp(f + direction * (tax ? 1 : 5), 0, tax ? 20 : 100);
}

void BudgetWindow::adjust(int row, int direction) {
    if (row == 1 || row == 2) return;   // ordinances and bonds have no rate
    for (int c : rows_[static_cast<size_t>(row)].categories) adjustCategory(c, direction);
}

void BudgetWindow::toggleOrdinance(int bit) { s_.ordinances ^= 1u << bit; }

void BudgetWindow::draw(SDL_Renderer* r, int sw, int sh) const {
    if (!open) return;
    const int ox = originX(sw), oy = originY(sh);
    fill(r, 0, 0, sw, sh, 0, 0, 0, 110);
    fill(r, ox, oy, kW, kH, 30, 34, 46);
    frame(r, ox, oy, kW, kH, 200, 200, 220);
    text(r, ox + 12, oy + 10, "Budget", 255, 255, 190);
    text(r, ox + 330, oy + 10, "To date", 190, 200, 220);
    text(r, ox + 440, oy + 10, "Year end", 190, 200, 220);
    text(r, ox + kW - 60, oy + 10, "[Done]", 255, 255, 190);
    long long todayTotal = 0, endTotal = 0;
    for (size_t i = 0; i < rows_.size(); ++i) {
        const int y = oy + 34 + static_cast<int>(i) * 24;
        long long td = 0, ye = 0;
        for (int c : rows_[i].categories) { td += toDate(c); ye += yearEnd(c); }
        todayTotal += td; endTotal += ye;
        fill(r, ox + 8, y - 3, kW - 16, 22, static_cast<int>(i) == detail_ ? 52 : 40, static_cast<int>(i) == detail_ ? 62 : 46, 62);
        text(r, ox + 14, y + 4, rows_[i].label);
        if (i != 1 && i != 2) {
            text(r, ox + 190, y + 4, "[-]");
            text(r, ox + 222, y + 4, std::to_string(rate(static_cast<int>(i))) + "%");
            text(r, ox + 262, y + 4, "[+]");
        } else if (i == 2) text(r, ox + 190, y + 4, std::to_string(s_.budget[kBond].fundingPercent) + "% interest", 170, 180, 200);
        text(r, ox + 330, y + 4, money(td), td < 0 ? 255 : 200, td < 0 ? 170 : 255, td < 0 ? 170 : 200);
        text(r, ox + 440, y + 4, money(ye), ye < 0 ? 255 : 200, ye < 0 ? 170 : 255, ye < 0 ? 170 : 200);
        if (i != 2) { if (bookIcon) bookIcon->drawSized(r, 0, ox + 298, y - 1, 24, 19); else text(r, ox + 300, y + 4, "[i]", 200, 220, 255); }
    }
    const int ys = oy + 34 + static_cast<int>(rows_.size()) * 24 + 10;
    text(r, ox + 14, ys, "Year to date cashflow  " + money(todayTotal));
    text(r, ox + 14, ys + 14, "Estimated cashflow     " + money(endTotal));
    text(r, ox + 14, ys + 28, "Current funds          " + money(s_.treasury));
    text(r, ox + 14, ys + 42, "End of year funds      " + money(static_cast<long long>(s_.treasury) + endTotal - todayTotal), 255, 255, 190);

    // detail panel of the opened book
    if (detail_ >= 0) {
        const int dy = ys + 66;
        fill(r, ox + 8, dy - 4, kW - 16, kH - (dy - oy) - 8, 22, 26, 36);
        if (detail_ == 1) {
            text(r, ox + 14, dy + 2, "Ordinances (click to enact / repeal)", 255, 255, 190);
            for (size_t k = 0; k < ordinances_.size(); ++k) {
                const int col = static_cast<int>(k) / 10, row = static_cast<int>(k) % 10;
                const int x = ox + 14 + col * 305, y = dy + 18 + row * 14;
                const bool on = (s_.ordinances >> ordinances_[k].second) & 1u;
                text(r, x, y, std::string(on ? "[x] " : "[ ] ") + ordinances_[k].first + "  " + money(ordinanceCost(s_, ordinances_[k].second)), on ? 190 : 150, on ? 255 : 160, on ? 190 : 170);
            }
        } else {
            text(r, ox + 14, dy + 2, rows_[static_cast<size_t>(detail_)].label + " detail", 255, 255, 190);
            int k = 0;
            for (int c : rows_[static_cast<size_t>(detail_)].categories) {
                const int y = dy + 18 + k++ * 14;
                text(r, ox + 14, y, kCategoryNames[c]);
                text(r, ox + 190, y, "[-]"); text(r, ox + 222, y, std::to_string(s_.budget[static_cast<size_t>(c)].fundingPercent) + "%"); text(r, ox + 262, y, "[+]");
                text(r, ox + 330, y, "units " + std::to_string(s_.budget[static_cast<size_t>(c)].currentCosts), 170, 180, 200);
                text(r, ox + 440, y, money(annual(c)) + "/yr", 200, 220, 255);
            }
        }
    }
}

bool BudgetWindow::mouseDown(int x, int y, int button, int sw, int sh) {
    if (!open) return false;
    (void)button;
    const int ox = originX(sw), oy = originY(sh);
    if (x < ox || y < oy || x >= ox + kW || y >= oy + kH) return true;   // clicks outside are swallowed while the window is open
    if (y < oy + 26 && x > ox + kW - 80) { open = false; return true; }
    for (size_t i = 0; i < rows_.size(); ++i) {
        const int ry = oy + 34 + static_cast<int>(i) * 24;
        if (y < ry - 3 || y >= ry + 19) continue;
        if (x >= ox + 186 && x < ox + 218) adjust(static_cast<int>(i), -1);
        else if (x >= ox + 258 && x < ox + 290) adjust(static_cast<int>(i), +1);
        else if (x >= ox + 296 && x < ox + 326 && i != 2) detail_ = detail_ == static_cast<int>(i) ? -1 : static_cast<int>(i);
        return true;
    }
    const int ys = oy + 34 + static_cast<int>(rows_.size()) * 24 + 10, dy = ys + 66;
    if (detail_ == 1) {
        for (size_t k = 0; k < ordinances_.size(); ++k) {
            const int col = static_cast<int>(k) / 10, row = static_cast<int>(k) % 10;
            const int bx = ox + 14 + col * 305, by = dy + 18 + row * 14;
            if (x >= bx && x < bx + 298 && y >= by - 2 && y < by + 12) toggleOrdinance(ordinances_[k].second);
        }
    } else if (detail_ >= 0) {
        int k = 0;
        for (int c : rows_[static_cast<size_t>(detail_)].categories) {
            const int by = dy + 18 + k++ * 14;
            if (y < by - 2 || y >= by + 12) continue;
            if (x >= ox + 186 && x < ox + 218) adjustCategory(c, -1);
            else if (x >= ox + 258 && x < ox + 290) adjustCategory(c, +1);
        }
    }
    return true;
}

}  // namespace sc2k
