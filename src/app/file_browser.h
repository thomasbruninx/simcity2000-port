// A small folder / file browser for "Save City As": lists the folders and the .sc2 / .scn files of a directory, a file name field, Save / Cancel.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace sc2k {

class FileBrowser {
public:
    void open(const std::string& title, const std::string& dir, const std::string& name);
    bool active() const { return active_; }
    int result() const { return result_; }              // -2 open, 0 saved (see path()), -1 cancelled
    std::string path() const;                           // directory + file name with the .sc2 extension
    void draw(SDL_Renderer* r, int w, int h);
    void mouseDown(int x, int y, int w, int h);
    bool key(SDL_Keycode k);
    void textInput(const std::string& utf8);
    void wheel(int dy);
    void close() { active_ = false; }

private:
    struct Entry { std::string name; bool dir; };
    bool active_ = false;
    int result_ = -2;
    std::string title_, dir_, name_;
    std::vector<Entry> entries_;
    int scroll_ = 0;
    static constexpr int kRows = 14, kRowH = 18;
    void refresh();
    void accept();
};

}  // namespace sc2k
