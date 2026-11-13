// Hybrid motion vectors: luma block-match flow at internal res, optional depth reproject + confidence merge.
#include "vk_mvec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#include "shaders/mv_copy_comp_spv.h"
#include "shaders/mv_debug_comp_spv.h"
#include "shaders/mv_flow_comp_spv.h"
#include "shaders/mv_luma_comp_spv.h"
#include "shaders/mv_median_comp_spv.h"
#include "shaders/mv_merge_comp_spv.h"
#include "shaders/mv_reproject_comp_spv.h"
#include "vk_dlss.h"
#include "vk_menu.h"
#include "vk_work.h"

namespace mkwvk {
namespace {

struct Img {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkImageView view = VK_NULL_HANDLE;
};

struct Pipe {
    VkDescriptorSetLayout set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
};

struct State {
    bool init = false, failed = false;
    uint32_t w = 0, h = 0;
    Pipe copy, luma, flow, median, reproject, merge, debug;
    VkSampler nearest = VK_NULL_HANDLE;
    VkSampler linear = VK_NULL_HANDLE;   // colour downsample into the capped working size
    Img colors[2], lumas[2], flows[2];
    Img mvec, repro, debug_img;
    int cur = 0;
    bool have_prev = false;
    bool valid = false;

    Texture* depth_tex = nullptr;
    VkImage depth_image = VK_NULL_HANDLE;
    uint32_t depth_level = 0, depth_layer = 0;
    uint32_t depth_w = 0, depth_h = 0;
    VkImageView depth_view = VK_NULL_HANDLE;
    uint64_t depth_hits = 0, depth_tries = 0;

    float curr_vp[16]{};
    float prev_vp[16]{};
    bool have_vp = false;
    bool vp_trusted = false;

    // Auto-pin: last accepted UBO slot (stage, index, byte offset) and consecutive hit count.
    int pin_stage = -1, pin_index = -1;
    uint32_t pin_off = 0;
    int pin_hits = 0;
    uint64_t pin_prog = 0;

    uint64_t frames = 0;
    uint64_t recorded_frame = 0;
} mv;

bool env_on() {
    // RX_VK_MVEC=0 force off; =1 force on. Default: only when DLSS SR can actually run — otherwise the
    // 4K block-match flow every frame TDRs the GPU for no benefit (DLSS SR was not loading).
    if (const char* e = getenv("RX_VK_MVEC")) return atoi(e) != 0;
    return false /* Streamline SR retired; temporal owns MVs */;
}
uint32_t max_edge() {
    // Hard cap: 2560+ block-match with DLSS DLAA@4K exhausted VRAM and froze the machine (2026-11-09).
    // 1920 covers Performance (Preset M) at 4K; Quality can share that field via mvecScale.
    static const uint32_t v = getenv("RX_VK_MVEC_MAX") ? (uint32_t)std::max(480, atoi(getenv("RX_VK_MVEC_MAX"))) : 1920u;
    return v;
}
bool env_debug() {
    static const bool v = getenv("RX_VK_MVEC_DEBUG") != nullptr;
    return v;
}
bool env_mat() {
    static const bool v = getenv("RX_VK_MVEC_MAT") != nullptr;
    return v;
}

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

bool make_img(Img& i, uint32_t w, uint32_t h, VkFormat f, VkImageUsageFlags usage, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
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
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    return vkCreateImageView(dev.device, &vi, nullptr, &i.view) == VK_SUCCESS;
}

void drop_img(Img& i) {
    if (i.view) vkDestroyImageView(dev.device, i.view, nullptr);
    if (i.image) destroy_image(i.image, i.alloc);
    i = {};
}

void to_general(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {aspect, 0, 1, 0, 1};
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &d);
}

bool init() {
    if (mv.init) return !mv.failed;
    mv.init = true;
    mv.failed = true;
    if (!env_on()) return false;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;
    const VkDescriptorType S = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, I = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const int radius = getenv("RX_VK_MVEC_RADIUS") ? std::clamp(atoi(getenv("RX_VK_MVEC_RADIUS")), 1, 16) : 4;
    const VkSpecializationMapEntry me{0, 0, 4};
    const VkSpecializationInfo spec{1, &me, 4, &radius};
    if (!make_pipe(mv.copy, MV_COPY_COMP_SPV, sizeof MV_COPY_COMP_SPV, {S, I}, 16) ||
        !make_pipe(mv.luma, MV_LUMA_COMP_SPV, sizeof MV_LUMA_COMP_SPV, {S, I}, 0) ||
        !make_pipe(mv.flow, MV_FLOW_COMP_SPV, sizeof MV_FLOW_COMP_SPV, {S, S, I}, 0, &spec) ||
        !make_pipe(mv.median, MV_MEDIAN_COMP_SPV, sizeof MV_MEDIAN_COMP_SPV, {S, I}, 0) ||
        !make_pipe(mv.reproject, MV_REPROJECT_COMP_SPV, sizeof MV_REPROJECT_COMP_SPV, {S, I}, 144) ||
        !make_pipe(mv.merge, MV_MERGE_COMP_SPV, sizeof MV_MERGE_COMP_SPV, {S, S, I}, 8) ||
        !make_pipe(mv.debug, MV_DEBUG_COMP_SPV, sizeof MV_DEBUG_COMP_SPV, {S, I}, 4))
        return false;
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.25f;
    if (vkCreateSampler(dev.device, &si, nullptr, &mv.nearest) != VK_SUCCESS) return false;
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    if (vkCreateSampler(dev.device, &si, nullptr, &mv.linear) != VK_SUCCESS) return false;
    mv.failed = false;
    logf("vk: hybrid MVec ready (colour flow, max edge %u, radius default 4%s; RX_VK_MVEC_DEBUG=%d)", max_edge(),
         env_debug() ? " + debug overlay" : "", env_debug() ? 1 : 0);
    return true;
}

void drop_depth_view() {
    if (mv.depth_view) {
        vkDestroyImageView(dev.device, mv.depth_view, nullptr);
        mv.depth_view = VK_NULL_HANDLE;
    }
}

bool refresh_depth_view() {
    drop_depth_view();
    if (!mv.depth_tex || !mv.depth_tex->image) return false;
    if (!(mv.depth_tex->usage & VK_IMAGE_USAGE_SAMPLED_BIT)) return false;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = mv.depth_tex->image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = mv.depth_tex->format;
    VkImageAspectFlags aspect = mv.depth_tex->aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!aspect) aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange = {aspect, mv.depth_level, 1, mv.depth_layer, 1};
    return vkCreateImageView(dev.device, &vi, nullptr, &mv.depth_view) == VK_SUCCESS;
}

bool ensure_size(VkCommandBuffer cmd, uint32_t w, uint32_t h) {
    if (mv.w == w && mv.h == h && mv.mvec.image) return true;
    {
        std::lock_guard qk(dev.present_mu);
        device_wait_idle_locked();
    }
    for (int i = 0; i < 2; i++) {
        drop_img(mv.colors[i]);
        drop_img(mv.lumas[i]);
        drop_img(mv.flows[i]);
    }
    drop_img(mv.mvec);
    drop_img(mv.repro);
    drop_img(mv.debug_img);
    mv.w = w;
    mv.h = h;
    mv.have_prev = false;
    mv.valid = false;
    const uint32_t qw = (w + 3) / 4, qh = (h + 3) / 4, cw = (qw + 3) / 4, ch = (qh + 3) / 4;
    const VkImageUsageFlags rw = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    bool ok = true;
    for (int i = 0; i < 2; i++) {
        ok = ok && make_img(mv.colors[i], w, h, VK_FORMAT_R8G8B8A8_UNORM, rw);
        ok = ok && make_img(mv.lumas[i], qw, qh, VK_FORMAT_R16_SFLOAT, rw);
        ok = ok && make_img(mv.flows[i], cw, ch, VK_FORMAT_R16G16B16A16_SFLOAT, rw);
    }
    ok = ok && make_img(mv.mvec, w, h, VK_FORMAT_R16G16_SFLOAT, rw | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ok = ok && make_img(mv.repro, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, rw);
    if (env_debug()) ok = ok && make_img(mv.debug_img, w, h, VK_FORMAT_R8G8B8A8_UNORM, rw | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!ok) {
        mv.failed = true;
        logf("vk: hybrid MVec: images (%ux%u) were not made: off", w, h);
        return false;
    }
    for (int i = 0; i < 2; i++) {
        to_general(cmd, mv.colors[i].image);
        to_general(cmd, mv.lumas[i].image);
        to_general(cmd, mv.flows[i].image);
    }
    to_general(cmd, mv.mvec.image);
    to_general(cmd, mv.repro.image);
    if (mv.debug_img.image) to_general(cmd, mv.debug_img.image);
    return true;
}

void pass(VkCommandBuffer cmd, const Pipe& p, std::initializer_list<std::pair<VkImageView, VkSampler>> bindings, uint32_t w,
          uint32_t h, const void* push = nullptr, uint32_t push_bytes = 0) {
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

bool invert4(const float m[16], float out[16]) {
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] -
             m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] +
             m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] +
              m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
             m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] +
             m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] +
             m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] +
              m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] +
             m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] +
              m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-12f) return false;
    const float id = 1.0f / det;
    for (int i = 0; i < 16; i++) out[i] = inv[i] * id;
    return true;
}

bool finite16(const float* m) {
    for (int i = 0; i < 16; i++)
        if (!(m[i] == m[i]) || std::fabs(m[i]) > 1e8f) return false;
    return true;
}

/// Score a column-major 4×4 as a likely view-projection (higher = better). 0 = reject.
float score_vp(const float* m) {
    if (!finite16(m)) return 0.f;
    float inv[16];
    if (!invert4(m, inv)) return 0.f;
    float abs_sum = 0.f;
    for (int i = 0; i < 16; i++) abs_sum += std::fabs(m[i]);
    if (abs_sum < 1.f) return 0.f;
    float id_err = 0.f;
    for (int i = 0; i < 16; i++) id_err += std::fabs(m[i] - ((i % 5 == 0) ? 1.f : 0.f));
    if (id_err < 0.01f) return 0.f;
    const float persp = std::max(std::fabs(m[11]), std::fabs(m[14]));
    if (persp < 0.05f && std::fabs(m[15] - 1.f) < 0.01f) return 0.f;
    float score = persp + 0.1f * std::min(abs_sum, 40.f);
    const float tx = std::fabs(m[12]), ty = std::fabs(m[13]), tz = std::fabs(m[14]);
    if (tx > 1e5f || ty > 1e5f || tz > 1e5f) score *= 0.2f;
    return score;
}

} // namespace

bool mvec_on() { return env_on() && init() && !mv.failed; }

void mvec_pin_depth(Texture* depth, uint32_t level, uint32_t layer, uint32_t w, uint32_t h) {
    if (!env_on() || !depth || !w || !h) return;
    mv.depth_tries++;
    // Exclude square shadow maps ≤768.
    if (w == h && w <= 768) return;
    // Prefer scene-like aspects; drop tiny buffers.
    if (w < 480 || h < 270) return;
    const float aspect = (float)w / (float)h;
    if (aspect < 1.2f || aspect > 2.4f) return;
    // Prefer matching the MVec colour size once known.
    if (mv.w && mv.h && (w != mv.w || h != mv.h)) {
        // Still accept if closer to internal 3D than previous pin and sizes are large.
        if (mv.depth_w && mv.depth_h && mv.depth_w == mv.w && mv.depth_h == mv.h) return;
    }
    const VkImage img = depth->image;
    const bool changed = depth != mv.depth_tex || img != mv.depth_image || level != mv.depth_level ||
                         layer != mv.depth_layer || w != mv.depth_w || h != mv.depth_h;
    mv.depth_tex = depth;
    mv.depth_image = img;
    mv.depth_level = level;
    mv.depth_layer = layer;
    mv.depth_w = w;
    mv.depth_h = h;
    mv.depth_hits++;
    if (changed) refresh_depth_view();
}

void mvec_set_viewproj(const float curr_vp[16], bool trusted) {
    if (!curr_vp) return;
    if (mv.have_vp) memcpy(mv.prev_vp, mv.curr_vp, sizeof mv.prev_vp);
    memcpy(mv.curr_vp, curr_vp, sizeof mv.curr_vp);
    if (!mv.have_vp) memcpy(mv.prev_vp, mv.curr_vp, sizeof mv.prev_vp);
    mv.have_vp = true;
    mv.vp_trusted = trusted;
}

void mvec_note_matrix_candidate(const float* m16, uint64_t prog, const char* where) {
    if (!env_mat() || !m16) return;
    static int logged;
    if (logged++ >= 24) return;
    logf("vk: MVec MAT candidate prog %016llx %s: "
         "[%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f]",
         (unsigned long long)prog, where ? where : "?", m16[0], m16[1], m16[2], m16[3], m16[4], m16[5], m16[6], m16[7],
         m16[8], m16[9], m16[10], m16[11], m16[12], m16[13], m16[14], m16[15]);
}

void mvec_scan_ubos(const NvnState& s, uint64_t prog, uint32_t pass_w, uint32_t pass_h) {
    if (!env_on() || !host || !host->gpu_read) return;
    // Only scan scene-sized passes (same heuristic as depth pin).
    if (pass_w < 480 || pass_h < 270) return;
    if (pass_w == pass_h && pass_w <= 768) return;
    const float aspect = (float)pass_w / (float)pass_h;
    if (aspect < 1.2f || aspect > 2.4f) return;

    float best_m[16]{};
    float best_score = 0.f;
    int best_stage = -1, best_index = -1;
    uint32_t best_off = 0;

    // Prefer a previously pinned slot when it still scores.
    auto try_buf = [&](int stage, int index, uint64_t addr, uint64_t size) {
        if (!addr || size < 64) return;
        const uint64_t n = std::min<uint64_t>(size, 2048);
        alignas(16) float buf[512];
        if (!host->gpu_read(addr, buf, n)) return;
        const uint32_t words = (uint32_t)(n / 4);
        for (uint32_t off = 0; off + 16 <= words; off += 4) {   // 16-byte aligned candidates
            const float* m = buf + off;
            const float sc = score_vp(m);
            if (sc > best_score) {
                best_score = sc;
                memcpy(best_m, m, 64);
                best_stage = stage;
                best_index = index;
                best_off = off * 4;
            }
        }
    };

    if (mv.pin_stage >= 0 && mv.pin_index >= 0 && mv.pin_stage < kNvnStages && mv.pin_index < 16) {
        const auto& u = s.ubo[mv.pin_stage][mv.pin_index];
        if (u.addr && u.size >= mv.pin_off + 64) {
            alignas(16) float m[16];
            if (host->gpu_read(u.addr + mv.pin_off, m, 64)) {
                const float sc = score_vp(m);
                if (sc > 0.f) {
                    best_score = sc + 1.f;   // sticky bonus
                    memcpy(best_m, m, 64);
                    best_stage = mv.pin_stage;
                    best_index = mv.pin_index;
                    best_off = mv.pin_off;
                }
            }
        }
    }

    // Scan vertex (0) and fragment (1) user UBOs (index >= 3; 0/2 are driver banks).
    for (int stage : {0, 1})
        for (int index = 3; index < 16; index++) {
            const auto& u = s.ubo[stage][index];
            if (u.addr) try_buf(stage, index, u.addr, u.size);
        }

    if (best_score <= 0.f) return;

    mvec_note_matrix_candidate(best_m, prog, "ubo-scan");

    const bool same_slot = best_stage == mv.pin_stage && best_index == mv.pin_index && best_off == mv.pin_off;
    if (same_slot) mv.pin_hits++;
    else {
        mv.pin_stage = best_stage;
        mv.pin_index = best_index;
        mv.pin_off = best_off;
        mv.pin_prog = prog;
        mv.pin_hits = 1;
    }

    // Auto-trust after a few consecutive hits on the same slot (or immediately if RX_VK_MVEC_TRUST=1).
    static const bool force = getenv("RX_VK_MVEC_TRUST") != nullptr;
    const bool trust = force || mv.pin_hits >= 3;
    mvec_set_viewproj(best_m, trust);
    if (trust && env_mat()) {
        static int trusted_logged;
        if (trusted_logged++ < 4)
            logf("vk: MVec MAT trusted: stage %d ubo%u +%u prog %016llx (hits %d)", best_stage, best_index, best_off,
                 (unsigned long long)prog, mv.pin_hits);
    }
}

bool mvec_record(VkCommandBuffer cmd, VkImageView color_view, uint32_t w, uint32_t h, bool skip_if_done) {
    const uint64_t frame = texture_frame_no();
    if (skip_if_done && mv.recorded_frame == frame && mv.valid) return true;
    if (!color_view || !w || !h || !mvec_on()) return false;
    // Cap working resolution so 4K native-output does not TDR the GPU on the block-match pass.
    const uint32_t src_w = w, src_h = h;
    const uint32_t edge = max_edge();
    if (std::max(w, h) > edge) {
        const float s = (float)edge / (float)std::max(w, h);
        w = std::max(1u, (uint32_t)std::lround(w * s));
        h = std::max(1u, (uint32_t)std::lround(h * s));
        static int capped_logged;
        if (capped_logged++ < 3)
            logf("vk: MVec capped %ux%u → %ux%u (RX_VK_MVEC_MAX=%u)", src_w, src_h, w, h, edge);
    }
    if (!ensure_size(cmd, w, h)) return false;
    const int cur = mv.cur, prev = cur ^ 1;
    const uint32_t qw = (w + 3) / 4, qh = (h + 3) / 4, cw = (qw + 3) / 4, ch = (qh + 3) / 4;
    full_barrier(cmd);
    const int32_t pc[4] = {0, 0, (int)w, (int)h};
    pass(cmd, mv.copy, {{color_view, mv.linear ? mv.linear : mv.nearest}, {mv.colors[cur].view, VK_NULL_HANDLE}}, w, h, pc,
         sizeof pc);
    pass(cmd, mv.luma, {{mv.colors[cur].view, mv.nearest}, {mv.lumas[cur].view, VK_NULL_HANDLE}}, qw, qh);

    const bool blend = mv.have_prev;
    int use_repro = 0;
    if (blend) {
        pass(cmd, mv.flow,
             {{mv.lumas[prev].view, mv.nearest}, {mv.lumas[cur].view, mv.nearest}, {mv.flows[0].view, VK_NULL_HANDLE}}, cw,
             ch);
        pass(cmd, mv.median, {{mv.flows[0].view, mv.nearest}, {mv.flows[1].view, VK_NULL_HANDLE}}, cw, ch);

        if (mv.vp_trusted && mv.have_vp && mv.depth_view && mv.depth_w == w && mv.depth_h == h) {
            float curr_inv[16];
            if (invert4(mv.curr_vp, curr_inv)) {
                struct {
                    float curr_inv[16];
                    float prev[16];
                    float size[2];
                    float z_near_eps;
                    float z_far_eps;
                } rpc{};
                memcpy(rpc.curr_inv, curr_inv, sizeof curr_inv);
                memcpy(rpc.prev, mv.prev_vp, sizeof mv.prev_vp);
                rpc.size[0] = (float)w;
                rpc.size[1] = (float)h;
                rpc.z_near_eps = 1e-6f;
                rpc.z_far_eps = 1e-5f;
                pass(cmd, mv.reproject, {{mv.depth_view, mv.nearest}, {mv.repro.view, VK_NULL_HANDLE}}, w, h, &rpc,
                     sizeof rpc);
                use_repro = 1;
            }
        }
        struct {
            int use_repro;
            float unused;
        } mpc{use_repro, 0.f};
        pass(cmd, mv.merge,
             {{mv.flows[1].view, mv.nearest}, {mv.repro.view, mv.nearest}, {mv.mvec.view, VK_NULL_HANDLE}}, w, h, &mpc,
             sizeof mpc);
        mv.valid = true;
    } else {
        // First frame: zero field.
        VkClearColorValue clear{{0.f, 0.f, 0.f, 0.f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, mv.mvec.image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        full_barrier(cmd);
        mv.valid = true;
    }

    if (env_debug() && mv.debug_img.image) {
        const float scale = getenv("RX_VK_MVEC_DEBUG_SCALE") ? (float)atof(getenv("RX_VK_MVEC_DEBUG_SCALE")) : 32.f;
        pass(cmd, mv.debug, {{mv.mvec.view, mv.nearest}, {mv.debug_img.view, VK_NULL_HANDLE}}, w, h, &scale, 4);
    }

    mv.have_prev = true;
    mv.cur = prev;
    mv.frames++;
    mv.recorded_frame = frame;

    // Validation log every 300 frames: size, flow path, depth-pin hit rate (mean |v| via RX_VK_MVEC_DEBUG overlay).
    if (mv.frames % 300 == 0) {
        const double hit = mv.depth_tries ? (100.0 * (double)mv.depth_hits / (double)mv.depth_tries) : 0.0;
        logf("vk: MVec %ux%u valid=%d merge=%s depth-pin hits %llu/%llu (%.1f%%) vp_trusted=%d "
             "(flow mean |v|: RX_VK_MVEC_DEBUG=1 false-colour; static≈grey, pan=streaks)",
             w, h, mv.valid ? 1 : 0, use_repro ? "reproject+flow" : "flow-only", (unsigned long long)mv.depth_hits,
             (unsigned long long)mv.depth_tries, hit, mv.vp_trusted ? 1 : 0);
        if (env_mat() && !mv.vp_trusted)
            logf("vk: MVec MAT: view-proj not trusted yet; reproject merge weight=0 (flow-only)");
    }
    return true;
}

VkImage mvec_image() { return mv.valid ? mv.mvec.image : VK_NULL_HANDLE; }
VkImageView mvec_view() { return mv.valid ? mv.mvec.view : VK_NULL_HANDLE; }
VmaAllocation mvec_alloc() { return mv.valid ? mv.mvec.alloc : nullptr; }
bool mvec_valid() { return mv.valid && mv.mvec.image; }
uint32_t mvec_width() { return mv.w; }
uint32_t mvec_height() { return mv.h; }
VkImage mvec_depth_image() { return mv.depth_image; }
VkImageView mvec_depth_view() { return mv.depth_view; }
Texture* mvec_depth_tex() { return mv.depth_tex; }
bool mvec_vp_trusted() { return mv.vp_trusted && mv.have_vp; }
const float* mvec_curr_vp() { return mv.curr_vp; }
const float* mvec_prev_vp() { return mv.prev_vp; }
VkImage mvec_debug_image() { return env_debug() && mv.debug_img.image ? mv.debug_img.image : VK_NULL_HANDLE; }
VkImageView mvec_debug_view() { return env_debug() && mv.debug_img.view ? mv.debug_img.view : VK_NULL_HANDLE; }
bool mvec_debug_active() { return env_debug() && mvec_valid() && mv.debug_img.image; }

} // namespace mkwvk
