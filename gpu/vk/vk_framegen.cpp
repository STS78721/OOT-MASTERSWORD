// Frame interpolation (RX_VK_FRAMEGEN=1; off by default): frames made between the game's, from its last two, shown in
// between by a display thread, so a game at 60 fps is shown at 120 (RX_VK_FRAMEGEN_MULT=3: 180). The WebGPU
// renderer's (gpu/render/rx_framegen.cpp, whose comments explain it), on Vulkan:
//   1. each frame is copied (it may be drawn over next frame) and reduced to quarter-size luma;
//   2. block matching: a vector per 16x16 pixels within +-32 pixels (RX_VK_FRAMEGEN_RADIUS quarter texels, default
//      8), then a 3x3 median of them;
//   3. each in-between frame at time t blends the last frame moved back along t and the new one forward along 1 - t.
// Recorded into the present's own recording, after the game's work; the display thread shows the made frames and
// then the game's, evenly over the game's recent frame time, about one frame behind the game. Loading hitches
// (frames over 100 ms apart) are not interpolated.
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#include "vk_dlss.h"
#include "vk_framegen.h"
#include "vk_fsr3_inputs.h"
#include "vk_fsr3fi.h"
#include "vk_menu.h"
#include "vk_reflex.h"
#include "vk_texture.h"

#include "shaders/fg_copy_comp_spv.h"
#include "shaders/fg_flow_comp_spv.h"
#include "shaders/fg_luma_comp_spv.h"
#include "shaders/fg_median_comp_spv.h"
#include "shaders/fg_scale_comp_spv.h"
#include "shaders/fg_synth_comp_spv.h"
#include "vk_work.h"   // (after the others: counts the commands that do GPU work, for full_barrier)

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION   // (older Windows SDKs lack it: Windows 10 1803 and later take it)
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace mkwvk {
extern std::atomic<uint32_t> present_kind;   // (vk_replay.cpp: the frame log's)
namespace {

constexpr int kMaxMult = 8;   // (up to 8x: players asked; the made frames are allocated for the multiplier chosen only)

struct Img {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkImageView view = VK_NULL_HANDLE;
};
struct OutSet {
    Img frame[kMaxMult];
    int made = 0;   // (frame[0..made) are made frames; frame[made] is the game's)
    bool busy = false;
    uint64_t reflex_id = 0;   // (NVIDIA Reflex: the game frame it was made at, 0 none: its game frame is presented with it)
};
struct Job {
    OutSet* set;
    int64_t at;
    uint64_t value;   // (the recording that makes its frames)
};
struct Pipe {
    VkDescriptorSetLayout set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
};

struct State {
    bool init = false, failed = false;
    uint32_t w = 0, h = 0;
    Pipe copy, luma, flow, median, synth;
    VkSampler nearest = VK_NULL_HANDLE, linear = VK_NULL_HANDLE;
    Img frames[2], lumas[2], flows[2];
    OutSet sets[3];   // (one shown, one waiting its place on the line, one being made: the pacer never holds the game back)
    int cur = 0, next_set = 0;
    bool have_prev = false;
    int64_t last_at = 0;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<Job> jobs;
    bool thread_started = false;
    OutSet* recorded = nullptr;   // (the set the last framegen_record filled, for framegen_queue)
    int64_t recorded_at = 0;
    int mult = 0;   // (the multiplier the sets' frames were made for)
} fg;

// Used only by present_image, whose lock serializes direct and interpolated presents.
struct ScaleState {
    bool init = false, failed = false;
    Pipe pipe;
    VkSampler linear = VK_NULL_HANDLE;
    Img cropped, scaled;
    uint32_t cropped_w = 0, cropped_h = 0, scaled_w = 0, scaled_h = 0;
} scale;

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int multiplier() {
    // The menu step for this engine (DLSS up to 6x, FSR 2x, MasterSword 8x). Not past what that engine allows.
    const int hi = std::min(kMaxMult, std::max(opts_fg_mult_max(), 1));
    return std::clamp(opts.framegen, 1, hi);
}


/// A compute pipeline over bindings of these types (push descriptors), push constants of push bytes
bool make_pipe(Pipe& p, const uint32_t* code, size_t bytes, std::initializer_list<VkDescriptorType> types, uint32_t push,
               const VkSpecializationInfo* spec = nullptr) {
    std::vector<VkDescriptorSetLayoutBinding> b;
    for (VkDescriptorType t : types) b.push_back({(uint32_t)b.size(), t, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    li.bindingCount = (uint32_t)b.size();
    li.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(dev.device, &li, nullptr, &p.set) != VK_SUCCESS) return false;
    const VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, std::max(push, 4u)};
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pi.setLayoutCount = 1;
    pi.pSetLayouts = &p.set;
    pi.pushConstantRangeCount = push ? 1 : 0;
    pi.pPushConstantRanges = &pr;
    if (vkCreatePipelineLayout(dev.device, &pi, nullptr, &p.layout) != VK_SUCCESS) return false;
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = bytes;
    mi.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev.device, &mi, nullptr, &m) != VK_SUCCESS) return false;
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ci.stage.module = m;
    ci.stage.pName = "main";
    ci.stage.pSpecializationInfo = spec;
    ci.layout = p.layout;
    const VkResult r = vkCreateComputePipelines(dev.device, VK_NULL_HANDLE, 1, &ci, nullptr, &p.pipe);
    vkDestroyShaderModule(dev.device, m, nullptr);
    return r == VK_SUCCESS;
}

bool init() {
    if (fg.init) return !fg.failed;
    fg.init = true;
    fg.failed = true;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;
    const VkDescriptorType S = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, I = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // Apex FG (Fast): cheaper flow search (radius 4) unless RX_VK_FRAMEGEN_RADIUS overrides.
    const int radius_default = opts.fg_backend == 6 ? 4 : 8;
    const int radius = getenv("RX_VK_FRAMEGEN_RADIUS") ? std::clamp(atoi(getenv("RX_VK_FRAMEGEN_RADIUS")), 1, 16)
                                                       : radius_default;
    const VkSpecializationMapEntry me{0, 0, 4};
    const VkSpecializationInfo spec{1, &me, 4, &radius};
    if (!make_pipe(fg.copy, FG_COPY_COMP_SPV, sizeof FG_COPY_COMP_SPV, {S, I}, 16) ||
        !make_pipe(fg.luma, FG_LUMA_COMP_SPV, sizeof FG_LUMA_COMP_SPV, {S, I}, 0) ||
        !make_pipe(fg.flow, FG_FLOW_COMP_SPV, sizeof FG_FLOW_COMP_SPV, {S, S, I}, 0, &spec) ||
        !make_pipe(fg.median, FG_MEDIAN_COMP_SPV, sizeof FG_MEDIAN_COMP_SPV, {S, I}, 0) ||
        !make_pipe(fg.synth, FG_SYNTH_COMP_SPV, sizeof FG_SYNTH_COMP_SPV, {S, S, S, I}, 4))
        return false;
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.25f;
    vkCreateSampler(dev.device, &si, nullptr, &fg.nearest);
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(dev.device, &si, nullptr, &fg.linear);
    fg.failed = false;
    const char* eng = opts.fg_backend == 1   ? "MasterSword/Hookshot"
                    : opts.fg_backend == 2   ? "Nodix SMFG (stand-in)"
                    : opts.fg_backend == 3   ? (dlss_fg_runs() ? "DLSS MFG (NGX)" : "DLSS MFG (stand-in)")
                    : opts.fg_backend == 4   ? (fsr3fi::capable() ? "FSR 3.1 FG" : "FSR 3.1 FG (stand-in)")
                    : opts.fg_backend == 5   ? "XeSS MFG (stand-in)"
                    : opts.fg_backend == 6   ? "Apex FG (Fast) (stand-in)"
                                             : "off";
    logf("vk: frame interpolation ready: %s %dx the game's frames (made from its last two, shown evenly)", eng,
         multiplier());
    return true;
}

bool init_scale() {
    if (scale.init) return !scale.failed;
    scale.init = scale.failed = true;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;
    const VkDescriptorType S = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, I = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    if (make_pipe(scale.pipe, FG_SCALE_COMP_SPV, sizeof FG_SCALE_COMP_SPV, {S, I}, 8)) {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod = 0.25f;
        si.magFilter = si.minFilter = VK_FILTER_LINEAR;
        if (vkCreateSampler(dev.device, &si, nullptr, &scale.linear) == VK_SUCCESS) {
            scale.failed = false;
            return true;
        }
    }
    if (scale.pipe.pipe) vkDestroyPipeline(dev.device, scale.pipe.pipe, nullptr);
    if (scale.pipe.layout) vkDestroyPipelineLayout(dev.device, scale.pipe.layout, nullptr);
    if (scale.pipe.set) vkDestroyDescriptorSetLayout(dev.device, scale.pipe.set, nullptr);
    scale.pipe = {};
    return false;
}

bool make_img(Img& i, uint32_t w, uint32_t h, VkFormat f, VkImageUsageFlags usage) {
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = f;
    ci.extent = {w, h, 1};
    ci.mipLevels = ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.usage = usage;
    if (!create_image(ci, i.image, i.alloc)) return false;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = i.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = f;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return vkCreateImageView(dev.device, &vi, nullptr, &i.view) == VK_SUCCESS;
}
void drop_img(Img& i) {
    if (i.view) vkDestroyImageView(dev.device, i.view, nullptr);
    if (i.image) destroy_image(i.image, i.alloc);
    i = {};
}

void to_general(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &d);
}

bool ensure_size(VkCommandBuffer cmd, uint32_t w, uint32_t h) {
    const int want = multiplier();
    if (fg.w == w && fg.h == h && fg.mult == want) return true;
    {   // (the display thread is done with the old sets first)
        std::unique_lock lk(fg.mu);
        fg.cv.wait(lk, [] { return fg.jobs.empty() && !fg.sets[0].busy && !fg.sets[1].busy && !fg.sets[2].busy; });
    }
    {
        std::lock_guard qk(dev.present_mu);   // (every queue idle: the present queue's too, device_wait_idle_locked)
        device_wait_idle_locked();
    }
    for (int i = 0; i < 2; i++) {
        drop_img(fg.frames[i]), drop_img(fg.lumas[i]), drop_img(fg.flows[i]);
    }
    for (OutSet& set : fg.sets)
        for (Img& t : set.frame) drop_img(t);
    fg.w = 0, fg.h = 0, fg.mult = 0;
    const uint32_t qw = (w + 3) / 4, qh = (h + 3) / 4, cw = (qw + 3) / 4, ch = (qh + 3) / 4;
    const VkImageUsageFlags rw = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    bool ok = true;
    for (int i = 0; i < 2; i++) {
        ok = ok && make_img(fg.frames[i], w, h, VK_FORMAT_R8G8B8A8_UNORM, rw | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        ok = ok && make_img(fg.lumas[i], qw, qh, VK_FORMAT_R16_SFLOAT, rw);
        ok = ok && make_img(fg.flows[i], cw, ch, VK_FORMAT_R16G16B16A16_SFLOAT, rw);
    }
    for (OutSet& set : fg.sets)
        for (int k = 0; k < want; k++)
            ok = ok && make_img(set.frame[k], w, h, VK_FORMAT_R8G8B8A8_UNORM,
                                rw | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!ok) {
        // Do not permanently kill FG (fg.failed): a higher mult can OOM while 2x–4x still fit — drop and retry next
        // present / after the user steps the multiplier back down.
        for (int i = 0; i < 2; i++) {
            drop_img(fg.frames[i]), drop_img(fg.lumas[i]), drop_img(fg.flows[i]);
        }
        for (OutSet& set : fg.sets)
            for (Img& t : set.frame) drop_img(t);
        logf("vk: frame interpolation: %dx images at %ux%u were not made (VRAM?); will retry", want, w, h);
        fg.have_prev = false;
        return false;
    }
    fg.w = w, fg.h = h, fg.mult = want;
    for (int i = 0; i < 2; i++) {
        to_general(cmd, fg.frames[i].image), to_general(cmd, fg.lumas[i].image), to_general(cmd, fg.flows[i].image);
    }
    for (OutSet& set : fg.sets)
        for (Img& t : set.frame)
            if (t.image) to_general(cmd, t.image);   // (only the multiplier's frames are made: a barrier on no image crashed the driver)
    fg.have_prev = false;
    {
        const char* eng = opts.fg_backend == 1 ? "MasterSword/Hookshot"
                        : opts.fg_backend == 3 ? (dlss_fg_runs() ? "DLSS MFG (NGX)" : "DLSS MFG (stand-in)")
                        : opts.fg_backend == 4 ? (fsr3fi::capable() ? "FSR 3.1 FG" : "FSR 3.1 FG (stand-in)")
                        : opts.fg_backend == 2 ? "Nodix SMFG (stand-in)"
                        : opts.fg_backend == 5 ? "XeSS MFG (stand-in)"
                        : opts.fg_backend == 6 ? "Apex FG (Fast) (stand-in)"
                        : opts.fg_backend > 0  ? "vendor FG (stand-in)"
                                               : "off";
        logf("vk: frame interpolation images: %s %dx at %ux%u", eng, want, w, h);
    }
    return true;
}

/// One pass: pipe over w x h in 8x8 groups, the bindings in order (a sampler when one is given: a sampled image)
void pass(VkCommandBuffer cmd, const Pipe& p, std::initializer_list<std::pair<VkImageView, VkSampler>> bindings,
          uint32_t w, uint32_t h, const void* push = nullptr, uint32_t push_bytes = 0) {
    VkDescriptorImageInfo ii[8];
    VkWriteDescriptorSet ws[8];
    uint32_t n = 0;
    for (const auto& [view, sampler] : bindings) {
        ii[n] = {sampler, view, VK_IMAGE_LAYOUT_GENERAL};
        ws[n] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        ws[n].dstBinding = n;
        ws[n].descriptorCount = 1;
        ws[n].descriptorType = sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        ws[n].pImageInfo = &ii[n];
        n++;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipe);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout, 0, n, ws);
    if (push_bytes) vkCmdPushConstants(cmd, p.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes, push);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);
    full_barrier(cmd);
}

// (hookshot) The display thread's wait for a frame's time, on a high-resolution timer. sleep_for rests on the system timer,
// whose 1 ms (timeBeginPeriod) Windows 11 does not keep for a process whose window it takes as unseen: with the presents
// no longer blocking (a present queue of their own) the thread slept before most frames and woke up to 20 ms after their time.
void wait_until_near(int64_t ns) {
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER d;
    d.QuadPart = -std::max<int64_t>(1, ns / 100);   // (relative, 100 ns units)
    if (timer && SetWaitableTimerEx(timer, &d, 0, nullptr, nullptr, nullptr, 0)) {
        WaitForSingleObject(timer, INFINITE);
        return;
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

static bool present_ahead_wanted(bool env_on, bool pace, int shown, uint32_t spare) {
    // (each frame of the set holds a swapchain image from its copy to its present: as many spare images as frames. 2x was
    // the only one at first; at 3x and above the made frames' copies queued behind the game's next frame the same way, so their
    // presents reached the screen unevenly. vk_main.cpp asks for one spare image per frame of the multiplier, up to 8x)
    return env_on && pace && shown >= 2 && shown <= kMaxMult && spare >= (uint32_t)shown;
}

void display_main() {
    SetThreadDescription(GetCurrentThread(), L"mkw_vk display");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    int64_t interval = 16666667, last_at = 0;
    struct { int on_time, late, skipped; int64_t since; } stat{};
    int64_t next_show = 0, shown_at = 0;
    int hist[6] = {};
    for (;;) {
        Job j;
        {
            std::unique_lock lk(fg.mu);
            fg.cv.wait(lk, [] { return !fg.jobs.empty(); });
            j = fg.jobs.front();
            fg.jobs.pop_front();
        }
        if (last_at) {
            const int64_t d = j.at - last_at;
            if (d > 0 && d < 100000000) interval = (interval * 7 + std::clamp<int64_t>(d, 4000000, 50000000)) / 8;
        }
        last_at = j.at;
        if (!stat.since) stat.since = j.at;
        if (j.at - stat.since > 5000000000LL) {
            logf("vk: frame interpolation: %d frames shown on time, %d late (over 2 ms), %d skipped; shown apart: <7 ms %d, 7-9 %d, "
                 "9-12 %d, 12-17 %d, 17-25 %d, over 25 %d", stat.on_time, stat.late, stat.skipped, hist[0], hist[1], hist[2],
                 hist[3], hist[4], hist[5]);
            memset(hist, 0, sizeof hist);
            stat = {0, 0, 0, j.at};
        }
        const bool pace = opts.pace != 0;
        const bool ms_pace = opts.ms_pacer != 0;
        const int shown = j.set->made + 1;
        const int64_t step = std::max<int64_t>(1, interval / shown);
        const int64_t skip_slop = shown >= 4 ? step / 2 : step;
        static const bool ahead_env = !getenv("RX_VK_PRESENT_AHEAD") || atoi(getenv("RX_VK_PRESENT_AHEAD")) != 0;
        const bool ahead = present_ahead_wanted(ahead_env, pace, shown, dev.swap_spare.load(std::memory_order_relaxed));
        static int said_pacer = -1;
        if (ms_pace != (said_pacer == 1)) {
            said_pacer = ms_pace ? 1 : 0;
            logf("vk: Mastersword Pacer %s", ms_pace ? "on" : "off");
        }
        PendingPresent pending[kMaxMult];
        if (ahead) {
            static bool said;
            if (!said) said = true, logf("vk: present ahead: a set's copies submitted at once, its presents on time (RX_VK_PRESENT_AHEAD=0: off)");
            for (int k = 0; k < shown; k++) {
                present_kind.store(k < j.set->made ? 1 : 0);
                reflex_present_frame = (k == j.set->made) ? j.set->reflex_id : 0;
                present_defer_into = &pending[k];
                present_image(j.set->frame[k].image, fg.w, fg.h, nullptr, j.set->frame[k].view);
                present_defer_into = nullptr;
            }
            if (pending[0].used) vkWaitForFences(dev.device, 1, &pending[0].done, VK_TRUE, 100000000ull);   // (100 ms at most)
        }
        for (int k = 0; k <= j.set->made; k++) {
            const int64_t sched = pace ? next_show : 0;
            const int64_t now = now_ns();
            if (!ms_pace && !ahead) {
                if (pace && k < j.set->made && shown >= 4 && now >= j.at + interval) {
                    stat.skipped += (j.set->made - k);
                    k = j.set->made - 1;
                    next_show = now;
                    continue;
                }
                if (pace && k < j.set->made && sched > 0 && now >= sched + skip_slop) {
                    next_show = sched + step;
                    stat.skipped++;
                    continue;
                }
            }
            /* Mastersword Pacer: an intermediate more than one step late is dropped and the beat moves on.
               The game frame, last in the set, is always shown. One late frame does not discard the rest. */
            if (ms_pace && !ahead && pace && k < j.set->made && sched > 0 && now >= sched + step) {
                next_show = sched + step;
                stat.skipped++;
                continue;
            }
            int64_t due = sched > 0 ? sched : now;
            if (!ms_pace && due < now) due = now;
            if (due > j.at + interval + step) due = j.at + interval + step;
            const bool slipped = ms_pace && sched > 0 && now > due;
            if (slipped) due = now;   /* late: show now and aim the next step from here, no catch-up short frame */
            if (due - now > 1000000) wait_until_near(due - now - 1000000);
            while (now_ns() < due) SwitchToThread();
            const int64_t t = now_ns();
            if (ahead) {
                if (!present_commit(pending[k])) {
                    next_show = t + step;
                    continue;
                }
            } else {
                present_kind.store(k < j.set->made ? 1 : 0);
                reflex_present_frame = (k == j.set->made) ? j.set->reflex_id : 0;
                present_image(j.set->frame[k].image, fg.w, fg.h, nullptr, j.set->frame[k].view);
            }
            menu_note_frame(k == j.set->made);
            if (shown_at) {
                const int64_t d = t - shown_at;
                hist[d < 7000000 ? 0 : d < 9000000 ? 1 : d < 12000000 ? 2 : d < 17000000 ? 3 : d < 25000000 ? 4 : 5]++;
            }
            shown_at = t;
            next_show = ms_pace && pace ? (slipped ? t + step : due + step) : t + step;
            (t - due > 2000000 ? stat.late : stat.on_time)++;
        }
        std::lock_guard lk(fg.mu);
        j.set->busy = false;
        fg.cv.notify_all();
    }
}

} // namespace

// The Insert menu's Frame generation is off at 0. While it was ignored, every present still copied the frame and
// reduced it to luma (framegen_record), then a second thread showed it. Open areas paid for that on a full GPU and
// gained no made frames. 2x and up is the interpolation; RX_VK_FRAMEGEN=0 keeps it off entirely.
bool framegen_wanted(int menu_value, int allow) {
    return allow && menu_value >= 2;
}

bool framegen_on() {
    static const int allow = !getenv("RX_VK_FRAMEGEN") || atoi(getenv("RX_VK_FRAMEGEN")) != 0;
    // Engine Off → no FG. MasterSword (1) = Hookshot native. DLSS MFG / SMFG / FSR / XeSS = same interpolator
    // as a stand-in until Streamline/vendor FG evaluates (do not run Hookshot when engine is Off).
    return (opts_fg_hookshot() || opts_fg_vendor()) && framegen_wanted(opts.framegen, allow) && !dev.offscreen &&
           !fg.failed;
}

VkImage framegen_scale(VkCommandBuffer cmd, VkImageView src, VkImage image, uint32_t sw, uint32_t sh, const int crop[4], uint32_t dw, uint32_t dh) {
    const int scale_mode = opts_spatial_upscale();
    if (!scale_mode && !opts.sharpness) return VK_NULL_HANDLE;
    if (!src || !image || !sw || !sh || !dw || !dh) return VK_NULL_HANDLE;
    if (crop && crop[2] > 0 && crop[3] > 0 &&
        (crop[0] < 0 || crop[1] < 0 || (uint32_t)crop[0] >= sw || (uint32_t)crop[1] >= sh)) return VK_NULL_HANDLE;
    if (!init_scale()) return VK_NULL_HANDLE;
    // (the game draws into part of its window textures, 2048 x 1152 of 2560 x 1440 here: the filter was given the whole texture, and the
    // picture came out in the top left corner of the window with black around it. The crop first, then the filter)
    if (crop && crop[2] > 0 && crop[3] > 0 && (crop[0] || crop[1] || (uint32_t)crop[2] < sw || (uint32_t)crop[3] < sh)) {
        const uint32_t cw = std::min((uint32_t)crop[2], sw - (uint32_t)crop[0]), ch = std::min((uint32_t)crop[3], sh - (uint32_t)crop[1]);
        if (scale.cropped_w != cw || scale.cropped_h != ch) {
            {
                std::lock_guard qk(dev.present_mu);
                device_wait_idle_locked();
            }
            drop_img(scale.cropped);
            scale.cropped_w = scale.cropped_h = 0;
            if (!make_img(scale.cropped, cw, ch, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
                drop_img(scale.cropped);
                return VK_NULL_HANDLE;
            }
            scale.cropped_w = cw, scale.cropped_h = ch;
            to_general(cmd, scale.cropped.image);
        }
        VkImageBlit b{};
        b.srcSubresource = b.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        b.srcOffsets[0] = {crop[0], crop[1], 0};
        b.srcOffsets[1] = {crop[0] + (int)cw, crop[1] + (int)ch, 1};
        b.dstOffsets[1] = {(int)cw, (int)ch, 1};
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_GENERAL, scale.cropped.image, VK_IMAGE_LAYOUT_GENERAL, 1, &b, VK_FILTER_NEAREST);
        full_barrier(cmd);
        src = scale.cropped.view;
    }
    if (scale.scaled_w != dw || scale.scaled_h != dh) {
        {
            std::lock_guard qk(dev.present_mu);   // (the queue is idle only while nothing else submits to it)
            device_wait_idle_locked();
        }
        drop_img(scale.scaled);
        scale.scaled_w = scale.scaled_h = 0;
        // (sampled too: with HDR on, framegen_hdr reads the scaled picture through a sampler. Without the usage that read is
        // undefined, and an upscaler or a Sharpness above 0 (every preset) gave a black, grey or purple HDR picture where 0 did not)
        if (!make_img(scale.scaled, dw, dh, VK_FORMAT_R8G8B8A8_UNORM,
                      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) {
            drop_img(scale.scaled);
            return VK_NULL_HANDLE;
        }
        scale.scaled_w = dw, scale.scaled_h = dh;
        to_general(cmd, scale.scaled.image);
    }
    const struct { int mode; float sharp; } pc{scale_mode, opts.sharpness / 100.0f};
    pass(cmd, scale.pipe, {{src, scale.linear}, {scale.scaled.view, VK_NULL_HANDLE}}, dw, dh, &pc, sizeof pc);
    static const bool report = getenv("RX_VK_UPSCALE_LOG") != nullptr;
    static bool reported = false;
    if (report && !reported) {
        reported = true;
        logf("vk: upscale dispatched: mode %d (menu %d), sharpness %d%% (of RCAS's strongest, neighbours one source pixel apart), "
             "source %ux%u, crop request %d,%d %dx%d, destination %ux%u",
             scale_mode, opts.upscale, opts.sharpness, sw, sh, crop ? crop[0] : 0, crop ? crop[1] : 0,
             crop ? crop[2] : (int)sw, crop ? crop[3] : (int)sh, dw, dh);
    }
    return scale.scaled.image;
}

VkImage framegen_made_image(uint32_t* w, uint32_t* h) {
    if (!fg.recorded || !fg.recorded->made) return VK_NULL_HANDLE;
    *w = fg.w, *h = fg.h;
    return fg.recorded->frame[0].image;
}

namespace {
int fsr3fi_frame(VkCommandBuffer cmd, OutSet& set, int cur, uint32_t uw, uint32_t uh, uint64_t sequence) {
    static uint64_t asked = 0, made = 0, no_inputs = 0;
    if (++asked % 600 == 0)
        logf("vk: fsr3fi: %llu of the last 600 made frames by FSR 3.1, %llu without its inputs (the renderer's own made them)",
             (unsigned long long)made, (unsigned long long)no_inputs),
            made = no_inputs = 0;
    fsr3_inputs::Frame f;
    if (!fsr3_inputs::take(f)) return no_inputs++, 0;
    fsr3fi::Inputs in;
    in.color = fg.frames[cur].view;
    in.color_image = fg.frames[cur].image;
    in.color_format = VK_FORMAT_R8G8B8A8_UNORM;
    in.width = uw, in.height = uh;
    in.depth = f.depth, in.motion = f.motion;
    in.render_width = f.width, in.render_height = f.height;
    in.jitter[0] = f.jitter[0], in.jitter[1] = f.jitter[1];
    in.camera = f.camera;
    in.frame_time_ms = f.frame_ms;
    in.reset = f.reset;
    in.sequence = sequence;
    if (!fsr3fi::generate(cmd, in) || !fsr3fi::made_image()) return 0;
    const VkImageCopy c{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},
                        {uw, uh, 1}};
    vkCmdCopyImage(cmd, fsr3fi::made_image(), VK_IMAGE_LAYOUT_GENERAL, set.frame[0].image, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
    made++;
    static bool told = false;
    if (!told) {
        told = true;
        logf("vk: fsr3fi: AMD FSR 3.1 makes the frame between each two of the game's (%ux%u, its depth and motion %ux%u)", uw,
             uh, f.width, f.height);
    }
    return 1;
}
} // namespace

bool framegen_record(VkCommandBuffer cmd, VkImageView src, uint32_t sw, uint32_t sh, const int crop[4]) {
    int x0 = 0, y0 = 0, w = (int)sw, h = (int)sh;
    if (crop && crop[2] > 0 && crop[3] > 0) x0 = crop[0], y0 = crop[1], w = crop[2], h = crop[3];
    w = std::min(w, (int)sw - x0), h = std::min(h, (int)sh - y0);
    if (!src || w <= 0 || h <= 0 || !init() || !ensure_size(cmd, (uint32_t)w, (uint32_t)h)) return false;
    if (!fg.thread_started) {
        fg.thread_started = true;
        std::thread(display_main).detach();
    }
    const int64_t now = now_ns();
    const bool gap = fg.last_at && now - fg.last_at > 100000000;   // (a loading hitch: nothing to blend)
    fg.last_at = now;
    OutSet* set;
    {
        std::unique_lock lk(fg.mu);
        // Never block the game on the display thread. Waiting here at 4x/6x locked game FPS to ~50
        // (one OutSet busy for ~20 ms while four 4K presents finished). Skip FG this frame instead.
        int idx = fg.next_set;
        for (int i = 0; i < 3; i++) {
            const int j = (fg.next_set + i) % 3;
            if (!fg.sets[j].busy) { idx = j; break; }
        }
        if (fg.sets[idx].busy) return false;
        set = &fg.sets[idx];
        set->busy = true;
        fg.next_set = (idx + 1) % 3;
    }
    const int cur = fg.cur, prev = cur ^ 1;
    const int m = multiplier();
    const bool blend = fg.have_prev && !gap;
    set->reflex_id = reflex_frame();   // (NVIDIA Reflex: the game frame this set shows, vk_reflex.cpp)
    static uint64_t sequence = 0;
    sequence++;
    const uint32_t uw = (uint32_t)w, uh = (uint32_t)h;
    const uint32_t qw = (uw + 3) / 4, qh = (uh + 3) / 4, cw = (qw + 3) / 4, ch = (qh + 3) / 4;
    full_barrier(cmd);
    const int32_t pc[4] = {x0, y0, w, h};
    pass(cmd, fg.copy, {{src, fg.nearest}, {fg.frames[cur].view, VK_NULL_HANDLE}}, uw, uh, pc, sizeof pc);
    pass(cmd, fg.luma, {{fg.frames[cur].view, fg.nearest}, {fg.lumas[cur].view, VK_NULL_HANDLE}}, qw, qh);
    set->made = 0;
    // (NVIDIA DLSS Frame Generation through NGX where it runs and DLSS MFG is selected, vk_dlss.cpp)
    if (m > 1) {
        VkImage outs[kMaxMult];
        VkImageView views[kMaxMult];
        for (int k = 0; k < m - 1; k++) outs[k] = set->frame[k].image, views[k] = set->frame[k].view;
        set->made = dlss_fg_frames(cmd, fg.frames[cur].image, fg.frames[cur].view, uw, uh, m - 1, outs, views, blend, sequence);
    }
    // (At 2x, AMD FSR 3.1's frame interpolation where FSR 3.1 FG is selected)
    if (set->made == 0 && blend && m == 2 && fsr3fi::frames_wanted()) set->made = fsr3fi_frame(cmd, *set, cur, uw, uh, sequence);
    if (set->made == 0 && blend && m > 1) {
        pass(cmd, fg.flow, {{fg.lumas[prev].view, fg.nearest}, {fg.lumas[cur].view, fg.nearest},
                            {fg.flows[0].view, VK_NULL_HANDLE}}, cw, ch);
        pass(cmd, fg.median, {{fg.flows[0].view, fg.nearest}, {fg.flows[1].view, VK_NULL_HANDLE}}, cw, ch);
        for (int k = 1; k < m; k++) {
            const float t = (float)k / (float)m;
            pass(cmd, fg.synth, {{fg.frames[prev].view, fg.linear}, {fg.frames[cur].view, fg.linear},
                                 {fg.flows[1].view, fg.nearest}, {set->frame[k - 1].view, VK_NULL_HANDLE}},
                 uw, uh, &t, 4);
        }
        set->made = m - 1;
    }
    const VkImageCopy c{{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},
                        {uw, uh, 1}};
    vkCmdCopyImage(cmd, fg.frames[cur].image, VK_IMAGE_LAYOUT_GENERAL, set->frame[set->made].image,
                   VK_IMAGE_LAYOUT_GENERAL, 1, &c);
    full_barrier(cmd);
    fg.have_prev = true;
    fg.cur = prev;
    fg.recorded = set;
    fg.recorded_at = now;
    return true;
}

void framegen_queue(uint64_t value) {
    if (!fg.recorded) return;
    std::lock_guard lk(fg.mu);
    // Keep at most 1 waiting job ahead of the one currently showing so the pacer's 3-set ring works smoothly
    // while preventing multi-set queue buildup at 6x/8x.
    while (fg.jobs.size() >= 2) {
        Job old = fg.jobs.front();
        fg.jobs.pop_front();
        old.set->busy = false;
    }
    fg.jobs.push_back({fg.recorded, fg.recorded_at, value});
    fg.recorded = nullptr;
    fg.cv.notify_all();
}

} // namespace mkwvk
