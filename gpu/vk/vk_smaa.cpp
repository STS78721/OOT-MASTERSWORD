// Present-path SMAA 1x (iryoku/smaa Medium): edge detection → blending weights → neighborhood blend.
#include "vk_smaa.h"
#include "vk_menu.h"
#include "shaders/smaa_edge_comp_spv.h"
#include "shaders/smaa_weight_comp_spv.h"
#include "shaders/smaa_blend_comp_spv.h"
#include "smaa/smaa_tex_sizes.h"
#include "smaa/area_tex_bytes.h"
#include "smaa/search_tex_bytes.h"

#include <algorithm>
#include <cstring>

namespace mkwvk {

namespace {

constexpr int kSlots = 3;

struct Img {
    VkImage img = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;
};

struct {
    bool tried = false, ok = false, lookups = false;
    VkDescriptorSetLayout edge_dsl = VK_NULL_HANDLE, weight_dsl = VK_NULL_HANDLE, blend_dsl = VK_NULL_HANDLE;
    VkPipelineLayout edge_layout = VK_NULL_HANDLE, weight_layout = VK_NULL_HANDLE, blend_layout = VK_NULL_HANDLE;
    VkPipeline edge = VK_NULL_HANDLE, weight = VK_NULL_HANDLE, blend = VK_NULL_HANDLE;
    VkSampler nearest = VK_NULL_HANDLE, linear = VK_NULL_HANDLE;
    VkImage area = VK_NULL_HANDLE, search = VK_NULL_HANDLE;
    VmaAllocation area_alloc = nullptr, search_alloc = nullptr;
    VkImageView area_view = VK_NULL_HANDLE, search_view = VK_NULL_HANDLE;
    VkBuffer area_stage = VK_NULL_HANDLE, search_stage = VK_NULL_HANDLE;
    VmaAllocation area_stage_alloc = nullptr, search_stage_alloc = nullptr;
    Img edges, weights, out, color;
    VkImageView src_view[kSlots]{};
} S;

void img_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout old_layout, VkPipelineStageFlags2 src_stage,
                 VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = src_stage;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst_stage;
    b.dstAccessMask = dst_access;
    b.oldLayout = old_layout;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}

bool make_pipe(const uint32_t* spv, size_t spv_bytes, VkDescriptorSetLayout dsl, VkPipelineLayout* layout,
               VkPipeline* pipe, uint32_t push_bytes) {
    const VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &dsl;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pr;
    if (vkCreatePipelineLayout(dev.device, &li, nullptr, layout) != VK_SUCCESS) return false;
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = spv_bytes;
    mi.pCode = spv;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev.device, &mi, nullptr, &m) != VK_SUCCESS) return false;
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main",
                nullptr};
    ci.layout = *layout;
    const VkResult r = vkCreateComputePipelines(dev.device, VK_NULL_HANDLE, 1, &ci, nullptr, pipe);
    vkDestroyShaderModule(dev.device, m, nullptr);
    return r == VK_SUCCESS;
}

void free_img(Img& i) {
    if (i.view) vkDestroyImageView(dev.device, i.view, nullptr);
    if (i.img) destroy_image(i.img, i.alloc);
    i = {};
}

bool ensure_img(Img& i, uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage) {
    if (i.img && i.w == w && i.h == h) return true;
    {
        std::lock_guard qk(dev.present_mu);
        queue_flush_locked();
        vkDeviceWaitIdle(dev.device);
    }
    free_img(i);
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = fmt;
    ii.extent = {w, h, 1};
    ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    if (!create_image(ii, i.img, i.alloc)) return false;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = i.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev.device, &vi, nullptr, &i.view) != VK_SUCCESS) {
        free_img(i);
        return false;
    }
    i.w = w;
    i.h = h;
    return true;
}

bool create_lookup(VkImage* img, VmaAllocation* alloc, VkImageView* view, VkBuffer* stage, VmaAllocation* stage_alloc,
                   VkFormat fmt, uint32_t w, uint32_t h, const void* pixels, VkDeviceSize bytes) {
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = fmt;
    ii.extent = {w, h, 1};
    ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!create_image(ii, *img, *alloc)) return false;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = *img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev.device, &vi, nullptr, view) != VK_SUCCESS) return false;
    void* map = nullptr;
    if (!create_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, *stage, *stage_alloc, &map)) return false;
    std::memcpy(map, pixels, (size_t)bytes);
    return true;
}

void upload_lookups(VkCommandBuffer cmd) {
    if (S.lookups) return;
    auto copy = [&](VkBuffer stage, VkImage img, uint32_t w, uint32_t h) {
        img_barrier(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        // override newLayout for transfer dst
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        d.imageMemoryBarrierCount = 1;
        d.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cmd, &d);
        VkBufferImageCopy c{};
        c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        c.imageExtent = {w, h, 1};
        vkCmdCopyBufferToImage(cmd, stage, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
        b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        b.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier2(cmd, &d);
    };
    copy(S.area_stage, S.area, SMAA_AREATEX_WIDTH, SMAA_AREATEX_HEIGHT);
    copy(S.search_stage, S.search, SMAA_SEARCHTEX_WIDTH, SMAA_SEARCHTEX_HEIGHT);
    S.lookups = true;
}

bool init() {
    S.tried = true;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;

    auto make_dsl = [](std::initializer_list<VkDescriptorType> types, VkDescriptorSetLayout* out) {
        VkDescriptorSetLayoutBinding b[4]{};
        int n = 0;
        for (VkDescriptorType t : types) {
            b[n].binding = (uint32_t)n;
            b[n].descriptorType = t;
            b[n].descriptorCount = 1;
            b[n].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            n++;
        }
        VkDescriptorSetLayoutCreateInfo dci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
        dci.bindingCount = (uint32_t)n;
        dci.pBindings = b;
        return vkCreateDescriptorSetLayout(dev.device, &dci, nullptr, out) == VK_SUCCESS;
    };

    if (!make_dsl({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}, &S.edge_dsl))
        return false;
    if (!make_dsl({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                  &S.weight_dsl))
        return false;
    if (!make_dsl({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                  &S.blend_dsl))
        return false;

    if (!make_pipe(SMAA_EDGE_COMP_SPV, sizeof SMAA_EDGE_COMP_SPV, S.edge_dsl, &S.edge_layout, &S.edge, 32)) return false;
    if (!make_pipe(SMAA_WEIGHT_COMP_SPV, sizeof SMAA_WEIGHT_COMP_SPV, S.weight_dsl, &S.weight_layout, &S.weight, 16))
        return false;
    if (!make_pipe(SMAA_BLEND_COMP_SPV, sizeof SMAA_BLEND_COMP_SPV, S.blend_dsl, &S.blend_layout, &S.blend, 32))
        return false;

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.25f;
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;
    if (vkCreateSampler(dev.device, &si, nullptr, &S.nearest) != VK_SUCCESS) return false;
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    if (vkCreateSampler(dev.device, &si, nullptr, &S.linear) != VK_SUCCESS) return false;

    if (!create_lookup(&S.area, &S.area_alloc, &S.area_view, &S.area_stage, &S.area_stage_alloc, VK_FORMAT_R8G8_UNORM,
                       SMAA_AREATEX_WIDTH, SMAA_AREATEX_HEIGHT, SMAA_AREA_TEX, sizeof SMAA_AREA_TEX))
        return false;
    if (!create_lookup(&S.search, &S.search_alloc, &S.search_view, &S.search_stage, &S.search_stage_alloc,
                       VK_FORMAT_R8_UNORM, SMAA_SEARCHTEX_WIDTH, SMAA_SEARCHTEX_HEIGHT, SMAA_SEARCH_TEX,
                       sizeof SMAA_SEARCH_TEX))
        return false;

    logf("vk: SMAA ready (Medium)");
    return S.ok = true;
}

} // namespace

bool smaa_active() { return opts.smaa != 0; }

VkImage smaa_record(VkCommandBuffer cmd, int slot, VkImage src, VkImageView /*src_view*/, VkFormat fmt, uint32_t sw,
                    uint32_t sh, const int crop[4], VkImageView* out_view, uint32_t* out_w, uint32_t* out_h) {
    if (!smaa_active() || !src || !sw || !sh) return VK_NULL_HANDLE;
    if (!S.tried) init();
    if (!S.ok) return VK_NULL_HANDLE;
    upload_lookups(cmd);

    int x0 = 0, y0 = 0, cw = (int)sw, ch = (int)sh;
    if (crop && crop[2] > 0 && crop[3] > 0) {
        x0 = crop[0];
        y0 = crop[1];
        cw = crop[2];
        ch = crop[3];
    }
    cw = std::min(cw, (int)sw - x0);
    ch = std::min(ch, (int)sh - y0);
    if (cw <= 0 || ch <= 0) return VK_NULL_HANDLE;
    const uint32_t w = (uint32_t)cw, h = (uint32_t)ch;

    const VkImageUsageFlags mid = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const VkImageUsageFlags out_use = mid | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!ensure_img(S.edges, w, h, VK_FORMAT_R8G8B8A8_UNORM, mid) ||
        !ensure_img(S.weights, w, h, VK_FORMAT_R8G8B8A8_UNORM, mid) ||
        !ensure_img(S.out, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, out_use) ||
        !ensure_img(S.color, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, out_use))
        return VK_NULL_HANDLE;

    slot = ((slot % kSlots) + kSlots) % kSlots;
    if (S.src_view[slot]) vkDestroyImageView(dev.device, S.src_view[slot], nullptr), S.src_view[slot] = VK_NULL_HANDLE;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = src;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt != VK_FORMAT_UNDEFINED ? fmt : VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev.device, &vi, nullptr, &S.src_view[slot]) != VK_SUCCESS) return VK_NULL_HANDLE;

    const VkPipelineStageFlags2 CS = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, ALL = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                XFER = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    // Always work from S.color (cropped), so blend can write S.out safely.
    img_barrier(cmd, src, VK_IMAGE_LAYOUT_GENERAL, ALL, VK_ACCESS_2_MEMORY_WRITE_BIT, XFER, VK_ACCESS_2_TRANSFER_READ_BIT);
    img_barrier(cmd, S.color.img, VK_IMAGE_LAYOUT_UNDEFINED, ALL, 0, XFER, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    {
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = ALL;
        b.dstStageMask = XFER;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.image = S.color.img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        d.imageMemoryBarrierCount = 1;
        d.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cmd, &d);
    }
    VkImageBlit blit{};
    blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[0] = {x0, y0, 0};
    blit.srcOffsets[1] = {x0 + (int)w, y0 + (int)h, 1};
    blit.dstOffsets[1] = {(int)w, (int)h, 1};
    vkCmdBlitImage(cmd, src, VK_IMAGE_LAYOUT_GENERAL, S.color.img, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
    img_barrier(cmd, S.color.img, VK_IMAGE_LAYOUT_GENERAL, XFER, VK_ACCESS_2_TRANSFER_WRITE_BIT, CS,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    struct {
        float metrics[4];
        float threshold, u0, u1, u2;
    } edge_pc{{1.f / w, 1.f / h, (float)w, (float)h}, 0.1f, 0, 0, 0};

    img_barrier(cmd, S.edges.img, VK_IMAGE_LAYOUT_UNDEFINED, ALL, 0, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    VkDescriptorImageInfo edge_in{S.linear, S.color.view, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo edge_out{VK_NULL_HANDLE, S.edges.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet ew[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    ew[0].dstBinding = 0;
    ew[0].descriptorCount = 1;
    ew[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ew[0].pImageInfo = &edge_in;
    ew[1].dstBinding = 1;
    ew[1].descriptorCount = 1;
    ew[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ew[1].pImageInfo = &edge_out;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.edge);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.edge_layout, 0, 2, ew);
    vkCmdPushConstants(cmd, S.edge_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof edge_pc, &edge_pc);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);

    img_barrier(cmd, S.edges.img, VK_IMAGE_LAYOUT_GENERAL, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, CS,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    img_barrier(cmd, S.weights.img, VK_IMAGE_LAYOUT_UNDEFINED, ALL, 0, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    float weight_pc[4] = {1.f / w, 1.f / h, (float)w, (float)h};
    VkDescriptorImageInfo wi[4] = {
        {S.nearest, S.edges.view, VK_IMAGE_LAYOUT_GENERAL},
        {S.linear, S.area_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {S.nearest, S.search_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, S.weights.view, VK_IMAGE_LAYOUT_GENERAL},
    };
    VkWriteDescriptorSet ww[4]{};
    for (int i = 0; i < 4; i++) {
        ww[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ww[i].dstBinding = (uint32_t)i;
        ww[i].descriptorCount = 1;
        ww[i].descriptorType = i < 3 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        ww[i].pImageInfo = &wi[i];
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.weight);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.weight_layout, 0, 4, ww);
    vkCmdPushConstants(cmd, S.weight_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof weight_pc, weight_pc);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);

    img_barrier(cmd, S.weights.img, VK_IMAGE_LAYOUT_GENERAL, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, CS,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    img_barrier(cmd, S.out.img, VK_IMAGE_LAYOUT_UNDEFINED, ALL, 0, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    struct {
        float metrics[4];
        int32_t offset[2];
        int32_t pad[2];
    } blend_pc{{1.f / w, 1.f / h, (float)w, (float)h}, {0, 0}, {0, 0}};
    VkDescriptorImageInfo bi[3] = {
        {S.linear, S.color.view, VK_IMAGE_LAYOUT_GENERAL},
        {S.linear, S.weights.view, VK_IMAGE_LAYOUT_GENERAL},
        {VK_NULL_HANDLE, S.out.view, VK_IMAGE_LAYOUT_GENERAL},
    };
    VkWriteDescriptorSet bw[3]{};
    for (int i = 0; i < 3; i++) {
        bw[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bw[i].dstBinding = (uint32_t)i;
        bw[i].descriptorCount = 1;
        bw[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bw[i].pImageInfo = &bi[i];
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.blend);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.blend_layout, 0, 3, bw);
    vkCmdPushConstants(cmd, S.blend_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof blend_pc, &blend_pc);
    vkCmdDispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);

    img_barrier(cmd, S.out.img, VK_IMAGE_LAYOUT_GENERAL, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, ALL,
                VK_ACCESS_2_MEMORY_READ_BIT);
    if (out_view) *out_view = S.out.view;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return S.out.img;
}

void smaa_release() {
    for (int i = 0; i < kSlots; i++)
        if (S.src_view[i]) vkDestroyImageView(dev.device, S.src_view[i], nullptr), S.src_view[i] = VK_NULL_HANDLE;
    free_img(S.edges);
    free_img(S.weights);
    free_img(S.out);
    free_img(S.color);
    if (S.area_view) vkDestroyImageView(dev.device, S.area_view, nullptr);
    if (S.search_view) vkDestroyImageView(dev.device, S.search_view, nullptr);
    if (S.area) destroy_image(S.area, S.area_alloc);
    if (S.search) destroy_image(S.search, S.search_alloc);
    if (S.area_stage) destroy_buffer(S.area_stage, S.area_stage_alloc);
    if (S.search_stage) destroy_buffer(S.search_stage, S.search_stage_alloc);
    if (S.edge) vkDestroyPipeline(dev.device, S.edge, nullptr);
    if (S.weight) vkDestroyPipeline(dev.device, S.weight, nullptr);
    if (S.blend) vkDestroyPipeline(dev.device, S.blend, nullptr);
    if (S.nearest) vkDestroySampler(dev.device, S.nearest, nullptr);
    if (S.linear) vkDestroySampler(dev.device, S.linear, nullptr);
    if (S.edge_layout) vkDestroyPipelineLayout(dev.device, S.edge_layout, nullptr);
    if (S.weight_layout) vkDestroyPipelineLayout(dev.device, S.weight_layout, nullptr);
    if (S.blend_layout) vkDestroyPipelineLayout(dev.device, S.blend_layout, nullptr);
    if (S.edge_dsl) vkDestroyDescriptorSetLayout(dev.device, S.edge_dsl, nullptr);
    if (S.weight_dsl) vkDestroyDescriptorSetLayout(dev.device, S.weight_dsl, nullptr);
    if (S.blend_dsl) vkDestroyDescriptorSetLayout(dev.device, S.blend_dsl, nullptr);
    S = {};
}

} // namespace mkwvk
