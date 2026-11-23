// AMD Anti-Lag 2 through VK_AMD_anti_lag (no external SDK required; safely gated at runtime).
#include "vk_antilag.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

#include "vk_menu.h"

namespace mkwvk {

namespace {

struct AntiLag {
    bool have_ext = false, ready = false;
    VkPhysicalDeviceAntiLagFeaturesAMD feature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ANTI_LAG_FEATURES_AMD};
    PFN_vkAntiLagUpdateAMD update = nullptr;
    bool active = false;
    bool frame_marked = false;
    uint64_t frame = 0;
    uint64_t since = 0;
    std::mutex mu;
};

AntiLag& al() {
    static AntiLag a;
    return a;
}

bool wanted() {
    static const char* e = getenv("RX_VK_ANTILAG");
    return (e && *e ? atoi(e) : opts.antilag) > 0;
}

void update(bool on, const VkAntiLagPresentationInfoAMD* stage) {
    VkAntiLagDataAMD d{VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD};
    d.mode = on ? VK_ANTI_LAG_MODE_ON_AMD : VK_ANTI_LAG_MODE_OFF_AMD;
    d.maxFPS = 0;
    d.pPresentationInfo = stage;
    al().update(dev.device, &d);
}

bool on() {
    AntiLag& a = al();
    if (!a.ready) return false;
    const bool want = wanted();
    if (want != a.active) {
        if (!want) update(false, nullptr);
        a.active = want, a.frame_marked = false, a.since = a.frame;
        logf("vk: AMD Anti-Lag 2: %s", want ? "on" : "off");
    }
    return want;
}

void stage(VkAntiLagStageAMD s, uint64_t frame) {
    VkAntiLagPresentationInfoAMD p{VK_STRUCTURE_TYPE_ANTI_LAG_PRESENTATION_INFO_AMD};
    p.stage = s, p.frameIndex = frame;
    update(true, &p);
}

} // namespace

void antilag_device_extensions(std::vector<const char*>& want, const std::vector<VkExtensionProperties>& all) {
    static const char* e = getenv("RX_VK_ANTILAG");
    if (e && !strcmp(e, "-")) return;
    bool have = false;
    for (const auto& x : all) have |= !strcmp(x.extensionName, VK_AMD_ANTI_LAG_EXTENSION_NAME);
    if (!have) return;
    bool listed = false;
    for (const char* w : want) listed |= !strcmp(w, VK_AMD_ANTI_LAG_EXTENSION_NAME);
    if (!listed) want.push_back(VK_AMD_ANTI_LAG_EXTENSION_NAME);
    al().have_ext = true;
}

void antilag_device_features(void**& tail) {
    AntiLag& a = al();
    if (!a.have_ext) return;
    *tail = &a.feature, tail = &a.feature.pNext;
}

void antilag_device_created() {
    AntiLag& a = al();
    if (!a.have_ext) return;
    if (!a.feature.antiLag) {
        logf("vk: AMD Anti-Lag 2: the device does not support it: off");
        return;
    }
    a.update = (PFN_vkAntiLagUpdateAMD)vkGetDeviceProcAddr(dev.device, "vkAntiLagUpdateAMD");
    a.ready = a.update != nullptr;
    logf("vk: AMD Anti-Lag 2: %s", a.ready ? "available (VK_AMD_anti_lag)" : "its function is missing: off");
}

bool antilag_available() { return al().ready; }

thread_local uint64_t antilag_present_frame = kAntiLagThisFrame;

uint64_t antilag_frame() {
    AntiLag& a = al();
    std::lock_guard lk(a.mu);
    return on() && a.frame_marked ? a.frame : 0;
}

void antilag_present() {
    AntiLag& a = al();
    std::lock_guard lk(a.mu);
    if (!on()) return;
    const uint64_t f = antilag_present_frame == kAntiLagThisFrame ? (a.frame_marked ? a.frame : 0) : antilag_present_frame;
    if (!f || f <= a.since) return;
    stage(VK_ANTI_LAG_STAGE_PRESENT_AMD, f);
}

void antilag_input() {
    AntiLag& a = al();
    std::lock_guard lk(a.mu);
    if (!on()) return;
    a.frame++;
    stage(VK_ANTI_LAG_STAGE_INPUT_AMD, a.frame);
    a.frame_marked = true;
}

} // namespace mkwvk
