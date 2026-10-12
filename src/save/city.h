// SimCity 2000 saved city / scenario (.SC2 / .SCN) reader and writer.
// Format: docs/formats/save-sc2.md. Multi-byte values in the file are big-endian; accessors expose host values.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sc2k {

// Row-major grid. `row` is the FIRST index of the game's row-pointer tables (layer[row][col]); the file stores
// rows in order. Which of row/col is the screen X or Y is not yet established (docs/simulation/map-layers-and-globals.md).
template <typename T, int N>
struct Grid {
    static constexpr int kSize = N;
    std::array<T, static_cast<size_t>(N) * N> v{};
    T& at(int row, int col) { return v[static_cast<size_t>(row) * N + col]; }
    const T& at(int row, int col) const { return v[static_cast<size_t>(row) * N + col]; }
    bool operator==(const Grid&) const = default;
};

struct RawChunk {
    std::string tag;                  // 4 characters
    std::vector<uint8_t> stored;      // bytes exactly as in the file (possibly RLE)
};

class CityFile {
public:
    static constexpr int kMap = 128;
    static constexpr int kHalf = 64;
    static constexpr int kQuarter = 32;
    static constexpr int kGraphSeries = 16;
    static constexpr int kGraphEntries = 52;

    // Parse a complete file. Throws Sc2Error with a descriptive message on any structural problem
    // (bad FORM/SCDH header, length mismatch, truncated chunk, wrong decoded layer size, missing layer).
    static CityFile parse(std::span<const uint8_t> file);
    static CityFile load(const std::string& path);

    // Exactly the bytes that were parsed (chunks emitted verbatim). Valid even if layers were edited afterwards
    // only when nothing was changed -- use serialize() after edits.
    std::vector<uint8_t> serializeOriginal() const;
    // Canonical output built from the typed layers (RLE re-encoded, canonical chunk order, unknown chunks kept).
    std::vector<uint8_t> serialize() const;

    bool isScenario() const { return hasScenarioChunks; }

    // --- typed content ---
    std::optional<std::array<uint8_t, 32>> cnam;  // city name chunk, Pascal-style (absent in some files)
    std::array<uint8_t, 4800> misc{};              // 1200 big-endian 32-bit words; field map TBD
    Grid<uint16_t, kMap> altm;                      // bits 0-4 altitude, 5-9 water level, 10-15 tunnel depth
    Grid<uint8_t, kMap> xter, xbld, xzon, xund, xtxt, xbit;
    std::array<uint8_t, 6400> xlab{};               // 256 labels x 25 bytes
    std::array<uint8_t, 1200> xmic{};               // 150 microsim records x 8 bytes
    std::array<uint8_t, 480> xthg{};                // 40 things x 12 bytes
    Grid<uint8_t, kHalf> xtrf, xplt, xval, xcrm;    // traffic, pollution, land value, crime
    Grid<uint8_t, kQuarter> xplc, xfir, xpop, xrog; // police, fire, population density, rate of growth
    std::array<int32_t, kGraphSeries * kGraphEntries> xgrp{};  // graph history, host-endian

    // Chunks we do not model (scenario TEXT/SCEN/PICT/TMPL and anything unknown), with their original index.
    struct Extra { size_t position; RawChunk chunk; };
    std::vector<Extra> extras;

    uint32_t miscWord(size_t i) const;   // big-endian word i of MISC (i < 1200)
    std::string name() const;            // decoded CNAM (empty if absent)

private:
    std::vector<RawChunk> original_;
    bool hasScenarioChunks = false;
};

}  // namespace sc2k
