#include "app/scurk/prompt.h"

#include "app/ui_draw.h"

namespace sc2k::scurk {

using namespace ui;

void Prompt::open(const std::string& title, std::vector<std::string> options, std::function<void(int, const std::string&)> done, bool field, const std::string& text, size_t maxLen) {
    title_ = title; options_ = std::move(options); done_ = std::move(done); field_ = field;
    text_.text = text; text_.maxLen = maxLen; text_.selected = !text.empty();
    active_ = true;
}

Prompt::Layout Prompt::layout(int w, int h) const {
    Layout l;
    const int pw = std::min(520, w - 40);
    const int th = std::max(kDebugLineH, wrappedHeight(title_, pw - 40));
    const int bh = 26;
    const int ph = 20 + th + 14 + (field_ ? 30 : 0) + static_cast<int>(options_.size()) * (bh + 4) + 12;
    l.panel = centered(Rect{0, 0, w, h}, pw, ph);
    Rect in = l.panel.inset(20, 0);
    in.cutTop(20);
    l.title = in.cutTop(th);
    in.cutTop(14);
    if (field_) { l.field = in.cutTop(22); in.cutTop(8); }
    for (const std::string& o : options_) { l.buttons.push_back({in.cutTop(bh), o}); in.cutTop(4); }
    return l;
}

void Prompt::draw(SDL_Renderer* r, int w, int h) {
    if (!active_) return;
    const Layout l = layout(w, h);
    fill(r, 0, 0, w, h, 0, 0, 0, 120);
    drawPanel(r, l.panel);
    wrappedText(r, l.title, title_, theme::kTitle);
    if (field_) { TextField f = text_; f.r = l.field; drawTextField(r, f); }
    for (const Button& b : l.buttons) drawButton(r, b);
}

void Prompt::finish(int choice) {
    active_ = false;
    auto cb = std::move(done_);
    if (cb) cb(choice, text_.text);
}

void Prompt::mouseDown(int x, int y, int w, int h) {
    if (!active_) return;
    const Layout l = layout(w, h);
    for (size_t i = 0; i < l.buttons.size(); ++i) if (l.buttons[i].hit(x, y)) { finish(static_cast<int>(i)); return; }
}

void Prompt::key(SDL_Keycode k) {
    if (!active_) return;
    if (k == SDLK_ESCAPE) finish(-1);
    else if (k == SDLK_RETURN) finish(0);
    else if (k == SDLK_BACKSPACE) text_.backspace();
}

void Prompt::textInput(const std::string& utf8) { if (active_ && field_) text_.textInput(utf8); }

}  // namespace sc2k::scurk
