#include "app/help_viewer.h"

#include <algorithm>
#include <cctype>

#include "app/ui_draw.h"

namespace sc2k {

using namespace ui;

namespace {
constexpr int kLineH = 14, kParaGap = 8, kPad = 12;
constexpr const char* kUsing[] = {
    "How to use this help",
    "",
    "Contents shows the table of contents of the help file. Click an underlined entry to jump to its topic; entries underlined with a dotted line are short notes about a term and work the same way.",
    "Back returns to the topic you came from.",
    "Search lists every topic that contains the words you type; click one to read it.",
    "The wheel, the arrow keys, Page Up and Page Down scroll a long topic. Esc closes the help.",
    "",
    "This help is read from the help file of your own copy of the game; pictures in it are not shown in this port.",
};
}  // namespace

bool HelpViewer::openFile(const std::string& path, const std::string& title) {
    error.clear();
    if (!help_ || path_ != path) {
        try { help_ = std::make_unique<WinHelp>(WinHelp::load(path)); } catch (const std::exception& e) { help_.reset(); error = e.what(); return false; }
        path_ = path;
    }
    title_ = title;
    history_.clear();
    active = true;
    mode_ = Mode::Topic;
    topic_ = 0; scroll_ = 0; layoutTopic_ = -1;
    return true;
}

void HelpViewer::openContents() { if (!help_) return; active = true; mode_ = Mode::Topic; showTopic(0); }
void HelpViewer::openSearch() { if (!help_) return; active = true; mode_ = Mode::Search; query_.text.clear(); query_.maxLen = 40; results_.items.clear(); resultTopics_.clear(); results_.selected = -1; results_.scroll = 0; }
void HelpViewer::openUsing() { active = true; mode_ = Mode::Using; scroll_ = 0; }

void HelpViewer::showTopic(int t, bool remember) {
    if (!help_ || t < 0 || t >= static_cast<int>(help_->topics().size())) return;
    if (remember && mode_ == Mode::Topic && topic_ != t) history_.push_back(topic_);
    mode_ = Mode::Topic;
    topic_ = t; scroll_ = 0; layoutTopic_ = -1;
}

HelpViewer::Layout HelpViewer::layout(int w, int h) const {
    Layout l;
    l.panel = {std::min(24, w / 20), std::min(24, h / 20), w - 2 * std::min(24, w / 20), h - 2 * std::min(24, h / 20)};
    Rect inner = l.panel.inset(10, 8);
    l.bar = inner.cutTop(30);
    inner.cutTop(6);
    l.content = inner;
    const int bw = 92, gap = 6;
    int x = l.bar.x;
    for (const char* name : {"Contents", "Back", "Search", "Close"}) {
        Button b{Rect{x, l.bar.y, bw, 26}, name};
        if (std::string(name) == "Back") b.enabled = !history_.empty();
        l.buttons.push_back(b);
        x += bw + gap;
    }
    return l;
}

void HelpViewer::relayout(int width) {
    words_.clear();
    contentHeight_ = 0;
    if (!help_) return;
    const WinHelp::Topic& t = help_->topics()[static_cast<size_t>(topic_)];
    const int maxChars = std::max(10, width / kDebugCharW);
    int y = 0;
    // the title
    for (const std::string& line : wrapText(t.title, width)) { words_.push_back({0, y, line, -1, false}); y += kLineH + 2; }
    y += kParaGap;
    for (const WinHelp::Line& line : t.lines) {
        int x = 0;
        bool any = false;
        for (const WinHelp::Run& run : line.runs) {
            size_t i = 0;
            const std::string& s = run.text;
            while (i < s.size()) {
                size_t j = i;
                while (j < s.size() && s[j] != ' ') ++j;
                while (j < s.size() && s[j] == ' ') ++j;
                std::string word = s.substr(i, j - i);
                i = j;
                while (!word.empty()) {   // words longer than the line are cut
                    const int wlen = static_cast<int>(word.size());
                    if (x > 0 && x + std::min(wlen, maxChars) * kDebugCharW > width + kDebugCharW * static_cast<int>(word.size() - word.find_last_not_of(' ') - 1)) { x = 0; y += kLineH; }
                    const std::string part = word.substr(0, static_cast<size_t>(std::min(wlen, maxChars)));
                    words_.push_back({x, y, part, run.link, run.popup});
                    x += static_cast<int>(part.size()) * kDebugCharW;
                    word.erase(0, part.size());
                    any = true;
                }
            }
        }
        (void)any;
        y += kLineH + (line.paragraphEnd ? kParaGap : 0);
    }
    contentHeight_ = y;
    layoutW_ = width; layoutTopic_ = topic_;
}

void HelpViewer::runSearch() {
    results_.items.clear(); resultTopics_.clear(); results_.selected = -1; results_.scroll = 0;
    if (!help_ || query_.text.empty()) return;
    auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    const std::string q = lower(query_.text);
    std::vector<std::string> words;
    for (size_t i = 0; i < q.size();) {
        size_t j = q.find(' ', i);
        if (j == std::string::npos) j = q.size();
        if (j > i) words.push_back(q.substr(i, j - i));
        i = j + 1;
    }
    for (size_t t = 0; t < help_->topics().size(); ++t) {
        const std::string text = lower(help_->plainText(t));
        const bool all = std::all_of(words.begin(), words.end(), [&](const std::string& w) { return text.find(w) != std::string::npos; });
        if (all) { results_.items.push_back(help_->topics()[t].title); resultTopics_.push_back(static_cast<int>(t)); }
    }
}

void HelpViewer::draw(SDL_Renderer* r, int w, int h) {
    if (!active) return;
    lastW_ = w; lastH_ = h;
    fill(r, 0, 0, w, h, 0, 0, 0, 150);
    const Layout l = layout(w, h);
    drawPanel(r, l.panel);
    for (const Button& b : l.buttons) drawButton(r, b);
    clippedText(r, l.buttons.back().r.right() + 14, l.bar.y + 9, std::max(0, l.bar.right() - l.buttons.back().r.right() - 14), title_.empty() ? "Help" : title_, theme::kTitle);
    const Rect c = l.content;
    fill(r, c, theme::kRow);
    frame(r, c, theme::kPanelBorder);
    const SDL_Rect clip{c.x + 1, c.y + 1, c.w - 2, c.h - 2};
    SDL_SetRenderClipRect(r, &clip);
    const Rect body = c.inset(kPad, kPad);
    if (mode_ == Mode::Using) {
        int y = body.y - scroll_;
        for (size_t i = 0; i < sizeof kUsing / sizeof *kUsing; ++i) {
            if (kUsing[i][0] == 0) { y += kLineH; continue; }
            for (const std::string& line : wrapText(kUsing[i], body.w)) { text(r, body.x, y, line, i == 0 ? theme::kTitle : theme::kText); y += kLineH; }
            y += 4;
        }
        contentHeight_ = y + scroll_ - body.y;
    } else if (mode_ == Mode::Search) {
        query_.r = {body.x, body.y, std::min(body.w, 360), 22};
        text(r, body.x, body.y - 1, "", theme::kText);
        drawTextField(r, query_, true);
        text(r, query_.r.right() + 10, body.y + 5, std::to_string(results_.items.size()) + " topics", theme::kTextDim);
        results_.r = {body.x, body.y + 30, body.w, std::max(20, body.h - 30)};
        results_.rowH = 18;
        if (query_.text.empty()) text(r, body.x, body.y + 40, "Type a word to find the topics that contain it.", theme::kTextDim);
        else drawListBox(r, results_);
        contentHeight_ = 0;
    } else if (help_) {
        if (layoutW_ != body.w || layoutTopic_ != topic_) relayout(body.w);
        for (const Word& wd : words_) {
            const int y = body.y + wd.y - scroll_;
            if (y + kLineH < c.y || y > c.bottom()) continue;
            const Color col = wd.link >= 0 ? Color{130, 180, 255} : wd.popup ? Color{150, 230, 170} : (wd.y < 2 * kLineH + 4 && &wd == &words_.front()) ? theme::kTitle : theme::kText;
            text(r, body.x + wd.x, y, wd.text, col);
            if (wd.link >= 0 || wd.popup) {
                int len = static_cast<int>(wd.text.size());
                while (len > 0 && wd.text[static_cast<size_t>(len - 1)] == ' ') --len;
                for (int px = 0; px < len * kDebugCharW; px += (wd.popup && wd.link < 0) ? 3 : 1) fill(r, body.x + wd.x + px, y + 10, (wd.popup && wd.link < 0) ? 2 : 1, 1, col.r, col.g, col.b);
            }
        }
    }
    SDL_SetRenderClipRect(r, nullptr);
    const int viewH = body.h;
    if (mode_ != Mode::Search && contentHeight_ > viewH) {   // scroll bar
        const int th = c.h - 4, bh = std::max(16, th * viewH / contentHeight_);
        const int by = c.y + 2 + (th - bh) * scroll_ / std::max(1, contentHeight_ - viewH);
        fill(r, c.right() - 8, by, 5, bh, 110, 120, 150);
    }
    if (!error.empty()) text(r, l.panel.x + 12, l.panel.bottom() - 18, error, theme::kTextWarn);
}

void HelpViewer::scrollBy(int d, int viewH) {
    scroll_ = std::clamp(scroll_ + d, 0, std::max(0, contentHeight_ - viewH));
}

bool HelpViewer::mouseDown(int x, int y, int button, int w, int h) {
    if (!active) return false;
    if (button != 1) return true;
    const Layout l = layout(w, h);
    if (l.buttons[0].hit(x, y)) { if (help_) { mode_ = Mode::Topic; showTopic(0); } return true; }
    if (l.buttons[1].hit(x, y)) { if (!history_.empty()) { const int t = history_.back(); history_.pop_back(); mode_ = Mode::Topic; showTopic(t, false); } return true; }
    if (l.buttons[2].hit(x, y)) { openSearch(); return true; }
    if (l.buttons[3].hit(x, y)) { close(); return true; }
    if (!l.content.contains(x, y)) return true;
    const Rect body = l.content.inset(kPad, kPad);
    if (mode_ == Mode::Search) {
        if (results_.press(x, y)) { const int i = results_.selected; if (i >= 0 && i < static_cast<int>(resultTopics_.size())) { mode_ = Mode::Topic; showTopic(resultTopics_[static_cast<size_t>(i)]); } }
        return true;
    }
    if (mode_ == Mode::Topic)
        for (const Word& wd : words_) {
            const int wy = body.y + wd.y - scroll_;
            if (wd.link >= 0 && x >= body.x + wd.x && x < body.x + wd.x + static_cast<int>(wd.text.size()) * kDebugCharW && y >= wy && y < wy + kLineH) { showTopic(wd.link); return true; }
        }
    return true;
}

void HelpViewer::wheel(int dy) {
    if (!active) return;
    if (mode_ == Mode::Search) { results_.wheel(dy); return; }
    scrollBy(-dy * 3 * kLineH, std::max(40, lastH_ - 100));
}

bool HelpViewer::key(SDL_Keycode k) {
    if (!active) return false;
    const int viewH = std::max(40, lastH_ - 100);
    if (k == SDLK_ESCAPE) { if (mode_ == Mode::Search && !query_.text.empty()) { query_.text.clear(); runSearch(); } else close(); return true; }
    if (mode_ == Mode::Search) {
        if (k == SDLK_BACKSPACE) { query_.backspace(); runSearch(); }
        else if (k == SDLK_RETURN && !resultTopics_.empty()) { mode_ = Mode::Topic; showTopic(resultTopics_.front()); }
        return true;
    }
    if (k == SDLK_DOWN) scrollBy(kLineH * 2, viewH);
    else if (k == SDLK_UP) scrollBy(-kLineH * 2, viewH);
    else if (k == SDLK_PAGEDOWN || k == SDLK_SPACE) scrollBy(viewH - 2 * kLineH, viewH);
    else if (k == SDLK_PAGEUP) scrollBy(-(viewH - 2 * kLineH), viewH);
    else if (k == SDLK_HOME) scroll_ = 0;
    else if (k == SDLK_END) scrollBy(contentHeight_, viewH);
    else if (k == SDLK_BACKSPACE && !history_.empty()) { const int t = history_.back(); history_.pop_back(); showTopic(t, false); }
    return true;
}

void HelpViewer::textInput(const std::string& utf8) {
    if (!active || mode_ != Mode::Search) return;
    query_.textInput(utf8);
    runSearch();
}

}  // namespace sc2k
