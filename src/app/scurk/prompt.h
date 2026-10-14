// A small modal question for the resource editors (non-blocking: the host calls draw / input while it is open and reads `result` afterwards).
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/widgets.h"

namespace sc2k::scurk {

class Prompt {
public:
    // Opens a question with option buttons and, when `field` is set, a text entry preset with `text`. `done(choice, text)` runs once (choice -1 = cancelled).
    void open(const std::string& title, std::vector<std::string> options, std::function<void(int, const std::string&)> done, bool field = false, const std::string& text = {}, size_t maxLen = 32);
    bool active() const { return active_; }
    void draw(SDL_Renderer* r, int w, int h);
    void mouseDown(int x, int y, int w, int h);
    void key(SDL_Keycode k);
    void textInput(const std::string& utf8);

private:
    struct Layout { ui::Rect panel, title, field; std::vector<ui::Button> buttons; };
    Layout layout(int w, int h) const;
    void finish(int choice);
    bool active_ = false, field_ = false;
    std::string title_;
    std::vector<std::string> options_;
    std::function<void(int, const std::string&)> done_;
    ui::TextField text_;
};

}  // namespace sc2k::scurk
