"""Slate 3 Step 3 checks: multi-program DescMru, Hero's Shade v2 config/SHDE v2, and ParLock site removal.

python -I -B tests/test_slate3_step3.py [--cxx g++]
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def cut_between(text, start_marker, end_marker):
    s = text.index(start_marker)
    e = text.index(end_marker, s)
    return text[s:e]


def check_sources():
    draw = (ROOT / "gpu" / "vk" / "vk_draw.cpp").read_text(encoding="utf-8")
    mem = (ROOT / "gpu" / "vk" / "vk_memory.cpp").read_text(encoding="utf-8")
    pack = (ROOT / "gpu" / "vk" / "vk_mempack.cpp").read_text(encoding="utf-8")
    ih = (ROOT / "gpu" / "vk" / "vk_internal.h").read_text(encoding="utf-8")

    cc = block(draw, "void clear_color_target(")
    if "ParLock lk(ParSite::Clear, clear_parlock_on())" not in cc:
        raise SystemExit("clear_color_target still unconditionally locks ParLock")
    if "ParLock clk(ParSite::Clear, !clear_parlock_on())" not in cc:
        raise SystemExit("clear_color_target masked path lost ParLock")
    cds = block(draw, "void clear_depth_stencil(")
    if "ParLock lk(ParSite::Clear, clear_parlock_on())" not in cds:
        raise SystemExit("clear_depth_stencil still unconditionally locks ParLock")

    bind = block(mem, "BufferRef gpu_buffer_bind(")
    if "const bool skip_par = written && !write_parlock" not in bind:
        raise SystemExit("gpu_buffer_bind lost written tl_pools skip_par fast path")
    if "if (tl.gen != pools_gen.load(std::memory_order_relaxed) || pool_needs_par(tl.p))" not in bind:
        raise SystemExit("gpu_buffer_bind lost locked re-validation before pool_of")

    for need in ("add_cand(def_t, 64);", "add_cand(def_t, 96);", "add_cand(std::max(2, def_t - 1), 96);",
                 "RX_HEROS_SHADE_MIN_DRAWS", 'const uint32_t tag = 0x45444853u /* "SHDE" */, ver = 3;', "if (ver == 3 && takes(&count, 4))",
                 # (MasterSword Temper 2026-11-14: v3, counts above the default up to par_threads_max)
                 "par_threads_max_public()"):
        if need not in pack:
            raise SystemExit(f"vk_mempack.cpp missing Hero's Shade v2 piece: {need}")

    if "ParSite::TexView" not in draw or "par_site_stats[(size_t)site].locks.fetch_add" not in ih:
        raise SystemExit("ParSite instrumentation missing")


def check_desc_mru_cpp(cxx):
    draw = (ROOT / "gpu" / "vk" / "vk_draw.cpp").read_text(encoding="utf-8")
    desc_writer = block(draw, "struct DescWriter {") + ";"
    mru_code = cut_between(draw, "struct DescMruEntry {", "thread_local VkPipeline last_pipeline")

    # Replace TLS_OBJECT / global helpers with deterministic test harness state
    harness = r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL:%d %s\n", __LINE__, #x); return 1; } } while (0)

typedef uint64_t VkBuffer;
typedef uint64_t VkSampler;
typedef uint64_t VkImageView;
typedef uint64_t VkBufferView;
typedef uint64_t VkDescriptorSet;
typedef uint64_t VkDeviceSize;
enum VkDescriptorType { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1 };
enum { VK_NULL_HANDLE = 0, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35, VK_IMAGE_LAYOUT_GENERAL = 1 };
struct VkDescriptorBufferInfo { VkBuffer buffer; VkDeviceSize offset, range; };
struct VkDescriptorImageInfo { VkSampler sampler; VkImageView imageView; int imageLayout; };
struct VkWriteDescriptorSet {
    int sType;
    const void* pNext;
    VkDescriptorSet dstSet;
    uint32_t dstBinding, dstArrayElement, descriptorCount;
    VkDescriptorType descriptorType;
    const VkDescriptorImageInfo* pImageInfo;
    const VkDescriptorBufferInfo* pBufferInfo;
    const VkBufferView* pTexelBufferView;
};
struct Program { bool uo = true; int ubo_n = 2; };
struct Texture { int id; };
struct NvnState { uint64_t bind_gen = 1, tex_gen[5]{1,1,1,1,1}, ssbo_gen[5]{1,1,1,1,1}; };

static uint32_t g_epoch = 1;
static uint64_t g_stamp = 1, g_written = 1, g_samp = 1, g_destroy = 1;
static uint32_t g_slot_ver[32]{1,1,1,1};
static int g_rebuilds = 0;

bool env_value_off(const char* e) { return e && e[0] == '0'; }
bool desc_partial_on() { return true; }
bool program_uniform_only(const Program& p) { return p.uo; }
uint32_t texture_epoch() { return g_epoch; }
uint64_t gpu_bind_stamp() { return g_stamp; }
uint64_t texture_written_generation() { return g_written; }
uint64_t sampler_state_generation() { return g_samp; }
uint64_t texture_destroy_generation() { return g_destroy; }
uint32_t texture_slot_version(int slot) { return g_slot_ver[slot & 31]; }

STRUCT_DESC_WRITER
static DescWriter g_dw;
static DescWriter& dw() { return g_dw; }
static std::vector<uint64_t> g_desc_fp, g_draw_sampled, g_gfx_sampled;
static bool g_desc_fp_dummy = false;
static uint64_t g_desc_samp_seen = 1, g_desc_destroy_seen = 1;
static uint64_t last_desc_stamp = 1, last_desc_written = 1;
static uint32_t last_desc_epoch = 1;
static std::vector<const Texture*> g_draw_tex, g_gfx_tex;
std::vector<uint64_t>& desc_fp() { return g_desc_fp; }
std::vector<const Texture*>& draw_sampled() { return g_draw_tex; }
std::vector<const Texture*>& gfx_sampled() { return g_gfx_tex; }
#define desc_fp_dummy g_desc_fp_dummy
#define desc_samp_seen g_desc_samp_seen
#define desc_destroy_seen g_desc_destroy_seen

bool rebuild_uniform_ranges(const NvnState& s, const Program& p) {
    if (g_dw.prog != &p || (int)g_dw.bufs.size() < p.ubo_n) return false;
    g_rebuilds++;
    for (int i = 0; i < p.ubo_n; i++) g_dw.bufs[(size_t)i] = {100 + (VkBuffer)i, s.bind_gen * 16, 64};
    return true;
}

#define TLS_OBJECT(fn, T) static T& fn() { static T obj; return obj; }
'''
    mru_clean = re.sub(r"TLS_OBJECT\(desc_mru,\s*DescMruCache\)", "TLS_OBJECT(desc_mru, DescMruCache)", mru_code)
    tail = r'''
static void make_build(const NvnState& s, const Program& p, uint64_t img_id, const Texture* tex, uint32_t slot) {
    g_dw.reset();
    g_dw.prog = &p;
    for (int i = 0; i < p.ubo_n; i++) g_dw.bufs.push_back({100 + (VkBuffer)i, s.bind_gen * 16, 64});
    g_dw.bufs.push_back({900, 0, 128});   // ssbo
    g_dw.imgs.push_back({1, img_id, VK_IMAGE_LAYOUT_GENERAL});
    VkWriteDescriptorSet w0{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, 123, 0, 0, (uint32_t)p.ubo_n, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, g_dw.bufs.data(), nullptr};
    VkWriteDescriptorSet w1{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, 123, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, g_dw.bufs.data() + p.ubo_n, nullptr};
    VkWriteDescriptorSet w2{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, 123, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, g_dw.imgs.data(), nullptr, nullptr};
    g_dw.writes = {w0, w1, w2};
    g_desc_fp = {(uint64_t)g_slot_ver[slot] << 20 | slot};
    g_desc_fp_dummy = false;
    g_desc_samp_seen = g_samp, g_desc_destroy_seen = g_destroy;
    last_desc_stamp = g_stamp, last_desc_written = g_written, last_desc_epoch = g_epoch;
    g_gfx_tex = {tex};
}

int main() {
    NvnState s;
    Program A{true, 2}, B{true, 1}, NonUo{false, 2};
    Texture tA{1}, tB{2};
    bool exact = false;

    make_build(s, A, 501, &tA, 1);
    desc_mru_save(s, A, 10, 20, 30, false);

    // Interleaved draw B overwrites dw()
    s.bind_gen = 2;
    make_build(s, B, 502, &tB, 2);
    desc_mru_save(s, B, 10, 20, 30, false);
    CHECK(g_dw.prog == &B);

    // Non-uniform-only never caches
    make_build(s, NonUo, 503, &tA, 1);
    desc_mru_save(s, NonUo, 10, 20, 30, false);

    // Return to A with changed bind_gen -> hits MRU and updates uniform ranges only
    CHECK(desc_mru_try(s, A, 10, 20, 30, &exact));
    CHECK(!exact);
    CHECK(g_rebuilds == 1);
    CHECK(g_dw.prog == &A);
    CHECK(g_dw.writes[0].dstSet == VK_NULL_HANDLE);
    CHECK(g_dw.writes[0].pBufferInfo == g_dw.bufs.data());
    CHECK(g_dw.writes[1].pBufferInfo == g_dw.bufs.data() + 2 && g_dw.writes[1].pBufferInfo->buffer == 900);
    CHECK(g_dw.writes[2].pImageInfo == g_dw.imgs.data() && g_dw.writes[2].pImageInfo->imageView == 501);
    CHECK(g_dw.bufs[0].offset == s.bind_gen * 16);
    CHECK(g_draw_tex.size() == 1 && g_draw_tex[0] == &tA);
    desc_mru_save(s, A, 10, 20, 30, true);

    // Draw A again with same bind_gen & ub_gen after B -> exact hit, zero uniform rebuild
    make_build(s, B, 502, &tB, 2);
    CHECK(desc_mru_try(s, A, 10, 20, 30, &exact));
    CHECK(exact && g_rebuilds == 1);

    // Invalidations: slot version, tex_gen, ssbo_gen, epoch, stamp, written, samp, destroy, dgen
    g_slot_ver[1]++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_slot_ver[1]--;
    CHECK(desc_mru_try(s, A, 10, 20, 30, &exact));

    s.tex_gen[0]++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    s.tex_gen[0]--;
    s.ssbo_gen[1]++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    s.ssbo_gen[1]--;
    g_epoch++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_epoch--;
    g_stamp++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_stamp--;
    g_written++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_written--;
    g_destroy++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_destroy--;
    g_samp++;
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    g_samp--;
    CHECK(!desc_mru_try(s, A, 11, 20, 30, &exact));   // dgen changed

    // Capacity > kDescMruSlots evicts oldest cleanly
    desc_mru().reset();
    CHECK(!desc_mru_try(s, A, 10, 20, 30, &exact));
    std::vector<Program> many(20);
    for (int i = 0; i < 20; i++) {
        make_build(s, many[(size_t)i], (uint64_t)(700 + i), &tA, 0);
        desc_mru_save(s, many[(size_t)i], 10, 20, 30, false);
    }
    CHECK(!desc_mru_try(s, many[0], 10, 20, 30, &exact));
    CHECK(desc_mru_try(s, many[19], 10, 20, 30, &exact));
    std::printf("PASS: DescMru multi-program cache & invalidations (%lld hits)\n",
                (long long)st_desc_mru.load());
    return 0;
}
'''
    src = harness.replace("STRUCT_DESC_WRITER", desc_writer) + mru_clean + tail
    with tempfile.TemporaryDirectory(prefix="slate3-step3-") as tmp:
        cpp = Path(tmp) / "fixture.cpp"
        exe = Path(tmp) / "fixture.exe"
        cpp.write_text(src, encoding="utf-8")
        subprocess.run([cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cxx", default="g++")
    a = p.parse_args()
    check_sources()
    check_desc_mru_cpp(a.cxx)


if __name__ == "__main__":
    main()
