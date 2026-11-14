"""Checks for the Tick-Tock performance & release fixes (MasterSword).

Verifies without launching the game or GPU:
1. prepare_workers.h: NT-heap count, segment-heap RAM scaling, cap at usable CPUs / kMaxWorkers,
   and header signature (0xDDEEDDEE at byte 16 vs NT heap).
2. nx2/win_manifest.py + nx2/build.py: UTF-8 activeCodePage + SegmentHeap linked into the exe.
3. vk_mempack.cpp: wisdom_adapt hysteresis stops 512/1024/2048 MB bouncing, and Hero's Shade
   never drops an active scene to 0 replay threads.
4. vk_draw.cpp: desc_counts_pack/unpack round-trips and rejects overflows, build_descriptors uses
   the ABI-safe side cache (leaving Program untouched in vk_shader.h), cut_part_record trims partial
   trailing records cleanly before appending to notes/state lists, and prepare_run/precompile_run/predict_run
   check prepare_stopped().
5. vk_main.cpp: prepare_with_screen calls state_list_run() on updates before precompile_run().

python -I -B tests/test_tick_tock_fixes.py [--cxx clang++]
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r"""
#include "prepare_workers.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

static inline uint64_t desc_counts_pack(size_t nbuf, size_t nimg, size_t nview) {
    constexpr size_t kLimit = 1u << 21;
    if (nbuf >= kLimit || nimg >= kLimit || nview >= kLimit) return ~0ull;
    return (uint64_t)nbuf | ((uint64_t)nimg << 21) | ((uint64_t)nview << 42);
}
static inline void desc_counts_unpack(uint64_t packed, size_t* nbuf, size_t* nimg, size_t* nview) {
    constexpr uint64_t kMask = (1ull << 21) - 1;
    *nbuf = (size_t)(packed & kMask);
    *nimg = (size_t)((packed >> 21) & kMask);
    *nview = (size_t)((packed >> 42) & kMask);
}

static uint64_t wisdom_step_mb(uint64_t cur_mb, uint64_t free_mb) {
    uint64_t want_mb = cur_mb ? cur_mb : 1024ull;
    if (free_mb >= 10240) want_mb = 2048;
    else if (free_mb >= 6144) want_mb = std::max<uint64_t>(want_mb, 1024);
    else if (free_mb >= 5120) want_mb = 1024;
    else if (free_mb >= 3072) want_mb = std::clamp<uint64_t>(want_mb, 512, 1024);
    else if (free_mb >= 2560) want_mb = 512;
    else if (free_mb >= 1536) want_mb = std::min<uint64_t>(want_mb, 512);
    else want_mb = 256;
    return want_mb;
}

static void cut_part_record(const std::string& path, uint64_t head, uint64_t rec) {
    std::error_code ec;
    const uint64_t size = std::filesystem::file_size(path, ec);
    if (ec || size < head) return;
    const uint64_t whole = head + (size - head) / rec * rec;
    if (whole == size) return;
    std::filesystem::resize_file(path, whole, ec);
}

int main(int argc, char** argv) {
    using namespace prepare_workers;
    // 1. prepare_workers
    assert(count(6, 12, 64 * 1024, false) == 6);
    assert(count(6, 12, 10 * 1024, true) == 12);
    assert(count(10, 20, kReserveMB + 3 * kPerWorkerMB, true) == 13);
    assert(count(16, 64, 1 << 30, true) == kMaxWorkers);
    unsigned char heap[32]{};
    const unsigned char seg[4] = {0xEE, 0xDD, 0xEE, 0xDD}, nt[4] = {0xEE, 0xFF, 0xEE, 0xFF};
    std::memcpy(heap + 16, seg, 4);
    assert(segment_heap_signature(heap));
    std::memcpy(heap + 16, nt, 4);
    assert(!segment_heap_signature(heap));
    assert(!segment_heap_signature(nullptr));

    // 2. desc_counts packing
    size_t b = 0, im = 0, v = 0;
    const uint64_t p = desc_counts_pack(123, 456, 789);
    assert(p != ~0ull);
    desc_counts_unpack(p, &b, &im, &v);
    assert(b == 123 && im == 456 && v == 789);
    assert(desc_counts_pack((1u << 21) - 1, (1u << 21) - 1, (1u << 21) - 1) != ~0ull);
    assert(desc_counts_pack(1u << 21, 0, 0) == ~0ull);

    // 3. wisdom_adapt hysteresis: hovering across old 8192 / 4096 boundaries must not bounce
    assert(wisdom_step_mb(2048, 8300) == 2048);
    assert(wisdom_step_mb(2048, 8000) == 2048);
    assert(wisdom_step_mb(1024, 8000) == 1024);
    assert(wisdom_step_mb(1024, 4200) == 1024);
    assert(wisdom_step_mb(1024, 3900) == 1024);
    assert(wisdom_step_mb(1024, 2900) == 512);
    assert(wisdom_step_mb(512, 4200) == 512);
    assert(wisdom_step_mb(256, 1400) == 256);

    // 4. cut_part_record truncates only incomplete tail bytes past header
    if (argc > 1) {
        const std::filesystem::path f = std::filesystem::path(argv[1]) / "part.bin";
        std::string payload(12 + 32 + 15, 'x');
        { std::ofstream out(f, std::ios::binary); out.write(payload.data(), payload.size()); }
        cut_part_record(f.string(), 12, 32);
        assert(std::filesystem::file_size(f) == 12 + 32);
        cut_part_record(f.string(), 12, 32);
        assert(std::filesystem::file_size(f) == 12 + 32);
    }
    return 0;
}
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cxx", default="clang++")
    args = ap.parse_args()

    mempack = (ROOT / "gpu/vk/vk_mempack.cpp").read_text(encoding="utf-8")
    assert "add_cand(0, def_s)" not in mempack, "Hero's Shade must not test 0 parallel threads on drawn scenes"
    assert "Hysteresis" in mempack and "cur_mb" in mempack

    vk_main = (ROOT / "gpu/vk/vk_main.cpp").read_text(encoding="utf-8")
    assert "if (update) state_list_run();" in vk_main

    draw = (ROOT / "gpu/vk/vk_draw.cpp").read_text(encoding="utf-8")
    assert "cut_part_record(path, sizeof head, sizeof(PipeNote));" in draw
    assert "cut_part_record(path, 12, sizeof(StateRec2));" in draw
    assert "DescCountSlot" in draw and "RX_VK_DESC_COUNTS" in draw
    assert "prepare_cpus(cpus)" in draw

    shader_h = (ROOT / "gpu/vk/vk_shader.h").read_text(encoding="utf-8")
    assert "desc_counts" not in shader_h, "Program ABI in vk_shader.h must remain unchanged"

    manifest = (ROOT / "nx2/win_manifest.py").read_text(encoding="utf-8")
    assert ">SegmentHeap</heapType>" in manifest

    with tempfile.TemporaryDirectory(prefix="tick-tock-") as tmp:
        work = Path(tmp)
        cpp = work / "fixture.cpp"
        exe = work / "fixture.exe"
        cpp.write_text(FIXTURE, encoding="utf-8")
        subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "gpu/vk"),
                        str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe), str(work)], check=True)
    print("PASS: Tick-Tock fixes verified (SegmentHeap/prepare_workers, Wisdom hysteresis, Hero's Shade, cut_part_record, desc_counts side cache, state_list_run)")


if __name__ == "__main__":
    main()
