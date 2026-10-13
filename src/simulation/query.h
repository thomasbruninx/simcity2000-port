// The query tool and the sign tool. `queryTile` is the data side of the original's tile-info dialog (paint routine 0x427640): which string of the exe's
// string table names the tile, and the labelled values shown under it. Texts come from the player's exe (see ui); numbers are computed here.
// Sign tool = 0x44D140 (labels 1..50 stored in World::xlab, tile overlay XTXT 1..0x32).
#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "simulation/sim_context.h"

namespace sc2k {

// A piece of a value line: a string-table id (>= 0) or literal text.
struct QueryPiece { int stringId = -1; std::string text; bool warn = false; };
struct QueryLine { int labelString = -1; std::vector<QueryPiece> value; };

struct QueryInfo {
    int a = 0, b = 0;
    int nameString = 0;                 // string-table id of the tile / building name
    int overlay = 0;                    // XTXT of the tile (sign, microsim record, thing marker, border link)
    std::vector<QueryLine> lines;
};

int tileNameString(const SimContext& c, int a, int b);       // 593 + group (name index of 0x42AE10(.., 2000, idx))
QueryInfo queryTile(const SimContext& c, int a, int b);

// Building-specific dialog of the microsimulation buildings (power plants, schools, stadium, ...): the original's 0x44CB30 shows the building's
// name (editable, stored in XLAB under the XTXT overlay value) and up to five lines taken from the strings 900 + table@0x4E6D90[class][line],
// where `#0` `#1` `#2` `#3` insert the record's byte at +1 and words at +2 / +4 / +6, `#G` a letter grade for the byte, `#S` the stadium's sport
// (string 786 + word at +4), `#T` the label named by the word at +6 and `#W` wins-losses of the byte (of 40 games).
struct MicrosimInfo {
    bool valid = false;
    int overlay = 0;                         // XTXT value 0x33..0xC8 (record slot + 0x33)
    int buildingId = 0;                      // byte 0 of the record
    int cls = 0;                             // 0x4E6C8A[id]: selects the line set
    std::array<int, 5> lineStrings{};        // string-table ids, -1 = no line
    uint8_t b1 = 0;
    uint16_t w2 = 0, w4 = 0, w6 = 0;
    std::string name;                        // XLAB label of the building ("" = unnamed)
    std::string teamLabel;                   // XLAB label number w6 (for #T)
};
MicrosimInfo microsimInfo(const SimContext& c, int a, int b);
std::string formatMicrosimLine(const MicrosimInfo& m, const std::string& pattern, const std::function<std::string(int)>& string);
bool renameBuilding(SimContext& c, int a, int b, const std::string& name);   // tail of 0x44CB30: the label is replaced by the edited text (at most 24 characters)

// Sign tool: `text` is what the player typed (empty = delete the sign); `cancelled` = the dialog was cancelled.
// Returns false (with a message through the hooks: 0x107 "50 signs", 0x108 "can't place a label here") when no sign could be placed or edited.
bool placeSign(SimContext& c, int a, int b, const std::string& text, bool cancelled);
std::string signText(const SimContext& c, int a, int b);        // text of the sign at the tile ("" if none)

}  // namespace sc2k
