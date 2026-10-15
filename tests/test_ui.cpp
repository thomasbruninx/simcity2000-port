#include "test.h"
#include "ui/geometry.h"
#include "ui/text_layout.h"
#include "ui/widgets.h"

using namespace sc2k::ui;

TEST(ui_wrap_breaks_at_spaces_and_cuts_long_words) {
    auto lines = wrapText("one two three four", 8 * 9);   // 9 characters per line
    CHECK(lines.size() == 3 && lines[0] == "one two" && lines[1] == "three" && lines[2] == "four");
    lines = wrapText("abcdefghijklmnopqrstuvwxyz", 8 * 10);
    CHECK(lines.size() == 3 && lines[0] == "abcdefghij" && lines[2] == "uvwxyz");
    lines = wrapText("first\nsecond line is long", 8 * 12);
    CHECK(lines.size() == 3 && lines[0] == "first" && lines[1] == "second line" && lines[2] == "is long");
    for (const std::string& l : wrapText("a very long text that does not fit on a single line of the dialog", 8 * 20)) CHECK(l.size() <= 20);
    CHECK(wrapText("", 100).size() == 1);
}

TEST(ui_ellipsize_keeps_the_start_or_the_end) {
    CHECK(ellipsize("short", 8 * 10) == "short");
    CHECK(ellipsize("a rather long label", 8 * 10) == "a rathe...");
    CHECK(ellipsizeStart("/Users/someone/Library/saves", 8 * 12) == "...ary/saves");
    CHECK(ellipsize("x", 8) == "x");
}

TEST(ui_geometry_cuts_and_splits) {
    Rect r{10, 10, 100, 50};
    Rect top = r.cutTop(20);
    CHECK(top.y == 10 && top.h == 20 && r.y == 30 && r.h == 30);
    Rect left = r.cutLeft(40);
    CHECK(left.w == 40 && r.x == 50 && r.w == 60);
    const Rect strip{0, 0, 100, 10};
    auto cols = columns(strip, 2, 10);
    CHECK(cols.size() == 2 && cols[0].w == 45 && cols[1].x == 55);
    const Rect big{0, 0, 100, 100};
    CHECK(centered(big, 20, 10).x == 40);
    const Rect sq{0, 0, 10, 10};
    CHECK(sq.contains(9, 9) && !sq.contains(10, 5));
}

TEST(ui_slider_checkbox_list_and_text_field) {
    Slider s; s.r = {100, 0, 200, 10}; s.min = 0; s.max = 47;
    CHECK(s.press(100, 5) && s.value == 0);
    CHECK(s.drag(300) && s.value == 47);
    CHECK(s.drag(1000) && s.value == 47 && s.drag(-50) && s.value == 0);
    s.release();
    CHECK(!s.drag(150) && !s.press(0, 0));
    Checkbox c; c.r = {0, 0, 50, 10};
    CHECK(c.click(5, 5) && c.checked && !c.click(100, 100) && c.checked);
    ListBox l; l.r = {0, 0, 100, 36}; l.rowH = 18;
    for (int i = 0; i < 10; ++i) l.items.push_back("row" + std::to_string(i));
    CHECK(l.visibleRows() == 2 && l.maxScroll() == 8);
    l.wheel(-3);
    CHECK(l.scroll == 3 && l.itemAt(5, 20) == 4);
    CHECK(l.press(5, 5) && l.selected == 3);
    l.ensureVisible(9);
    CHECK(l.scroll == 8 && l.scrollBar().h >= 12);
    TextField t; t.maxLen = 5;
    t.textInput("ab\x01" "cdefg");
    CHECK(t.text == "abcde" && t.backspace() && t.text == "abcd");
    t.accept = [](char ch) { return ch != '/'; };
    t.textInput("/x");
    CHECK(t.text == "abcdx");
}

TEST(ui_text_field_selection_is_replaced_by_the_next_input) {
    ui::TextField f;
    f.text = "MYSET"; f.selected = true;
    f.textInput("ab");
    CHECK(f.text == "ab" && !f.selected);
    f.text = "MYSET"; f.selected = true;
    CHECK(f.backspace() && f.text.empty() && !f.selected);
    f.text = "abc";
    CHECK(f.backspace() && f.text == "ab");
}
