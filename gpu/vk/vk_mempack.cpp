// (MasterSword 0.1.9d) The game's RAM under pressure: the stutters of 2026-11-14 were the PC running out of free RAM (8.7 GB
// free at the start of a session, ~1 GB after it; the game's working set 14 -> 22.6 GB of a 32 GB PC). Windows then trims the
// game's pages and pages them back one fault at a time (game threads stalled up to 2 s with the renderer idle), and every new
// pool import waits for the driver to pin pages it has to free first (13-17 ms -> ~58 ms each).
//
// 1. Resident floor (RX_MEM_RESIDENT_MB): a soft minimum working set, so that Windows trims other programs before the game.
//    Soft: under real need Windows still takes pages from it, so the PC is never starved. Default 55% of RAM, at most 18 GB;
//    0 turns it off.
// 2. The census (RX_MEM_CENSUS, on by default, every 3600 frames on a worker): what the working set holds, by kind: the guest
//    window (the game's own memory), the host's private allocations (the renderer, the driver, the shader translator), mapped
//    files and code, with the largest private allocations. It says which host data the memory pack should take out of RAM.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "vk_internal.h"
#include "vk_jobs.h"

namespace mkwvk {

// (vk_shader.cpp make_module: the game programs' SPIR-V handed to the driver, which keeps a copy in its own heap; made, not
// counted down at destruction: an upper bound of what the driver may hold)
std::atomic<int64_t> g_module_bytes{0}, g_modules{0};

namespace {

int env_int(const char* name, int def) {
    const char* e = getenv(name);
    return e && *e ? atoi(e) : def;
}

void resident_floor() {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (!GlobalMemoryStatusEx(&ms)) return;
    const uint64_t total_mb = ms.ullTotalPhys >> 20;
    const int want = env_int("RX_MEM_RESIDENT_MB", -1);
    uint64_t floor_mb = want >= 0 ? (uint64_t)want : std::min<uint64_t>(total_mb * 55 / 100, 18 * 1024);
    if (!floor_mb) {
        logf("vk: resident floor off (RX_MEM_RESIDENT_MB=0)");
        return;
    }
    floor_mb = std::min<uint64_t>(floor_mb, total_mb * 80 / 100);   // (never most of the PC)
    const SIZE_T lo = (SIZE_T)(floor_mb << 20), hi = (SIZE_T)ms.ullTotalPhys;
    // (soft both ways: QUOTA_LIMITS_HARDWS_MIN_DISABLE, so the floor is the trimmer's order, not a lock)
    if (SetProcessWorkingSetSizeEx(GetCurrentProcess(), lo, hi, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE))
        logf("vk: resident floor: Windows trims other programs before the game's first %llu MB (of %llu MB RAM; soft, "
             "RX_MEM_RESIDENT_MB=<MB> or 0)", (unsigned long long)floor_mb, (unsigned long long)total_mb);
    else
        logf("vk: resident floor of %llu MB not set (error %lu): Windows trims the game as before", (unsigned long long)floor_mb,
             GetLastError());
}

struct Census {
    uint64_t guest = 0, priv = 0, mapped = 0, image = 0, shared = 0;
    struct Alloc {
        uintptr_t base;
        uint64_t resident, size;
    };
    std::vector<Alloc> top;
};

void census_run() {
    static std::atomic<bool> busy{false};
    if (busy.exchange(true)) return;
    struct Done {
        ~Done() { busy = false; }
    } done;
    const auto t0 = GetTickCount64();
    // (the working set's pages, one entry each: grown until it fits)
    std::vector<ULONG_PTR> buf(1 << 20);
    for (;;) {
        auto* ws = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(buf.data());
        if (K32QueryWorkingSet(GetCurrentProcess(), ws, (DWORD)(buf.size() * sizeof(ULONG_PTR)))) break;
        if (GetLastError() != ERROR_BAD_LENGTH || buf.size() > (64u << 20)) return;
        buf.resize(std::max<size_t>(buf.size() * 2, (size_t)ws->NumberOfEntries + 4096));
    }
    const auto* ws = reinterpret_cast<const PSAPI_WORKING_SET_INFORMATION*>(buf.data());
    std::vector<uintptr_t> pages((size_t)ws->NumberOfEntries);
    uint64_t shared = 0;
    for (size_t i = 0; i < pages.size(); i++) {
        pages[i] = (uintptr_t)ws->WorkingSetInfo[i].VirtualPage << 12;
        shared += ws->WorkingSetInfo[i].Shared;
    }
    std::vector<ULONG_PTR>().swap(buf);
    std::sort(pages.begin(), pages.end());
    const uintptr_t gbase = (uintptr_t)host->guest_base(), gend = gbase + (uintptr_t)host->guest_size();
    Census c;
    c.shared = shared;
    std::vector<Census::Alloc> allocs;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t region_end = 0;
    for (const uintptr_t p : pages) {
        if (p >= gbase && p < gend) {
            c.guest++;
            continue;
        }
        if (p >= region_end) {
            if (!VirtualQuery((const void*)p, &mbi, sizeof mbi)) continue;
            region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            if (mbi.Type == MEM_PRIVATE &&
                (allocs.empty() || allocs.back().base != (uintptr_t)mbi.AllocationBase))
                allocs.push_back({(uintptr_t)mbi.AllocationBase, 0, 0});
        }
        if (mbi.Type == MEM_IMAGE) c.image++;
        else if (mbi.Type == MEM_MAPPED) c.mapped++;
        else c.priv++, allocs.back().resident++;
    }
    // (an allocation's whole size: its regions, walked from its base)
    std::sort(allocs.begin(), allocs.end(), [](const Census::Alloc& a, const Census::Alloc& b) { return a.resident > b.resident; });
    if (allocs.size() > 10) allocs.resize(10);
    for (auto& a : allocs) {
        uintptr_t at = a.base;
        while (VirtualQuery((const void*)at, &mbi, sizeof mbi) && (uintptr_t)mbi.AllocationBase == a.base) {
            a.size += mbi.RegionSize;
            at = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }
    }
    const auto mb = [](uint64_t pages_) { return (unsigned long long)(pages_ >> 8); };
    logf("vk: memory census (%llu ms): working set %llu MB: guest window (the game's memory) %llu MB, host private (renderer, "
         "driver, translator) %llu MB, mapped files %llu MB, code %llu MB; %llu MB shared",
         (unsigned long long)(GetTickCount64() - t0), mb(pages.size()), mb(c.guest), mb(c.priv), mb(c.mapped), mb(c.image),
         mb(c.shared));
    logf("vk: memory census: %lld shader modules made, %lld MB of SPIR-V handed to the driver", (long long)g_modules.load(),
         (long long)(g_module_bytes.load() >> 20));
    for (const auto& a : allocs)
        logf("vk:   host allocation %#llx: %llu MB resident of %llu MB", (unsigned long long)a.base, mb(a.resident),
             (unsigned long long)(a.size >> 20));
}

} // namespace

/// (vk_texture.cpp's tend, every 30 frames) the resident floor once, the census now and then
void mempack_tick(uint64_t frame_no) {
    static bool floor_done = false;
    if (!floor_done) floor_done = true, resident_floor();
    static const int every = env_int("RX_MEM_CENSUS", 3600);
    if (every > 0 && frame_no >= 600 && frame_no % (uint64_t)std::max(every, 30) < 30) jobs_push([] { census_run(); });
}

} // namespace mkwvk
