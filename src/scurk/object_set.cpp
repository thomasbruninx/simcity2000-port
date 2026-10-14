#include "scurk/object_set.h"

#include <algorithm>

namespace sc2k {

const char* objectGroupName(ObjectGroup g) {
    static const char* names[11] = {"Residential", "Commercial", "Industrial", "Special", "Power", "Transportation & Military", "Misc", "Animated I", "Animated II", "Construction", "All"};
    return names[static_cast<int>(g)];
}

ObjectCatalog ObjectCatalog::build(const MifFile& original, const PeResources& res) {
    ObjectCatalog c;
    // object order = first appearance of each id (mod 500) in ORIGINAL.MIF
    std::vector<int> ids;
    for (const MifShape& s : original.shapes) {
        const int base = s.id % 500 == 0 ? 500 : s.id % 500;
        if (std::find(ids.begin(), ids.end(), base) == ids.end()) ids.push_back(base);
    }
    // names: the strings 32001.. in order, long name then short name (one id is skipped inside the range: empty strings are not listed)
    std::vector<std::string> seq;
    for (int i = 32001; i < 32372; ++i) { const std::string s = res.string(i); if (!s.empty()) seq.push_back(s); }
    // group boundaries: the first object of each group in that list
    static const struct { const char* firstName; ObjectGroup group; } starts[] = {
        {"Large Apartments 1", ObjectGroup::Residential}, {"Office Park", ObjectGroup::Commercial}, {"Warehouse", ObjectGroup::Industrial}, {"Plymouth Arcology", ObjectGroup::Special},
        {"Hydroelectric Power Plant 1", ObjectGroup::Power}, {"Construction 1", ObjectGroup::Construction}, {"Subway Station", ObjectGroup::TransportMilitary}, {"Rubble 1", ObjectGroup::Misc},
        {"Tornado A", ObjectGroup::Animated1}, {"Bulldozer-SouthEast", ObjectGroup::Animated2}};
    ObjectGroup group = ObjectGroup::Residential;
    for (size_t k = 0; k < ids.size(); ++k) {
        ObjectInfo o;
        o.id = ids[k];
        if (2 * k + 1 < seq.size()) { o.name = seq[2 * k]; o.shortName = seq[2 * k + 1]; }
        else { o.name = "Object " + std::to_string(o.id); o.shortName = o.name; group = ObjectGroup::Misc; }
        for (const auto& st : starts) if (o.name == st.firstName) group = st.group;
        o.group = group;
        c.objects_.push_back(std::move(o));
    }
    return c;
}

const ObjectInfo* ObjectCatalog::find(int id) const {
    for (const ObjectInfo& o : objects_) if (o.id == id) return &o;
    return nullptr;
}

std::vector<const ObjectInfo*> ObjectCatalog::inGroup(ObjectGroup g) const {
    std::vector<const ObjectInfo*> out;
    for (const ObjectInfo& o : objects_) if (g == ObjectGroup::All || o.group == g) out.push_back(&o);
    return out;
}

bool ObjectSet::customized(int objectId, int zoom) const {
    const MifShape* s = mif_.find(spriteId(objectId, zoom));
    return s && !s->placeholder();
}

Sprite ObjectSet::sprite(int objectId, int zoom, const SpriteSet& originals) const {
    if (const MifShape* s = mif_.find(spriteId(objectId, zoom)); s && !s->placeholder()) return s->decode();
    if (const Sprite* o = originals.get(spriteId(objectId, zoom))) return *o;
    return Sprite{};
}

void ObjectSet::setSprite(int objectId, int zoom, const Sprite& s) {
    MifShape sh;
    sh.id = spriteId(objectId, zoom);
    if (const MifShape* old = mif_.find(sh.id)) sh.endArg = old->endArg;
    sh.setSprite(s);
    mif_.put(sh);
    dirty = true;
}

void ObjectSet::copyObject(const ObjectSet& from, int objectId) {
    for (int z = 0; z < 3; ++z)
        if (const MifShape* s = from.mif_.find(spriteId(objectId, z))) mif_.put(*s);
    if (const auto n = from.mif_.names.find(objectId); n != from.mif_.names.end()) { mif_.names[objectId] = n->second; mif_.order.emplace_back('N', objectId); }
    dirty = true;
}

std::string ObjectSet::name(int objectId, const std::string& fallback) const {
    const auto n = mif_.names.find(objectId);
    return n != mif_.names.end() && !n->second.empty() ? n->second : fallback;
}
void ObjectSet::setName(int objectId, const std::string& n) {
    if (!mif_.names.count(objectId)) mif_.order.emplace_back('N', objectId);
    mif_.names[objectId] = n;
    dirty = true;
}
void ObjectSet::clearName(int objectId) {
    mif_.names.erase(objectId);
    dirty = true;
}

void ObjectSet::applyTo(SpriteSet& target) const {
    target.clearOverrides();
    for (const MifShape& s : mif_.shapes) if (!s.placeholder()) target.setOverride(s.id, s.decode());
}

}  // namespace sc2k
