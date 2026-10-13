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
    // Bitmaps stored under a name instead of a number (the toolbar buttons: e.g. "BULLDOZERU"); names upper-cased as stored.
    std::vector<uint8_t> namedBitmap(const std::string& name) const { const auto it = namedBitmaps_.find(name); return it == namedBitmaps_.end() ? std::vector<uint8_t>() : it->second; }
    const std::map<std::string, std::vector<uint8_t>>& namedBitmaps() const { return namedBitmaps_; }
    // Raw bytes of a custom resource (type given by name, e.g. "DATA"), empty when missing.
    std::vector<uint8_t> custom(const std::string& type, int id) const { const auto t = custom_.find(type); if (t == custom_.end()) return {}; const auto i = t->second.find(id); return i == t->second.end() ? std::vector<uint8_t>() : i->second; }
    const std::map<int, std::string>& strings() const { return strings_; }
    // Controls of a dialog template (RT_DIALOG): id and caption text of every control, in template order.
    struct DialogControl { int id; int x, y, w, h; std::string cls; std::string text; };
    struct Dialog { std::string title; std::vector<DialogControl> controls; };
    const Dialog* dialog(int id) const { const auto it = dialogs_.find(id); return it == dialogs_.end() ? nullptr : &it->second; }
    const std::map<int, Dialog>& dialogs() const { return dialogs_; }
    // Menu templates (RT_MENU): items with command id (0 for separators / popups), text with '&' accelerators, flags (MF_*) and sub-items.
    struct MenuItem { int id = 0; unsigned flags = 0; std::string text; std::vector<MenuItem> items; };
    const std::map<std::string, MenuItem>& menus() const { return menus_; }   // key: resource name, or "#id"


private:
    std::map<int, std::vector<uint8_t>> bitmaps_;
    std::map<std::string, std::vector<uint8_t>> namedBitmaps_;
    std::map<int, std::string> strings_;
    std::map<std::string, std::map<int, std::vector<uint8_t>>> custom_;
    std::map<int, Dialog> dialogs_;
    std::map<std::string, MenuItem> menus_;
};

}  // namespace sc2k
