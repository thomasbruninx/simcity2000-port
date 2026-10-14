// SCURK's objects: the catalogue (ids, names, groups) and object sets (a .MIF file's graphics; unchanged objects fall back to the originals).
#pragma once
#include <string>
#include <vector>

#include "resources/pe_resources.h"
#include "resources/sprite_archive.h"
#include "scurk/mif.h"

namespace sc2k {

enum class ObjectGroup { Residential, Commercial, Industrial, Special, Power, TransportMilitary, Misc, Animated1, Animated2, Construction, All };
const char* objectGroupName(ObjectGroup g);   // as in WINSCURK's "Object Group" menu

struct ObjectInfo {
    int id = 0;                 // tile / building id 1..499; the sprites are id (small), id + 500 (medium), id + 1000 (large)
    std::string name, shortName;
    ObjectGroup group = ObjectGroup::Misc;
};

// The 186 objects of SCURK in the order of ORIGINAL.MIF, named from WINSCURK.EXE's string table (32001..), grouped by their place in that list.
class ObjectCatalog {
public:
    static ObjectCatalog build(const MifFile& original, const PeResources& scurkResources);
    const std::vector<ObjectInfo>& objects() const { return objects_; }
    const ObjectInfo* find(int id) const;
    std::vector<const ObjectInfo*> inGroup(ObjectGroup g) const;   // All = everything
private:
    std::vector<ObjectInfo> objects_;
};

// A set of object graphics. Zoom 0 = small, 1 = medium, 2 = large.
class ObjectSet {
public:
    ObjectSet() = default;
    explicit ObjectSet(MifFile mif, std::string path = {}) : mif_(std::move(mif)), path_(std::move(path)) {}
    static ObjectSet load(const std::string& path) { return ObjectSet(MifFile::load(path), path); }
    MifFile& mif() { return mif_; }
    const MifFile& mif() const { return mif_; }
    const std::string& path() const { return path_; }
    void setPath(std::string p) { path_ = std::move(p); }
    bool dirty = false;

    static int spriteId(int objectId, int zoom) { return objectId + 500 * zoom; }
    bool customized(int objectId, int zoom) const;                   // a real (not placeholder) shape is stored
    // The graphics to show: the stored shape, or the original from the game's archives when the set does not change it.
    Sprite sprite(int objectId, int zoom, const SpriteSet& originals) const;
    void setSprite(int objectId, int zoom, const Sprite& s);          // marks the set as changed
    void copyObject(const ObjectSet& from, int objectId);              // all three zooms, name included
    std::string name(int objectId, const std::string& fallback) const;
    void setName(int objectId, const std::string& n);
    void clearName(int objectId);
    // Replacement graphics for the renderer: every customised shape of the set becomes an override of `target`.
    void applyTo(SpriteSet& target) const;

private:
    MifFile mif_;
    std::string path_;
};

}  // namespace sc2k
