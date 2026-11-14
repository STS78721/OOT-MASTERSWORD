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
#include <list>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>

#include "vk_internal.h"
#include "vk_jobs.h"
#include "vk_draw.h"   // (jobs_worker_placement)
#include "vk_paths.h"
#include "vk_shader.h"

namespace mkwvk {

VkShaderModule make_module_counted(const std::vector<uint32_t>& spirv);   // (vk_shader.cpp: a module, as before the pack)

// ---- 3. The memory pack (RX_MEM_PACK, on by default where the device takes inline SPIR-V) ----
// The census of 2026-11-14: 194425 shader modules made, 13.5 GB of SPIR-V handed to the driver, the host's private memory
// 12.8 GB of a 22 GB working set (the game's own memory 9.2 GB). The driver keeps each module's code for as long as the module
// lives, and every program kept its modules for the session, though a module is only read while a pipeline is made from it.
// Here the code goes to mastersword_spirv.pack beside the program cache, once for each distinct SPIR-V (content-addressed:
// the variants share most of it), and is read back and handed to the driver inline (VkShaderModuleCreateInfo chained to the
// stage, the graphics pipeline library's rule) at each pipeline build. Reads go through Windows' file cache: free RAM keeps
// the hot ones, and under pressure Windows drops them at once (clean file pages: nothing goes to the pagefile). The pack
// stays between runs (mastersword_spirv.idx: hash, place, size), so a later start writes only what is new.
namespace {
struct Pack {
    std::mutex mu;
    HANDLE file = INVALID_HANDLE_VALUE, idx = INVALID_HANDLE_VALUE;
    std::string path;   // (the warm-up opens its own handle: reads on one synchronous handle go one at a time)
    uint64_t end = 0;
    struct Rec {
        uint64_t at;
        uint32_t words;
    };
    std::unordered_map<uint64_t, Rec> recs;
    bool on = false;
    std::atomic<int64_t> reads{0}, read_bytes{0}, new_bytes{0}, shared_hits{0}, failed{0};
};
Pack& pack() {
    static Pack p;
    return p;
}

void triforce_warm();   // (below: Courage)

uint64_t code_hash(const std::vector<uint32_t>& w) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ (uint64_t)w.size();
    for (const uint32_t v : w) h = (h ^ v) * 0x100000001B3ull, h ^= h >> 29;
    return h;
}

void pack_open() {
    Pack& p = pack();
    const char* e = getenv("RX_MEM_PACK");
    if (e && *e == '0') {
        logf("vk: memory pack off (RX_MEM_PACK=0): shader modules kept for the session, as before");
        return;
    }
    if (!dev.graphics_pipeline_library) {
        logf("vk: memory pack off: this device does not take SPIR-V inline (no graphics pipeline library)");
        return;
    }
    char exe[MAX_PATH];
    const DWORD n = game_module_path(exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    std::string dir(exe, n);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    const std::string pack_path = dir + "mastersword_spirv.pack", idx_path = dir + "mastersword_spirv.idx";
    p.file = CreateFileA(pack_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    p.idx = CreateFileA(idx_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (p.file == INVALID_HANDLE_VALUE || p.idx == INVALID_HANDLE_VALUE) {
        logf("vk: memory pack off: %s could not be opened (error %lu)", pack_path.c_str(), GetLastError());
        if (p.file != INVALID_HANDLE_VALUE) CloseHandle(p.file), p.file = INVALID_HANDLE_VALUE;
        if (p.idx != INVALID_HANDLE_VALUE) CloseHandle(p.idx), p.idx = INVALID_HANDLE_VALUE;
        return;
    }
    LARGE_INTEGER size{};
    GetFileSizeEx(p.file, &size);
    p.end = (uint64_t)size.QuadPart;
    // (the index: 16-byte records, hash then place (40 bits) and size in words (24 bits); a record past the pack's end, or a
    // torn last one, ends it: the pack is then only appended to from there)
    LARGE_INTEGER isize{};
    GetFileSizeEx(p.idx, &isize);
    std::vector<uint64_t> raw((size_t)(isize.QuadPart / 16) * 2);
    DWORD got = 0;
    if (!raw.empty()) ReadFile(p.idx, raw.data(), (DWORD)(raw.size() * 8), &got, nullptr);
    uint64_t valid_end = 0;
    size_t kept = 0;
    for (size_t i = 0; i + 1 < raw.size() && (i + 2) * 8 <= got; i += 2) {
        const uint64_t at = raw[i + 1] & ((1ull << 40) - 1), words = raw[i + 1] >> 40;
        if (!words || at + words * 4 > p.end) break;
        p.recs[raw[i]] = {at, (uint32_t)words};
        valid_end = std::max(valid_end, at + words * 4);
        kept = i + 2;
    }
    p.end = std::max(valid_end, (uint64_t)0);
    LARGE_INTEGER to{};
    to.QuadPart = (LONGLONG)(kept * 8);
    SetFilePointerEx(p.idx, to, nullptr, FILE_BEGIN), SetEndOfFile(p.idx);
    to.QuadPart = (LONGLONG)p.end;
    SetFilePointerEx(p.file, to, nullptr, FILE_BEGIN), SetEndOfFile(p.file);
    p.on = true;
    p.path = pack_path;
    logf("vk: MasterSword Triforce Shader Delivery: the pack %s holds %zu shader codes (%llu MB); the driver keeps only the "
         "hot ones (RX_TRIFORCE_HOT_MB, default 1024), the rest are read back from the pack (RX_MEM_PACK=0: all as modules, as "
         "before)", pack_path.c_str(), p.recs.size(), (unsigned long long)(p.end >> 20));
    triforce_warm();
}
bool pack_on() {
    static std::once_flag once;
    std::call_once(once, pack_open);
    return pack().on;
}
} // namespace

bool stage_set_code(Stage& st, const std::vector<uint32_t>& spirv) {
    if (spirv.empty()) return false;
    if (!pack_on() || spirv.size() >= (1u << 24)) return (st.module = make_module_counted(spirv)) != VK_NULL_HANDLE;
    Pack& p = pack();
    const uint64_t h = code_hash(spirv);
    std::lock_guard lk(p.mu);
    if (auto it = p.recs.find(h); it != p.recs.end() && it->second.words == spirv.size()) {
        p.shared_hits++;
        st.code_at = it->second.at, st.code_words = it->second.words;
        return true;
    }
    const uint64_t at = p.end;
    // (positional, as the readers' are: no file pointer shared between the writer and stage_bind's reads)
    OVERLAPPED ov{};
    ov.Offset = (DWORD)at, ov.OffsetHigh = (DWORD)(at >> 32);
    DWORD put = 0;
    const DWORD bytes = (DWORD)(spirv.size() * 4);
    if (!WriteFile(p.file, spirv.data(), bytes, &put, &ov) || put != bytes) {
        p.failed++;
        return (st.module = make_module_counted(spirv)) != VK_NULL_HANDLE;   // (the disk full: a module, as before)
    }
    const uint64_t rec[2] = {h, at | (uint64_t)spirv.size() << 40};
    WriteFile(p.idx, rec, sizeof rec, &put, nullptr);
    p.end = at + bytes;
    p.new_bytes += bytes;
    p.recs[h] = {at, (uint32_t)spirv.size()};
    st.code_at = at, st.code_words = (uint32_t)spirv.size();
    return true;
}

// ---- MasterSword Triforce Shader Delivery ----
// Power: the pack above (every distinct shader once, on the NVMe). Wisdom: the hot cache below. Courage: the warm-up below.
// Why Wisdom: with the pack alone every pipeline build read its code back and the driver parsed it again: 53,000 reads,
// 3.4 GB a session, and the area changes' pipeline time doubled (417 -> 924 ms in their slowest frames). The hot cache keeps
// the modules of the code used lately, up to RX_TRIFORCE_HOT_MB (default 1024; 0: none, every build reads the pack), so an
// area's builds reuse a parsed module, and evicts the least recently used one in no build's hands (a module may be destroyed
// once the pipelines made from it exist). The RAM the driver holds stays bounded, where it held 13.5 GB before the pack.
namespace {
struct Hot {
    VkShaderModule module = VK_NULL_HANDLE;
    uint32_t words = 0;
    int refs = 0;
    std::list<uint64_t>::iterator lru;   // (in hot_lru while refs == 0: the eviction order)
};
std::mutex hot_mu;
std::unordered_map<uint64_t, Hot> hot;   // (by the code's place in the pack)
std::list<uint64_t> hot_lru;              // (front: the least recently used of those not in use)
uint64_t hot_bytes = 0;
std::atomic<int64_t> hot_hits{0}, hot_made{0}, hot_evicted{0};
uint64_t hot_budget() {
    static const uint64_t b = [] {
        const char* e = getenv("RX_TRIFORCE_HOT_MB");
        return (e && *e ? (uint64_t)std::max(0, atoi(e)) : 1024ull) << 20;
    }();
    return b;
}
void hot_evict_locked() {   // (hot_mu held)
    while (hot_bytes > hot_budget() && !hot_lru.empty()) {
        const uint64_t key = hot_lru.front();
        hot_lru.pop_front();
        auto it = hot.find(key);
        if (it == hot.end()) continue;
        vkDestroyShaderModule(dev.device, it->second.module, nullptr);
        hot_bytes -= (uint64_t)it->second.words * 4;
        hot.erase(it);
        hot_evicted++;
    }
}
bool pack_read(uint64_t at, uint32_t words, std::vector<uint32_t>& out) {
    Pack& p = pack();
    out.resize(words);
    OVERLAPPED ov{};
    ov.Offset = (DWORD)at, ov.OffsetHigh = (DWORD)(at >> 32);
    DWORD got = 0;
    const DWORD bytes = words * 4;
    if (!ReadFile(p.file, out.data(), bytes, &got, &ov) || got != bytes) {
        static std::atomic<int> told{0};
        if (told++ < 10) logf("vk: memory pack: a shader's code could not be read back (error %lu)", GetLastError());
        p.failed++;
        return false;
    }
    p.reads++, p.read_bytes += bytes;
    return true;
}
} // namespace

void triforce_release(uint64_t key) {
    std::lock_guard lk(hot_mu);
    auto it = hot.find(key);
    if (it == hot.end() || --it->second.refs > 0) return;
    it->second.lru = hot_lru.insert(hot_lru.end(), key);
    hot_evict_locked();
}

bool stage_bind(VkPipelineShaderStageCreateInfo& si, const Stage& st, StageCode& holder) {
    si.module = st.module;
    if (st.module || !st.code_words) return st.module != VK_NULL_HANDLE;
    const uint64_t key = st.code_at;
    if (hot_budget()) {   // (Wisdom: a module made lately for this code)
        std::lock_guard lk(hot_mu);
        if (auto it = hot.find(key); it != hot.end()) {
            if (it->second.refs++ == 0) hot_lru.erase(it->second.lru);
            holder.hot_key = key;
            si.module = it->second.module;
            hot_hits++;
            return true;
        }
    }
    if (!pack_read(st.code_at, st.code_words, holder.words)) return false;
    if (hot_budget()) {   // (made here, outside the lock: the driver parses it once for the builds that follow)
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = holder.words.size() * 4;
        ci.pCode = holder.words.data();
        VkShaderModule m = VK_NULL_HANDLE;
        if (vkCreateShaderModule(dev.device, &ci, nullptr, &m) == VK_SUCCESS && m) {
            std::lock_guard lk(hot_mu);
            auto [it, fresh] = hot.try_emplace(key);
            if (!fresh) {   // (another thread made it meanwhile: theirs)
                vkDestroyShaderModule(dev.device, m, nullptr);
                if (it->second.refs++ == 0) hot_lru.erase(it->second.lru);
            } else {
                it->second.module = m, it->second.words = (uint32_t)holder.words.size(), it->second.refs = 1;
                hot_bytes += ci.codeSize;
                hot_made++;
            }
            holder.hot_key = key;
            si.module = it->second.module;
            std::vector<uint32_t>().swap(holder.words);
            hot_evict_locked();
            return true;
        }
    }
    holder.ci.codeSize = holder.words.size() * 4;   // (no hot cache, or no module: inline, as the pack alone did)
    holder.ci.pCode = holder.words.data();
    holder.ci.pNext = si.pNext;
    si.pNext = &holder.ci;
    return true;
}

// Courage: the pack read ahead of need. At start, a thread at background priority (its disk reads and the pages they bring in
// both lowest: Windows takes those pages first) reads the pack front to back in 8 MB reads, so the first build of a shader in
// an area finds its code in the file cache, not on the disk: the NVMe's sequential speed instead of one small read at a time.
// Only when the PC has the pack's size free twice over (RX_TRIFORCE_WARM=0: never).
namespace {
void triforce_warm() {
    const char* e = getenv("RX_TRIFORCE_WARM");
    if (e && *e == '0') return;
    Pack& p = pack();
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (!p.on || !p.end || !GlobalMemoryStatusEx(&ms) || ms.ullAvailPhys < 2 * p.end) {
        if (p.on && p.end) logf("vk: Triforce Shader Delivery: no warm-up (%llu MB free, the pack %llu MB)",
                                (unsigned long long)(ms.ullAvailPhys >> 20), (unsigned long long)(p.end >> 20));
        return;
    }
    const uint64_t size = p.end;
    const HANDLE h = CreateFileA(p.path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                 FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::thread([size, h] {
        SetThreadDescription(GetCurrentThread(), L"mkw_vk Triforce warm-up");
        jobs_worker_placement();
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
        const auto t0 = GetTickCount64();
        std::vector<uint8_t> buf(8u << 20);
        uint64_t done = 0;
        while (done < size) {
            const DWORD n = (DWORD)std::min<uint64_t>(buf.size(), size - done);
            OVERLAPPED ov{};
            ov.Offset = (DWORD)done, ov.OffsetHigh = (DWORD)(done >> 32);
            DWORD got = 0;
            if (!ReadFile(h, buf.data(), n, &got, &ov) || !got) break;
            done += got;
        }
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
        CloseHandle(h);
        logf("vk: Triforce Shader Delivery: warm-up read %llu MB of the pack in %llu ms (background priority)",
             (unsigned long long)(done >> 20), (unsigned long long)(GetTickCount64() - t0));
    }).detach();
}
} // namespace

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
    {
        Pack& p = pack();
        if (p.on)
            logf("vk: memory pack: %zu shader codes (%llu MB on disk, %llu MB new this run), %lld stages shared one; %lld reads "
                 "(%lld MB) for pipeline builds, %lld failures",
                 p.recs.size(), (unsigned long long)(p.end >> 20), (unsigned long long)(p.new_bytes.load() >> 20),
                 (long long)p.shared_hits.load(), (long long)p.reads.load(), (long long)(p.read_bytes.load() >> 20),
                 (long long)p.failed.load());
        if (p.on) {
            std::lock_guard lk(hot_mu);
            logf("vk: Triforce hot cache: %zu modules (%llu MB of %llu), %lld builds served from it, %lld made, %lld evicted",
                 hot.size(), (unsigned long long)(hot_bytes >> 20), (unsigned long long)(hot_budget() >> 20),
                 (long long)hot_hits.load(), (long long)hot_made.load(), (long long)hot_evicted.load());
        }
    }
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
