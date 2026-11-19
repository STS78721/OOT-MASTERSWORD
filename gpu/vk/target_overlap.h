/* Render targets drawn over from another start (vk_texture.cpp's eviction): a frame's transient targets that the game
   makes in a scratch heap whose offsets shift from frame to frame. */
#pragma once

#include <algorithm>
#include <cstdint>

namespace mkwvk::target_overlap {

/// What newer render targets drew over of a render target's storage [lo, hi), last used at frame `last_used`, added up. One
/// counts when it starts elsewhere (the same start is drawn_over's own case), is not inside it (a view of a part of it) and
/// was written after its last use; its part inside [lo, hi) is merged with the others', so an overlap counts once. add()
/// takes them in start order (vk_texture.cpp's by_start): one pass merges them cleanly.
struct ShiftedOver {
    uint64_t lo, hi, last_used;
    uint64_t done = 0, run_lo = 0, run_hi = 0;

    void add(uint64_t olo, uint64_t ohi, uint64_t o_written) {
        if (hi <= lo || ohi <= olo || olo == lo || o_written <= last_used) return;
        if (olo >= lo && ohi <= hi) return;
        const uint64_t a = std::max(lo, olo), b = std::min(hi, ohi);
        if (b <= a) return;
        if (a <= run_hi) {
            run_hi = std::max(run_hi, b);
        } else {
            done += run_hi - run_lo;
            run_lo = a, run_hi = b;
        }
    }

    /// Whether they cover an eighth of it or more: its texels there are gone from that memory on the console too
    bool an_eighth() const { return hi > lo && (done + run_hi - run_lo) * 8 >= hi - lo; }
};

} // namespace mkwvk::target_overlap
