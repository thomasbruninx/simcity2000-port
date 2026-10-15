// An in-game viewer for the player's own help files (SC2USA.HLP, SCURK.HLP): contents, topics with clickable links, Back and a text search. The original hands
// these files to WinHelp; the port reads them itself (src/resources/winhelp.*). "Using help" explains this viewer.
#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "resources/winhelp.h"
#include "ui/widgets.h"

namespace sc2k {

class HelpViewer {
public:
    bool active = false;
    std::string error;                               // why the last open failed
    bool openFile(const std::string& path, const std::string& title);   // shows its first topic (the contents)
    void openContents();
    void openSearch();
    void openUsing();
    void close() { active = false; }

    void draw(SDL_Renderer* r, int w, int h);
    bool mouseDown(int x, int y, int button, int w, int h);
    void wheel(int dy);
    bool key(SDL_Keycode k);
    void textInput(const std::string& utf8);

    // scripting / tests
    int topic() const { return topic_; }
    void showTopic(int t, bool remember = true);

private:
    enum class Mode { Topic, Search, Using } mode_ = Mode::Topic;
    std::unique_ptr<WinHelp> help_;
    std::string path_, title_;
    std::vector<int> history_;
    int topic_ = 0, scroll_ = 0, contentHeight_ = 0, layoutW_ = -1, layoutTopic_ = -1;
    struct Word { int x, y; std::string text; int link; bool popup; };
    std::vector<Word> words_;
    ui::TextField query_;
    ui::ListBox results_;
    std::vector<int> resultTopics_;
    int lastW_ = 0, lastH_ = 0;

    struct Layout { ui::Rect panel, bar, content; std::vector<ui::Button> buttons; };
    Layout layout(int w, int h) const;
    void relayout(int width);
    void runSearch();
    void scrollBy(int d, int viewH);
};

}  // namespace sc2k
