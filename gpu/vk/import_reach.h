// (longshot #431) How far back a scan over the imported pools, by their start, has to look: the live imports' largest span.
//
// vk_memory.cpp keeps its imports in a map by start (lo). A scan for the imports over [a, b) walks down from the last one
// starting below b and stops where none further down can reach a. It used to stop once an import ended more than 1 GiB below a,
// on the belief that no import is larger than 1 GiB. But pool_lookup's merge escape leaves an import larger than that over the
// smaller ones inside it (a binding or, with RX_VK_IMPORT_CHUNK_MB=0, a game pool above 1 GiB), and the device allows imports up
// to max_import_size: a scan stopped at a small import inside a large one, missed the large one, and the same bytes were
// imported twice. Now each scan stops only where nothing can reach: below the start of an import, every import further down
// starts lower and spans less than bound(), so one starting at s with a - s >= bound() ends before a.
#pragma once
#include <bit>
#include <cstdint>

namespace import_reach {

struct Reach {
    uint32_t count[65] = {};   // (the imports counted, by class: std::bit_width(span), 0 for an empty one)
    unsigned top = 0;          // (the highest class with one counted; 0: none)

    void add(uint64_t span) {
        const unsigned c = (unsigned)std::bit_width(span);
        count[c]++;
        if (c > top) top = c;
    }
    void remove(uint64_t span) {
        const unsigned c = (unsigned)std::bit_width(span);
        if (!count[c]) return;
        count[c]--;
        while (top && !count[top]) top--;
    }
    uint64_t bound() const { return top >= 64 ? UINT64_MAX : (uint64_t)1 << top; }
};

/// Whether no counted import starting at or below start reaches past a (start + span <= a for every span counted)
inline bool beyond(const Reach& r, uint64_t start, uint64_t a) {
    return a >= start && a - start >= r.bound();
}

} // namespace import_reach
