// Colour adjustment on the present (vk_color.cpp): brightness, contrast, saturation and gamma of the game's
// picture, after the game and before the window scale / blit. Presets match STS78721 / TOTK / DKB (vivid OLED).
#pragma once

#include "vk_internal.h"

namespace mkwvk {

/// True when any colour slider differs from "unchanged" (default profile).
bool color_grade_active();

/// Records a colour-grade pass of src's crop into an RGBA16F image. Returns that image (GENERAL) and fills
/// *out_view / *out_w / *out_h; VK_NULL_HANDLE when colour is off or the pass could not run.
VkImage color_grade_record(VkCommandBuffer cmd, int slot, VkImage src, VkImageView src_view, VkFormat fmt,
                           uint32_t sw, uint32_t sh, const int crop[4], VkImageView* out_view, uint32_t* out_w,
                           uint32_t* out_h);

void color_grade_release();   // before the device goes away

} // namespace mkwvk
