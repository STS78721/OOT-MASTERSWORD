// Present-path SMAA 1x (vk_smaa.cpp): Subpixel Morphological Anti-Aliasing after the game / colour grade and
// before the window scale. Official Area/Search lookups (iryoku/smaa Medium). Menu: opts.smaa.
#pragma once

#include "vk_internal.h"

namespace mkwvk {

bool smaa_active();

/// Runs SMAA on src's crop into an RGBA16F image. Returns that image (GENERAL) and fills *out_view / *out_w / *out_h;
/// VK_NULL_HANDLE when SMAA is off or the pass could not run.
VkImage smaa_record(VkCommandBuffer cmd, int slot, VkImage src, VkImageView src_view, VkFormat fmt, uint32_t sw,
                    uint32_t sh, const int crop[4], VkImageView* out_view, uint32_t* out_w, uint32_t* out_h);

void smaa_release();

} // namespace mkwvk
