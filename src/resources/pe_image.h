// Minimal read-only PE32 reader: maps virtual addresses of a Windows executable to its on-disk bytes.
// Used to read constant tables from the user's own SIMCITY.EXE (no game data is stored in this repository).
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sc2k {

class PeImage {
public:
    static PeImage load(const std::string& path);   // throws Sc2Error on missing/invalid file
    uint32_t timestamp() const { return timestamp_; }
    uint32_t imageBase() const { return imageBase_; }
    // Bytes at a virtual address (image base included). Throws if not backed by file data.
    std::span<const uint8_t> bytesAt(uint32_t va, size_t n) const;

    struct Section { uint32_t va, vsize, rawPtr, rawSize; };   // va is an RVA
    const std::vector<Section>& sections() const { return sections_; }
    uint32_t sizeOfImage() const { return sizeOfImage_; }
    std::span<const uint8_t> file() const { return file_; }

private:
    std::vector<uint8_t> file_;
    std::vector<Section> sections_;
    uint32_t imageBase_ = 0, timestamp_ = 0, sizeOfImage_ = 0;
};

// Build .022 of the 1995 CD Collection: PE timestamp 0x302FEA8A (docs/known-versions.md).
constexpr uint32_t kTimestampWin95Cd1995 = 0x302FEA8A;

}  // namespace sc2k
