// The query tool and the sign tool. `queryTile` is the data side of the original's tile-info dialog (paint routine 0x427640): which string of the exe's
// string table names the tile, and the labelled values shown under it. Texts come from the player's exe (see ui); numbers are computed here.
// Sign tool = 0x44D140 (labels 1..50 stored in World::xlab, tile overlay XTXT 1..0x32).
#pragma once
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

// Sign tool: `text` is what the player typed (empty = delete the sign); `cancelled` = the dialog was cancelled.
// Returns false (with a message through the hooks: 0x107 "50 signs", 0x108 "can't place a label here") when no sign could be placed or edited.
bool placeSign(SimContext& c, int a, int b, const std::string& text, bool cancelled);
std::string signText(const SimContext& c, int a, int b);        // text of the sign at the tile ("" if none)

}  // namespace sc2k
