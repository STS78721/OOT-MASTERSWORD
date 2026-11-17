// When the dead transient render targets are looked for between the eviction's passes (vk_texture.cpp texture_frame and
// dead_targets_sweep). (longshot #1255)
#pragma once

#include <algorithm>
#include <cstdint>

namespace mkwvk::dead_targets {

/// The eviction's pass (budget, hold, sweep) runs at every tenth present: texture_frame's own gate
inline constexpr uint64_t kPassEvery = 10;

/// Whether the eviction's pass runs at this present
inline bool pass_due(uint64_t frame_no) { return frame_no % kPassEvery == 0; }

/// Whether the dead targets are looked for at this present, between the passes: every `every` presents (RX_VK_DEAD_EVERY)
inline bool sweep_due(uint64_t frame_no, uint64_t every, bool on) {
    return on && !pass_due(frame_no) && frame_no % std::max<uint64_t>(every, 1) == 0;
}

/// Whether a render target may go as a dead one at present `frame_no`: of one level and layer, unused for `need` presents
/// (RX_VK_DEAD_MIN_UNUSED) and the GPU past its last use.
inline bool candidate(uint64_t frame_no, uint64_t last_frame, uint64_t need, uint64_t last_value, uint64_t completed, bool single) {
    return single && frame_no - last_frame >= need && last_value <= completed;
}

}  // namespace mkwvk::dead_targets
