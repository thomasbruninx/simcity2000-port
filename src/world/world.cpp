#include "world/world.h"

#include <cstring>

namespace sc2k {

World World::fromCity(const CityFile& c) {
    World w;
    w.xbld = c.xbld; w.xzon = c.xzon; w.xbit = c.xbit; w.xund = c.xund; w.xtxt = c.xtxt; w.xter = c.xter;
    w.altm = c.altm;
    w.xmic = c.xmic;
    w.xlab = c.xlab;
    // The files store each label Pascal-style (length byte, text); the game keeps C strings (0x44D140 reads and writes them that way)
    for (size_t i = 0; i < 256; ++i) {
        uint8_t* l = &w.xlab[i * 25];
        const size_t n = l[0];
        if (n == 0 || n > 24 || std::memchr(l + 1, 0, n) != nullptr) continue;   // empty or already a C string
        std::memmove(l, l + 1, n);
        l[n] = 0;
    }
    w.xthg = c.xthg;
    w.xtrf = c.xtrf; w.xplt = c.xplt; w.xval = c.xval; w.xcrm = c.xcrm;
    w.xplc = c.xplc; w.xfir = c.xfir; w.xpop = c.xpop; w.xrog = c.xrog;
    return w;
}

void World::writeBack(CityFile& c) const {
    c.xbld = xbld; c.xzon = xzon; c.xbit = xbit; c.xund = xund; c.xtxt = xtxt; c.xter = xter;
    c.altm = altm;
    c.xmic = xmic; c.xthg = xthg;
    const std::array<uint8_t, 6400> loaded = c.xlab;   // the records as the file had them
    c.xlab = xlab;
    auto decoded = [](const uint8_t* rec) {   // what fromCity() makes of a record
        const size_t n = rec[0];
        if (n == 0 || n > 24 || std::memchr(rec + 1, 0, n) != nullptr) return std::string(reinterpret_cast<const char*>(rec), strnlen(reinterpret_cast<const char*>(rec), 24));
        return std::string(reinterpret_cast<const char*>(rec + 1), n);
    };
    for (size_t i = 0; i < 256; ++i) {   // back to the file's Pascal strings
        uint8_t* l = &c.xlab[i * 25];
        if (decoded(&loaded[i * 25]) == std::string(reinterpret_cast<const char*>(l), strnlen(reinterpret_cast<const char*>(l), 24))) {   // unchanged: keep the file's bytes (including the unused tail the game left there)
            std::memcpy(l, &loaded[i * 25], 25);
            continue;
        }
        const size_t n = strnlen(reinterpret_cast<const char*>(l), 24);
        if (n == 0) { std::memset(l, 0, 25); continue; }
        std::memmove(l + 1, l, n);
        l[0] = static_cast<uint8_t>(n);
        std::memset(l + 1 + n, 0, 24 - n);
    }
    c.xtrf = xtrf; c.xplt = xplt; c.xval = xval; c.xcrm = xcrm;
    c.xplc = xplc; c.xfir = xfir; c.xpop = xpop; c.xrog = xrog;
}

}  // namespace sc2k
