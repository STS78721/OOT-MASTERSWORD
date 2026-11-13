// Colour adjustment on the present: brightness, contrast, saturation and gamma of the game's picture, after the
// game and before the window scale / blit. Settings from the Insert menu's Colour Profiles page (opts.color_*).
// At the default values nothing is recorded and the present is what it was. Design and preset values: STS78721's
// colour profiles (TOTK / DKB vivid OLED).
#include "vk_color.h"
#include "vk_menu.h"
#include "shaders/color_grade_comp_spv.h"

#include <algorithm>
#include <cstring>

namespace mkwvk {

namespace {

constexpr int kSlots = 3;

struct {
    bool tried = false, ok = false;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImage img = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    VkImageView src_view[kSlots]{};
} C;

bool init() {
    C.tried = true;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) return false;
    VkDescriptorSetLayoutBinding b[2] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo dci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    dci.bindingCount = 2;
    dci.pBindings = b;
    if (vkCreateDescriptorSetLayout(dev.device, &dci, nullptr, &C.dsl) != VK_SUCCESS) return false;
    const VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 32};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &C.dsl;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pr;
    if (vkCreatePipelineLayout(dev.device, &li, nullptr, &C.layout) != VK_SUCCESS) return false;
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.25f;
    if (vkCreateSampler(dev.device, &si, nullptr, &C.sampler) != VK_SUCCESS) return false;
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = sizeof COLOR_GRADE_COMP_SPV;
    mi.pCode = COLOR_GRADE_COMP_SPV;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev.device, &mi, nullptr, &m) != VK_SUCCESS) return false;
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, m, "main",
                nullptr};
    ci.layout = C.layout;
    const VkResult r = vkCreateComputePipelines(dev.device, VK_NULL_HANDLE, 1, &ci, nullptr, &C.pipe);
    vkDestroyShaderModule(dev.device, m, nullptr);
    if (r != VK_SUCCESS) {
        logf("vk: colour adjustment off: its pipeline could not be created");
        return false;
    }
    return C.ok = true;
}

void free_image() {
    if (C.view) vkDestroyImageView(dev.device, C.view, nullptr);
    if (C.img) destroy_image(C.img, C.alloc);
    C.view = VK_NULL_HANDLE;
    C.img = VK_NULL_HANDLE;
    C.w = C.h = 0;
}

bool ensure_image(uint32_t w, uint32_t h) {
    if (C.img && C.w == w && C.h == h) return true;
    {
        std::lock_guard qk(dev.present_mu);
        queue_flush_locked();
        vkDeviceWaitIdle(dev.device);
    }
    free_image();
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ii.extent = {w, h, 1};
    ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (!create_image(ii, C.img, C.alloc)) {
        C.img = VK_NULL_HANDLE;
        return false;
    }
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = C.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev.device, &vi, nullptr, &C.view) != VK_SUCCESS) {
        C.view = VK_NULL_HANDLE;
        free_image();
        return false;
    }
    C.w = w;
    C.h = h;
    return true;
}

void barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkPipelineStageFlags2 ss, VkAccessFlags2 sa,
             VkPipelineStageFlags2 ds, VkAccessFlags2 da) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = ss;
    b.srcAccessMask = sa;
    b.dstStageMask = ds;
    b.dstAccessMask = da;
    b.oldLayout = from;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}

} // namespace

bool color_grade_active() {
    return opts.color_brightness != 0 || opts.color_contrast != 100 || opts.color_saturation != 100 ||
           opts.color_gamma != 100;
}

VkImage color_grade_record(VkCommandBuffer cmd, int slot, VkImage src, VkImageView src_view, VkFormat fmt, uint32_t sw,
                           uint32_t sh, const int crop[4], VkImageView* out_view, uint32_t* out_w, uint32_t* out_h) {
    if (!color_grade_active() || !src || !src_view || !sw || !sh) return VK_NULL_HANDLE;
    if (!C.tried) init();
    int x0 = 0, y0 = 0, cw = (int)sw, ch = (int)sh;
    if (crop && crop[2] > 0 && crop[3] > 0) {
        x0 = crop[0];
        y0 = crop[1];
        cw = crop[2];
        ch = crop[3];
    }
    cw = std::min(cw, (int)sw - x0);
    ch = std::min(ch, (int)sh - y0);
    if (cw <= 0 || ch <= 0 || !C.ok || !ensure_image((uint32_t)cw, (uint32_t)ch)) return VK_NULL_HANDLE;
    slot = ((slot % kSlots) + kSlots) % kSlots;
    if (C.src_view[slot]) vkDestroyImageView(dev.device, C.src_view[slot], nullptr), C.src_view[slot] = VK_NULL_HANDLE;
    // Prefer a dedicated view so format matches the sampled image (window textures are usually UNORM).
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = src;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt != VK_FORMAT_UNDEFINED ? fmt : VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(dev.device, &vi, nullptr, &C.src_view[slot]) != VK_SUCCESS) {
        C.src_view[slot] = VK_NULL_HANDLE;
        return VK_NULL_HANDLE;
    }
    const VkPipelineStageFlags2 CS = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, ALL = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier(cmd, src, VK_IMAGE_LAYOUT_GENERAL, ALL, VK_ACCESS_2_MEMORY_WRITE_BIT, CS, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    barrier(cmd, C.img, VK_IMAGE_LAYOUT_UNDEFINED, ALL, 0, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    VkDescriptorImageInfo ii{C.sampler, C.src_view[slot], VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo oi{VK_NULL_HANDLE, C.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    w[0].dstBinding = 0;
    w[0].descriptorCount = 1;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[0].pImageInfo = &ii;
    w[1].dstBinding = 1;
    w[1].descriptorCount = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    w[1].pImageInfo = &oi;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, C.pipe);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, C.layout, 0, 2, w);
    struct {
        int32_t offset[2], size[2];
        float adj[4];
    } pc{{x0, y0},
         {cw, ch},
         {(float)opts.color_brightness / 100.0f, (float)opts.color_contrast / 100.0f,
          (float)opts.color_saturation / 100.0f, (float)opts.color_gamma / 100.0f}};
    vkCmdPushConstants(cmd, C.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof pc, &pc);
    vkCmdDispatch(cmd, ((uint32_t)cw + 7) / 8, ((uint32_t)ch + 7) / 8, 1);
    barrier(cmd, src, VK_IMAGE_LAYOUT_GENERAL, CS, 0, ALL, 0);
    barrier(cmd, C.img, VK_IMAGE_LAYOUT_GENERAL, CS, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, ALL,
            VK_ACCESS_2_MEMORY_READ_BIT);
    if (out_view) *out_view = C.view;
    if (out_w) *out_w = C.w;
    if (out_h) *out_h = C.h;
    (void)src_view;
    return C.img;
}

void color_grade_release() {
    for (int i = 0; i < kSlots; i++)
        if (C.src_view[i]) vkDestroyImageView(dev.device, C.src_view[i], nullptr), C.src_view[i] = VK_NULL_HANDLE;
    free_image();
    if (C.pipe) vkDestroyPipeline(dev.device, C.pipe, nullptr);
    if (C.sampler) vkDestroySampler(dev.device, C.sampler, nullptr);
    if (C.layout) vkDestroyPipelineLayout(dev.device, C.layout, nullptr);
    if (C.dsl) vkDestroyDescriptorSetLayout(dev.device, C.dsl, nullptr);
    C = {};
}

} // namespace mkwvk
