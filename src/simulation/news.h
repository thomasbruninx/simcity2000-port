// The newspaper: the port of SIMCITY.EXE's news queue (0x47BA90 insert, 0x479F4B ageing, 0x479C00 start-up) and its text generator 0x47A700
// (with the headline / article wrappers 0x47A3B0 and 0x47A5C0). The text grammar is data: DATA/DATA_USA.DAT of the player's installation holds
// 250 rule groups of alternative texts (headlines "HEADLINE+article", names, verbs, ...), the message priorities and their monthly decay.
// A message id (as passed to ZoneHooks::postMessage) is the number of the rule group of its story. See docs/simulation/newspaper.md.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "simulation/rng.h"

namespace sc2k {

struct NewsData {
    std::array<uint16_t, 250> start{}, count{};          // DATA 1000 / 1001: first rule index and number of alternatives of each group
    std::vector<uint32_t> offset;                       // DATA 1002: rule index -> byte offset in `text`
    std::vector<uint8_t> text;                          // DATA 1003: NUL-terminated rules
    std::array<int16_t, 80> priority{}, decay{};        // DATA 1004 / 1005: priority of a message id and its decay per month
    std::array<int16_t, 256> digraph{};                 // exe table @0x4E9218: byte -> rule index used to compress the text (0 = literal)
    bool loaded = false;
    // `dir` is the game directory (DATA/DATA_USA.IDX + .DAT); `digraphTable` is read from the exe (GameTables::newsDigraph)
    static NewsData load(const std::string& dir, const std::array<int16_t, 256>& digraphTable);
};

struct NewsEntry {
    int id = 0;                  // message id = rule group
    int priority = 0;            // current priority (decays every month)
    uint8_t arg = 0;             // argument of the message (building class, neighbour, ...)
    uint8_t c5 = 0xFF, c6 = 0xFF, c7 = 0xFF;   // per-story cached random choices
};

class NewsRoom {
public:
    explicit NewsRoom(const NewsData* data = nullptr) : data_(data) {}
    void setData(const NewsData* data) { data_ = data; }
    void reset(CrtRand& rng);                      // 0x479C00: nine filler stories (ids 11..19) and the shuffled page layout
    void post(int id, int arg);                    // 0x47BA90: insert by priority into the first seven slots
    void age();                                    // 0x479F4B: priorities decay, then the slots are sorted
    const std::array<NewsEntry, 9>& entries() const { return entries_; }
    // page layout (0x4C9D3C): 6 pages x 5 bytes {paper name, layout, motto, topic, ...}
    const std::array<uint8_t, 30>& pages() const { return pages_; }

    struct Context {
        std::string cityName;                                     // '=' token
        std::function<std::string(int)> label;                    // XLAB label by index ('>' uses 0xFB + argument, '~' label 0)
    };
    // 0x47A3B0: the headline of story `entry` (resets the per-article cache of '[' tokens); `rng` should be seeded per page by the caller
    std::string headline(int entry, CrtRand& rng, const Context& ctx);
    // 0x47A5C0: the article text that follows the '+' of the same rule (same cache); sentences start with capitals
    std::string article(int entry, CrtRand& rng, const Context& ctx);

private:
    const NewsData* data_;
    std::array<NewsEntry, 9> entries_{};
    std::array<uint8_t, 30> pages_{};
    std::array<uint8_t, 250> cache_{};
    std::string expand(int rule, int entry, int mode, CrtRand& rng, const Context& ctx);
    void expandInto(std::string& out, int rule, int entry, int mode, CrtRand& rng, const Context& ctx);
};

}  // namespace sc2k
