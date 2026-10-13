#include "simulation/availability.h"

namespace sc2k {

void ToolAvailability::applyEvent(int i, const CityState& s) {
    switch (i) {
        case 0: plantMask |= 8; break;
        case 1: if (!(s.ordinances & 0x20000)) plantMask |= 0x10; break;   // nuclear plants need the Nuclear Free Zone ordinance off
        case 2: plantMask |= 0x40; break;
        case 3: plantMask |= 0x20; break;
        case 4: plantMask |= 0x80; break;
        case 5: plantMask |= 0x100; break;
        case 6: mask[8] |= 2; break;       // airport zone
        case 7: mask[6] |= 0xA; break;     // highway, onramp
        case 8: mask[6] |= 0x10; break;    // bus depot
        case 9: mask[7] |= 0x1A; break;    // subway, subway station, rail-subway link
        case 10: mask[4] |= 8; break;      // water treatment
        case 11: mask[4] |= 0x10; break;   // desalination
        case 12: case 13: case 14: case 15: ++arcologies; break;
        default: break;
    }
}

ToolAvailability ToolAvailability::fromState(const CityState& s, const GameTables& t) {
    ToolAvailability a;
    for (size_t k = 0; k < 18; ++k) if (k != 5) a.mask[k] = static_cast<uint32_t>(t.toolAvailBase[k]);
    for (int i = 0; i < 17; ++i) if (s.eventYears[static_cast<size_t>(i)] == 0) a.applyEvent(i, s);
    return a;
}

bool ToolAvailability::allows(int category, int sub) const {
    if (category == 0 || category == 1) return true;
    if (category == 3) return sub == 0 || (sub >= 2 && sub <= 10 && ((plantMask >> (sub - 2)) & 1u));
    if (category == 5) return sub < 5 || (sub >= 5 && sub <= 8 && arcologies > sub - 5);   // rewards 0..4 are granted by the council (not modelled); arcologies by date
    if (category >= 18) return true;
    return (mask[static_cast<size_t>(category)] >> sub) & 1u;
}

}  // namespace sc2k
