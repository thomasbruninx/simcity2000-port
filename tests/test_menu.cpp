// The menu bar's logic (no drawing): bar titles open drop-downs, an item with sub-items opens its submenu, a click in the submenu chooses its command.
#include "app/menu_bar.h"
#include "test.h"

using namespace sc2k;

namespace {
PeResources::MenuItem item(int id, const char* text, std::vector<PeResources::MenuItem> sub = {}) {
    PeResources::MenuItem m; m.id = id; m.text = text; m.items = std::move(sub); return m;
}
}  // namespace

TEST(menu_bar_opens_submenus_and_returns_their_commands) {
    MenuBar bar;
    PeResources::MenuItem root = item(0, "", {item(0, "&File", {item(0, "Load", {item(110, "City"), item(111, "Source")}), item(0, "New", {item(140, "City")}), item(24310, "Exit")})});
    bar.load(root);
    int cmd = -1;
    CHECK(bar.mouseDown(10, 5, cmd) && bar.isOpen() && cmd == 0);   // the File title
    CHECK(bar.mouseDown(20, 28, cmd) && cmd == 0 && bar.isOpen());   // "Load" has a submenu: nothing is chosen yet, the menu stays open
    CHECK(bar.mouseDown(110, 28, cmd) && cmd == 110 && !bar.isOpen());   // the first item of the submenu
    CHECK(bar.mouseDown(10, 5, cmd) && bar.mouseDown(20, 44, cmd) && cmd == 0);   // "New"
    CHECK(bar.mouseDown(110, 44, cmd) && cmd == 140);
    CHECK(bar.mouseDown(10, 5, cmd) && bar.mouseDown(20, 60, cmd) && cmd == 24310 && !bar.isOpen());   // a plain item below the submenus
    CHECK(bar.mouseDown(10, 5, cmd) && bar.mouseDown(400, 300, cmd) && cmd == 0 && !bar.isOpen());   // a click elsewhere closes it
}
