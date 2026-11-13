// Hybrid motion vectors (vk_mvec.cpp): colour-flow RG16F at internal resolution, optional depth-reproject merge.
// Output convention: RG16F screen-space pixel delta (current → previous). Ready for DLSS SR bind.
#pragma once

#include "vk_internal.h"
#include "vk_texture.h"
#include "vk_state.h"

namespace mkwvk {

/// RX_VK_MVEC=0 off, =1 force on. Default: only when Upscaling=DLSS SR and Streamline is available.
bool mvec_on();

/// Heuristic scene-depth pin from a colour+depth pass (excludes square shadow maps ≤768).
void mvec_pin_depth(Texture* depth, uint32_t level, uint32_t layer, uint32_t w, uint32_t h);

/// Scan guest UBOs of a scene pass for a view-projection 4×4; auto-trust after stable hits.
void mvec_scan_ubos(const NvnState& s, uint64_t prog, uint32_t pass_w, uint32_t pass_h);

/// Optional view-proj (column-major 4×4). Until trusted, reproject weight stays 0 (flow-only).
void mvec_set_viewproj(const float curr_vp[16], bool trusted);

/// Record colour-flow (+ depth merge when ready) into the internal RG16F field. colour_view is sampled GENERAL.
/// skip_if_done: no-op when already recorded this present frame (present-path fallback after TAAU).
bool mvec_record(VkCommandBuffer cmd, VkImageView color_view, uint32_t w, uint32_t h, bool skip_if_done = false);

VkImage mvec_image();
VkImageView mvec_view();
VmaAllocation mvec_alloc();
bool mvec_valid();
uint32_t mvec_width();
uint32_t mvec_height();

/// Pinned scene depth (may be null if not yet found).
VkImage mvec_depth_image();
VkImageView mvec_depth_view();
Texture* mvec_depth_tex();
bool mvec_vp_trusted();
/// Column-major curr/prev view-proj (valid when mvec_vp_trusted()).
const float* mvec_curr_vp();
const float* mvec_prev_vp();

/// RX_VK_MVEC_DEBUG=1: false-colour RGBA8 of the motion field (same size as mvec).
VkImage mvec_debug_image();
VkImageView mvec_debug_view();
bool mvec_debug_active();

/// Probe guest UBO floats for camera matrices when RX_VK_MVEC_MAT=1 (logging / future trust).
void mvec_note_matrix_candidate(const float* m16, uint64_t prog, const char* where);

} // namespace mkwvk
