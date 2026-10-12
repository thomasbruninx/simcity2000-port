// Byte RLE used by SimCity 2000 saved cities (docs/formats/save-sc2.md). VERIFIED on all 61 supplied files.
//   control c in 0x01..0x7F: copy next c bytes; 0x81..0xFF: repeat next byte (c - 127) times; 0x80 invalid.
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace sc2k {

// Throws Sc2Error on malformed input (control byte 0x80, truncated literal/run, zero-length literal).
std::vector<uint8_t> rleDecode(std::span<const uint8_t> in);

// Canonical encoder: runs of >= 2 identical bytes become repeat records (max 128), the rest literals (max 127).
// Decodes back to the input exactly, but is NOT guaranteed to reproduce the original game's bytes
// (it does for ~55% of sample chunks); unmodified chunks are therefore kept verbatim by CityFile.
std::vector<uint8_t> rleEncode(std::span<const uint8_t> in);

}  // namespace sc2k
