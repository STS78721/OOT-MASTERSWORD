// AMD Anti-Lag 2 via VK_AMD_anti_lag (no external SDK required; no-op on non-AMD or unsupported GPUs).
#pragma once

#include <cstdint>
#include <vector>

#include "vk_internal.h"

namespace mkwvk {

void antilag_device_extensions(std::vector<const char*>& want, const std::vector<VkExtensionProperties>& all);
void antilag_device_features(void**& tail);
void antilag_device_created();
bool antilag_available();
uint64_t antilag_frame();
constexpr uint64_t kAntiLagThisFrame = ~0ull;
extern thread_local uint64_t antilag_present_frame;
void antilag_present();
void antilag_input();

} // namespace mkwvk
