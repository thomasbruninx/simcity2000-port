// A folder / file browser for "Save City As" built from the widget toolkit (src/ui): a path line, a list of the folders and the .sc2 / .scn files of a
// directory, a file name field and Save / Cancel buttons.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "ui/widgets.h"

namespace sc2k {

class FileBrowser {
public:
    // `exts`: the file types listed (lower case, with the dot); the first one is appended to a typed name. `openMode`: choose an existing file (button "Open").
    void open(const std::string& title, const std::string& dir, const std::string& name, std::vector<std::string> exts = {".sc2", ".scn"}, bool openMode = false);
    bool active() const { return active_; }
    int result() const { return result_; }              // -2 open, 0 accepted (see path()), -1 cancelled
    std::string path() const;                           // directory + file name with the .sc2 extension
    void draw(SDL_Renderer* r, int w, int h);
    void mouseDown(int x, int y, int w, int h);
    bool key(SDL_Keycode k);
    void textInput(const std::string& utf8);
    void wheel(int dy);
    void close() { active_ = false; }

private:
    struct Entry { std::string name; bool dir; };
    struct Widgets { ui::Rect panel, pathLine; ui::ListBox list; ui::TextField name; ui::Button save, cancel; };
    Widgets layout(int w, int h) const;
    bool active_ = false;
    int result_ = -2;
    std::string title_, dir_;
    std::vector<std::string> exts_{".sc2", ".scn"};
    bool openMode_ = false;
    std::vector<Entry> entries_;
    ui::ListBox list_;          // items mirror entries_ (folders shown in brackets); only scroll / selection live here
    ui::TextField name_;
    void refresh();
    void accept();
};

}  // namespace sc2k
