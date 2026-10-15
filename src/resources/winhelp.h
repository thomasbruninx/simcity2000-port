// Reader for Windows 3.x / 95 help files (.HLP: SC2USA.HLP, SCURK.HLP): the internal file system, the |TOPIC text records (LZ77 + |Phrases) and the
// hyperlinks. Only text and hyperlinks are read; pictures are skipped. Format knowledge: docs/formats/winhelp.md. Nothing of the files is stored in the repository.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sc2k {

class WinHelp {
public:
    struct Run { std::string text; int link = -1; bool popup = false; };   // link: topic index (-1 = plain text)
    struct Line { std::vector<Run> runs; bool paragraphEnd = false; };      // paragraphEnd: a blank line follows
    struct Topic { std::string title; std::vector<Line> lines; };

    static WinHelp load(const std::string& path);   // throws std::runtime_error
    const std::vector<Topic>& topics() const { return topics_; }
    // plain text of a topic (for searching)
    std::string plainText(size_t topic) const;

private:
    std::vector<Topic> topics_;
};

}  // namespace sc2k
