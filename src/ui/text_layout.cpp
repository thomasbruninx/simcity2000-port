#include "ui/text_layout.h"

#include <algorithm>

namespace sc2k::ui {

std::vector<std::string> wrapText(const std::string& text, int maxWidth, const Measure& measure) {
    std::vector<std::string> lines;
    maxWidth = std::max(maxWidth, measure("M"));
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string para = text.substr(pos, nl - pos);
        if (!para.empty() && para.back() == '\r') para.pop_back();
        std::string cur;
        size_t i = 0;
        while (i < para.size()) {
            size_t j = para.find(' ', i);
            if (j == std::string::npos) j = para.size();
            std::string word = para.substr(i, j - i);
            i = j + 1;
            // a word wider than a whole line is cut into pieces
            while (!word.empty() && measure(word) > maxWidth) {
                if (!cur.empty()) { lines.push_back(cur); cur.clear(); }
                size_t k = word.size();
                while (k > 1 && measure(word.substr(0, k)) > maxWidth) --k;
                lines.push_back(word.substr(0, k));
                word.erase(0, k);
            }
            if (word.empty()) continue;
            if (!cur.empty() && measure(cur + " " + word) > maxWidth) { lines.push_back(cur); cur.clear(); }
            cur += (cur.empty() ? "" : " ") + word;
        }
        lines.push_back(cur);
        if (nl == text.size()) break;
        pos = nl + 1;
    }
    if (lines.size() > 1 && lines.back().empty() && !text.empty() && text.back() == '\n') lines.pop_back();
    return lines;
}

std::string ellipsize(const std::string& text, int maxWidth, const Measure& measure) {
    if (measure(text) <= maxWidth) return text;
    std::string t = text;
    while (!t.empty() && measure(t + "...") > maxWidth) t.pop_back();
    return t + "...";
}

std::string ellipsizeStart(const std::string& text, int maxWidth, const Measure& measure) {
    if (measure(text) <= maxWidth) return text;
    std::string t = text;
    while (!t.empty() && measure("..." + t) > maxWidth) t.erase(t.begin());
    return "..." + t;
}

}  // namespace sc2k::ui
