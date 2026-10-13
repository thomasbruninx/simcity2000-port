// Information windows: history graphs (data and series names are the original's), city statistics, advisors and the newspaper.
// Notes on fidelity (docs/ui.md): the graph data is the ported history (0x413910); the advisor message queue follows 0x47BA90 (6 entries, priority-ordered) but the
// advisor texts / topics assigned to message ids and the newspaper content are the port's own reading of the exe's strings.
#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "app/icons.h"
#include "resources/pe_resources.h"
#include "simulation/advisors.h"
#include "simulation/news.h"
#include "simulation/simulation.h"

namespace sc2k {

class InfoWindows {
public:
    enum class Which { None, Graphs, City, Advisors, Newspaper };
    InfoWindows(Simulation& sim, const PeResources& res);

    Which open = Which::None;
    std::string cityName;             // shown by the newspaper stories ('=' token)
    void draw(SDL_Renderer* r, int screenW, int screenH);
    bool mouseDown(int x, int y, int screenW, int screenH);   // true when consumed (a window is open: clicks outside close nothing but are swallowed)
    void postMessage(int id, int arg);                        // from ZoneHooks::postMessage: the news queue (0x47BA90)
    void age() { news_.age(); }                               // from ZoneHooks::newsAge (0x479F4B)
    void initNews(const std::string& gameDir);                // loads DATA/DATA_USA.* (the newspaper's text grammar)
    bool popupDue() { const bool d = popup_; popup_ = false; return d; }   // a milestone / new-tool story arrived: the newspaper pops up
    const NewsRoom& news() const { return news_; }

private:
    Simulation& sim_;
    const PeResources& res_;
    NewsData newsData_;
    NewsRoom news_;
    bool popup_ = false;
    int page_ = 0, newsOpened_ = 0;
    Which last_ = Which::None;
    uint32_t session_ = 1;
    int period_ = 0;                  // graphs: 0 one year, 1 ten years, 2 a hundred years
    uint32_t shown_ = 0x1FF;          // graphs: bit per series
    std::vector<std::string> seriesNames_;
    std::vector<std::unique_ptr<IconStrip>> portraits_;
    bool portraitsTried_ = false;
    static constexpr int kW = 640, kH = 440;
    int ox(int sw) const { return (sw - kW) / 2; }
    int oy(int sh) const { return (sh - kH) / 2; }
    void drawGraphs(SDL_Renderer* r, int x, int y);
    void drawCity(SDL_Renderer* r, int x, int y);
    void drawAdvisors(SDL_Renderer* r, int x, int y);
    void drawNewspaper(SDL_Renderer* r, int x, int y);
};

}  // namespace sc2k
