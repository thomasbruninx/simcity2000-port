#include "save/rle.h"

#include "save/error.h"

namespace sc2k {

std::vector<uint8_t> rleDecode(std::span<const uint8_t> in) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < in.size()) {
        const uint8_t c = in[i++];
        if (c == 0) throw Sc2Error("RLE: zero-length literal");
        if (c <= 0x7F) {
            if (i + c > in.size()) throw Sc2Error("RLE: truncated literal");
            out.insert(out.end(), in.begin() + i, in.begin() + i + c);
            i += c;
        } else if (c == 0x80) {
            throw Sc2Error("RLE: invalid control byte 0x80");
        } else {
            if (i >= in.size()) throw Sc2Error("RLE: truncated run");
            out.insert(out.end(), static_cast<size_t>(c) - 127, in[i++]);
        }
    }
    return out;
}

std::vector<uint8_t> rleEncode(std::span<const uint8_t> in) {
    std::vector<uint8_t> out;
    std::vector<uint8_t> lit;
    auto flush = [&] {
        for (size_t p = 0; p < lit.size();) {
            const size_t k = std::min<size_t>(127, lit.size() - p);
            out.push_back(static_cast<uint8_t>(k));
            out.insert(out.end(), lit.begin() + p, lit.begin() + p + k);
            p += k;
        }
        lit.clear();
    };
    size_t i = 0;
    while (i < in.size()) {
        size_t j = i;
        while (j < in.size() && in[j] == in[i] && j - i < 128) ++j;
        if (j - i >= 2) {
            flush();
            out.push_back(static_cast<uint8_t>(127 + (j - i)));
            out.push_back(in[i]);
            i = j;
        } else {
            lit.push_back(in[i++]);
        }
    }
    flush();
    return out;
}

}  // namespace sc2k
