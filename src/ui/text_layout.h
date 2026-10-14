// Text measuring, word wrapping and ellipsizing. The measure function is a parameter: the 8x8 debug font of SDL (8 pixels per character) or FreeType.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace sc2k::ui {

using Measure = std::function<int(const std::string&)>;   // width of a string in pixels

constexpr int kDebugCharW = 8, kDebugLineH = 8;
inline int debugWidth(const std::string& s) { return static_cast<int>(s.size()) * kDebugCharW; }
inline Measure debugMeasure() { return debugWidth; }

// Breaks `text` into lines no wider than `maxWidth`: at spaces when possible, inside a word that is wider than a line; '\n' forces a break (an empty line stays).
std::vector<std::string> wrapText(const std::string& text, int maxWidth, const Measure& measure = debugMeasure());
// Shortens `text` with "..." so that it fits in `maxWidth` (the end is cut off).
std::string ellipsize(const std::string& text, int maxWidth, const Measure& measure = debugMeasure());
// Same, but keeps the end (file paths): "...rest/of/the/path".
std::string ellipsizeStart(const std::string& text, int maxWidth, const Measure& measure = debugMeasure());

}  // namespace sc2k::ui
