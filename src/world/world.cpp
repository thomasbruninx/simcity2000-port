#include "world/world.h"

namespace sc2k {

World World::fromCity(const CityFile& c) {
    World w;
    w.xbld = c.xbld; w.xzon = c.xzon; w.xbit = c.xbit; w.xund = c.xund; w.xtxt = c.xtxt; w.xter = c.xter;
    w.altm = c.altm;
    w.xmic = c.xmic;
    w.xlab = c.xlab;
    w.xthg = c.xthg;
    w.xtrf = c.xtrf; w.xplt = c.xplt; w.xval = c.xval; w.xcrm = c.xcrm;
    w.xplc = c.xplc; w.xfir = c.xfir; w.xpop = c.xpop; w.xrog = c.xrog;
    return w;
}

void World::writeBack(CityFile& c) const {
    c.xbld = xbld; c.xzon = xzon; c.xbit = xbit; c.xund = xund; c.xtxt = xtxt; c.xter = xter;
    c.altm = altm;
    c.xmic = xmic; c.xlab = xlab; c.xthg = xthg;
    c.xtrf = xtrf; c.xplt = xplt; c.xval = xval; c.xcrm = xcrm;
    c.xplc = xplc; c.xfir = xfir; c.xpop = xpop; c.xrog = xrog;
}

}  // namespace sc2k
