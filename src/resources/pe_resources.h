// Reads RT_STRING and RT_BITMAP resources of the user's SIMCITY.EXE (the game's menu texts and button bitmaps).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "resources/pe_image.h"

namespace sc2k {

class PeResources {
public:
    explicit PeResources(const PeImage& pe);
    std::string string(int id) const;                 // text of string-table entry `id` (ANSI bytes as UTF-8-compatible ASCII), "" when missing
    // Raw DIB (BITMAPINFOHEADER + palette + pixels) of RT_BITMAP resource `id`, empty when missing.
    std::vector<uint8_t> bitmap(int id) const;
    std::vector<int> bitmapIds() const;
    const std::map<int, std::string>& strings() const { return strings_; }
    // Controls of a dialog template (RT_DIALOG): id and caption text of every control, in template order.
    struct DialogControl { int id; int x, y, w, h; std::string cls; std::string text; };
    struct Dialog { std::string title; std::vector<DialogControl> controls; };
    const Dialog* dialog(int id) const { const auto it = dialogs_.find(id); return it == dialogs_.end() ? nullptr : &it->second; }
    const std::map<int, Dialog>& dialogs() const { return dialogs_; }

private:
    std::map<int, std::vector<uint8_t>> bitmaps_;
    std::map<int, std::string> strings_;
    std::map<int, Dialog> dialogs_;
};

}  // namespace sc2k
