// Whether ASTC blocks decode with every texel's alpha at 255, from their headers alone. An image of such a texture is kept as
// BC1 instead of BC3: the colour half of BC3's blocks, so half the video memory for the same texels.
#pragma once
#include <cstdint>
#include <cstring>

namespace astc_opaque {

enum Kind { kAlpha = 0, kOpaque = 1, kEmpty = 2 };

inline bool cem_alpha(uint32_t cem) { return cem == 4 || cem == 5 || cem == 10 || cem == 12 || cem == 13; }

inline Kind block(const uint8_t* b) {
    uint64_t lo, hi;
    std::memcpy(&lo, b, 8);
    std::memcpy(&hi, b + 8, 8);
    if (!lo && !hi) return kEmpty;
    const uint32_t mode = (uint32_t)(lo & 0x7FF);
    if ((mode & 0x1FF) == 0x1FC) {
        if (mode & 0x200) return kAlpha;
        return b[15] == 0xFF ? kOpaque : kAlpha;
    }
    const uint32_t parts = (uint32_t)(lo >> 11 & 3) + 1;
    uint32_t cem;
    if (parts == 1) {
        cem = (uint32_t)(lo >> 13 & 0xF);
    } else {
        const uint32_t enc = (uint32_t)(lo >> 23 & 0x3F);
        if (enc & 3) return kAlpha;
        cem = enc >> 2 & 0xF;
    }
    return cem_alpha(cem) ? kAlpha : kOpaque;
}

struct Scan {
    uint64_t opaque = 0, alpha = 0, empty = 0;
};

inline Scan scan(const uint8_t* p, uint64_t n, bool stop_at_alpha = true) {
    Scan s;
    for (uint64_t i = 0; i + 16 <= n; i += 16) {
        const Kind k = block(p + i);
        if (k == kOpaque) s.opaque++;
        else if (k == kEmpty) s.empty++;
        else if (s.alpha++, stop_at_alpha) break;
    }
    return s;
}

inline bool whole_opaque(const Scan& s) { return !s.alpha && s.opaque && s.opaque >= s.empty; }

}  // namespace astc_opaque
