// The render target pool's bound by default (vk_texture.cpp pool_cap): a twelfth of what the driver allows the process, from 256 MB
// to 768 MB. OoT makes its transient targets (a 384 x 384 depth target, 1280 x 720, 640 x 360 and 2560 x 1440 ones) at shifted
// offsets of a scratch heap every frame; a dead one is reusable a frame or two after it is let go, once the GPU is done with it, so
// the pool has to hold two or three frames of them. (longshot #994, #1255)
#pragma once

#include <algorithm>
#include <cstdint>

namespace mkwvk::target_pool {

inline constexpr uint64_t kMinMB = 256, kMaxMB = 768;
// How long an image waits in the pool for the next target of its shape, in presents (RX_VK_POOL_IDLE).
inline constexpr uint64_t kIdlePresents = 8;

/// The default bound in MB for a driver allowance of `allowed` bytes (0: not known, the minimum)
inline uint64_t default_mb(uint64_t allowed) {
    if (!allowed) return kMinMB;
    return std::clamp<uint64_t>((allowed >> 20) / 12, kMinMB, kMaxMB);
}

/// Whether an image put into the pool at present `since` has waited too long at present `now`
inline bool idle_out(uint64_t now, uint64_t since, uint64_t idle) { return now - since > idle; }

}  // namespace mkwvk::target_pool
