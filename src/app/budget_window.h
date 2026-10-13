// The budget and ordinances windows (the original's dialogs 102 and 119). Labels and the ordinance names come from the dialog
// templates of the player's SIMCITY.EXE; the arithmetic (settlement, estimates) follows the ported finance code.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "resources/pe_resources.h"
#include "simulation/city_state.h"
#include "simulation/tables.h"

namespace sc2k {

class BudgetWindow {
public:
    BudgetWindow(CityState& state, const GameTables& tables, const PeResources& res);
    bool open = false;

    void draw(SDL_Renderer* r, int screenW, int screenH) const;
    bool mouseDown(int x, int y, int button, int screenW, int screenH);   // true when the click was inside the window (consumed)

    // arithmetic used by the window (also unit-tested)
    long long annual(int category) const;      // yearly amount at the current rate: income > 0, expense < 0
    long long toDate(int category) const;      // accumulated this budget year
    long long yearEnd(int category) const;     // to date + the remaining months at the current rate
    int rate(int row) const;                   // % shown for a main row (taxes: average rate)
    void adjust(int row, int direction);       // main row arrows (+-1 for taxes, +-5 for services)
    void adjustCategory(int category, int direction);
    void toggleOrdinance(int bit);
    void openDetail(int row) { detail_ = row; }

private:
    struct Row { std::string label; std::vector<int> categories; };
    CityState& s_;
    const GameTables& t_;
    std::vector<Row> rows_;                     // property taxes, ordinances, bonds, police, fire, health, education, transit
    std::vector<std::pair<std::string, int>> ordinances_;   // name, bit (0..19)
    int detail_ = -1;                           // row whose book is open
    static constexpr int kW = 620, kH = 490;
    int originX(int sw) const { return (sw - kW) / 2; }
    int originY(int sh) const { return (sh - kH) / 2; }
};

}  // namespace sc2k
