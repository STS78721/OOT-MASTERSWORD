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
#include <cstring>
#include <list>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vk_internal.h"
#include "vk_jobs.h"
#include "vk_draw.h"   // (jobs_worker_placement)
#include "vk_paths.h"
#include "vk_shader.h"

namespace mkwvk {

VkShaderModule make_module_counted(const std::vector<uint32_t>& spirv);   // (vk_shader.cpp: a module, as before the pack)
int par_threads_max_public();   // (vk_replay.cpp: the most parallel-replay workers Hero's Shade may try)

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
    std::unordered_map<uint64_t, uint64_t> hash_at;   // (a record's place -> its hash: the Mirror Shield's check)
    std::unordered_set<uint64_t> verified;             // (places checked this session: each once)
    std::string dir;
    bool on = false, damaged = false;
    std::atomic<int64_t> reads{0}, read_bytes{0}, new_bytes{0}, shared_hits{0}, failed{0}, shield_checked{0};
};
Pack& pack() {
    static Pack p;
    return p;
}

void triforce_warm();   // (below: Courage)
void song_of_time_play();   // (below: the Sheikah Slate's boot replay)
void slate_load();

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
    p.dir = dir;
    // (MasterSword Mirror Shield: a pack found damaged last run (a code whose hash was not its record's) is rebuilt from nothing:
    // the shaders are translated again and written anew, never handed to the driver as they were read)
    const std::string marker = dir + "mastersword_spirv.damaged";
    if (GetFileAttributesA(marker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(pack_path.c_str()), DeleteFileA(idx_path.c_str()), DeleteFileA((dir + "mastersword_spirv.song").c_str());
        DeleteFileA((dir + "mastersword_slate.bin").c_str());
        DeleteFileA(marker.c_str());
        logf("vk: MasterSword Mirror Shield: the shader pack was found damaged last run: rebuilt from nothing");
    }
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
        p.hash_at[at] = raw[i];
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
    slate_load();
    song_of_time_play();
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
    p.hash_at[at] = h;
    p.verified.insert(at);   // (made here from the code itself: nothing to check)
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
// (Wisdom, adaptive: the budget follows the PC's free RAM, measured every 300 frames (wisdom_adapt): 2 GB with over 8 GB free,
// 1 GB over 4, 512 MB over 2, 256 MB below. RX_TRIFORCE_HOT_MB=<MB> fixes it; 0: no hot cache)
std::atomic<uint64_t> hot_budget_auto{1024ull << 20};
int64_t hot_budget_fixed() {
    static const int64_t f = [] {
        const char* e = getenv("RX_TRIFORCE_HOT_MB");
        return e && *e ? (int64_t)std::max(0, atoi(e)) << 20 : (int64_t)-1;
    }();
    return f;
}
uint64_t hot_budget() {
    const int64_t f = hot_budget_fixed();
    return f >= 0 ? (uint64_t)f : hot_budget_auto.load(std::memory_order_relaxed);
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
    // (MasterSword Mirror Shield: each record's code checked against its hash the first time it is read this session)
    bool check;
    {
        std::lock_guard lk(p.mu);
        check = !p.verified.count(at);
    }
    if (check) {
        const uint64_t h = code_hash(out);
        std::lock_guard lk(p.mu);
        const auto it = p.hash_at.find(at);
        const auto rec = it == p.hash_at.end() ? p.recs.end() : p.recs.find(it->second);
        if (it == p.hash_at.end() || it->second != h || rec == p.recs.end() || rec->second.words != words) {
            p.failed++;
            static int told = 0;
            if (told++ < 8)
                logf("vk: MasterSword Mirror Shield: a shader's code in the pack is not what was written (at %llu): not handed to "
                     "the driver; the pack is rebuilt at the next start", (unsigned long long)at);
            if (!p.damaged) {
                p.damaged = true;
                const HANDLE m = CreateFileA((p.dir + "mastersword_spirv.damaged").c_str(), GENERIC_WRITE, 0, nullptr,
                                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (m != INVALID_HANDLE_VALUE) CloseHandle(m);
            }
            return false;
        }
        p.verified.insert(at);
        p.shield_checked++;
    }
    return true;
}

// (Wisdom, adaptive: see hot_budget) every 300 frames from mempack_tick
// Hysteresis prevents bouncing across 512/1024/2048 MB thresholds and evicting thousands of cached modules.
void wisdom_adapt() {
    if (hot_budget_fixed() >= 0) return;
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (!GlobalMemoryStatusEx(&ms)) return;
    const uint64_t free_mb = ms.ullAvailPhys >> 20;
    const uint64_t cur_mb = hot_budget_auto.load(std::memory_order_relaxed) >> 20;
    uint64_t want_mb = cur_mb ? cur_mb : 1024ull;
    if (free_mb >= 10240) want_mb = 2048;
    else if (free_mb >= 6144) want_mb = std::max<uint64_t>(want_mb, 1024);
    else if (free_mb >= 5120) want_mb = 1024;
    else if (free_mb >= 3072) want_mb = std::clamp<uint64_t>(want_mb, 512, 1024);
    else if (free_mb >= 2560) want_mb = 512;
    else if (free_mb >= 1536) want_mb = std::min<uint64_t>(want_mb, 512);
    else want_mb = 256;
    const uint64_t want = want_mb << 20;
    const uint64_t was = hot_budget_auto.exchange(want);
    if (want != was) {
        logf("vk: MasterSword Triforce, Wisdom: the hot cache's budget %llu -> %llu MB (%llu MB of RAM free)",
             (unsigned long long)(was >> 20), (unsigned long long)(want >> 20), (unsigned long long)free_mb);
        std::lock_guard lk(hot_mu);
        hot_evict_locked();
    }
}
} // namespace

void triforce_release(uint64_t key) {
    std::lock_guard lk(hot_mu);
    auto it = hot.find(key);
    if (it == hot.end() || --it->second.refs > 0) return;
    it->second.lru = hot_lru.insert(hot_lru.end(), key);
    hot_evict_locked();
}

bool stage_bind(VkPipelineShaderStageCreateInfo& si, const Stage& st, StageCode& holder, bool* busy) {
    if (busy) *busy = false;
    si.module = st.module;
    if (st.module || !st.code_words) return st.module != VK_NULL_HANDLE;
    const uint64_t key = st.code_at;
    if (hot_budget()) {   // (Wisdom: a module made lately for this code)
        bool hit = false;
        {
            std::lock_guard lk(hot_mu);
            if (auto it = hot.find(key); it != hot.end()) {
                if (it->second.refs++ == 0) hot_lru.erase(it->second.lru);
                holder.hot_key = key;
                si.module = it->second.module;
                hot_hits++;
                hit = true;
            }
        }
        if (hit) return true;
    }
    if (!pack_read(st.code_at, st.code_words, holder.words)) return false;
    if (hot_budget()) {   // (made here, outside the lock: the driver parses it once for the builds that follow)
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = holder.words.size() * 4;
        ci.pCode = holder.words.data();
        VkShaderModule m = VK_NULL_HANDLE;
        VkResult r = VK_ERROR_UNKNOWN;
        {
            IntelDriverGate gate(busy != nullptr);   // (busy given: dispatch ask only tries the gate)
            if (gate.busy) {
                *busy = true;
                std::vector<uint32_t>().swap(holder.words);
                return false;
            }
            r = extern_guarded_shader_module(&ci, &m);
        }
        if (r == VK_SUCCESS && m) {
            {
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
                hot_evict_locked();
            }
            std::vector<uint32_t>().swap(holder.words);
            return true;
        }
    }
    holder.ci.codeSize = holder.words.size() * 4;   // (no hot cache, or no module: inline, as the pack alone did)
    holder.ci.pCode = holder.words.data();
    holder.ci.pNext = si.pNext;
    si.pNext = &holder.ci;
    return true;
}

// ---- MasterSword Sheikah Slate: the self-training memory (mastersword_slate.bin, kept across cold boots) ----
// What a start no longer has to learn again:
//  - scores: every use of a shader's code (a hot-cache hit, or a module made) is counted; at each save a code's score becomes
//    old * 0.75 + this session's uses, so what the player uses session after session rises and what they left behind fades;
//  - pictures: when an area streams in (more than 8 textures made in a frame), the codes first used in the next 15 s are taken
//    as that area's picture (up to 4096). When an area streams again, its first 24 codes are compared with every picture's
//    first 48; the best match of at least half is replayed: its codes made ready in the hot cache on an E-core thread, before
//    the area asks for them. No scene number is needed: an area is known by what it draws. 64 pictures, the least seen go;
//  - Wisdom's settled budget, the start's budget the next time.
// Saved every 1800 frames and when the window closes; damaged with the pack, rebuilt with it (Mirror Shield).
// Song of Time plays it at a start: the best-scored codes made ready, up to 90% of the budget.
// RX_TRIFORCE_SONG=0: no replay at a start; RX_SLATE_PICTURES=0: no pictures.
namespace {
struct Picture {
    // (SLT2: head, the first 48 programs (their keys: stable across runs) drawn after the area began streaming in; all, the
    // shader codes of every program drawn then, in first-draw order: what a match makes ready)
    std::vector<uint64_t> head, all;
    uint64_t seen = 0;   // (the session counter when last taken or matched)
};

// ---- Slate 3 / Temper 3: ABI-safe per-area & per-program attribution side tables ----
uint64_t picture_sig(const std::vector<uint64_t>& head) {
    if (head.empty()) return 0;
    uint64_t h = 0x9E3779B97F4A7C15ull ^ (uint64_t)head.size();
    const size_t n = std::min<size_t>(24, head.size());
    for (size_t i = 0; i < n; i++) {
        h ^= head[i] + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    }
    return h ? h : 1;
}
struct TemperProg {
    uint64_t draws = 0, desc_rebuilds = 0, uniform_rebuilds = 0;
};
struct TemperArea {
    uint64_t frames = 0, session_frames = 0;
    uint64_t par_locks = 0, draws = 0, desc_rebuilds = 0, uniform_rebuilds = 0;
    double sum_total_ms = 0, sum_submit_ms = 0, sum_host_ms = 0, sum_par_wait_ms = 0;
    uint64_t s_draws = 0, s_desc = 0, s_ubo = 0, s_locks = 0;
    double s_total_ms = 0, s_submit_ms = 0, s_host_ms = 0, s_wait_ms = 0;
    std::unordered_map<uint64_t, TemperProg> progs;
};
enum { T3_MAX_SLOTS = 128, T3_SETS = 128, T3_WAYS = 2, T3_BUCKETS = T3_SETS * T3_WAYS };
struct alignas(64) TemperBucket {
    uint64_t key = 0;
    uint32_t draws = 0, desc = 0, ubo = 0, _pad = 0;
};
struct alignas(64) TemperSlot {
    TemperBucket b[T3_BUCKETS]{};
};
struct TemperSnap {
    uint64_t key = 0;
    uint32_t draws = 0, desc = 0, ubo = 0;
};
struct ShadeEntry {
    int32_t threads = -1, seg = -1;
    double base_ms = 0, best_ms = 0;
    uint32_t flags = 0, reserved = 0;
};
struct ShadeCand {
    int threads = -1, seg = -1;
    std::vector<double> tot, sub;
    int seen = 0;
};
struct ShadeProbe {
    uint64_t area = 0;
    int step = -1, settle = 0, idle = 0;
    std::vector<ShadeCand> cands;
};
struct Temper3 {
    std::mutex mu;
    std::unordered_map<uint64_t, TemperArea> areas;
    uint64_t active_area = 0;
    std::vector<std::array<TemperSnap, T3_BUCKETS>> snaps;
    std::unordered_map<uint64_t, ShadeEntry> shade_learned;
    ShadeProbe shade_probe;
};
Temper3& t3_state() {
    static Temper3 t;
    return t;
}
TemperSlot t3_slots[T3_MAX_SLOTS + 1];
std::atomic<int> t3_slot_count{0};
inline thread_local TemperSlot* t3_my_slot = nullptr;   // (raw pointer: no TLS ctor/dtor)
std::atomic<uint64_t> t3_active_sig{0};
std::atomic<int> hs_def_threads{-1}, hs_def_seg{-1}, hs_active_threads{-1}, hs_active_seg{-1};

bool heros_shade_on() {
    static const bool on = getenv("RX_HEROS_SHADE") && atoi(getenv("RX_HEROS_SHADE")) != 0;
    return on;
}

bool temper3_on() {
    static const bool on = !getenv("RX_SLATE_TEMPER") || atoi(getenv("RX_SLATE_TEMPER")) != 0;
    return on;
}
TemperSlot* t3_alloc_slot() {
    const int idx = t3_slot_count.fetch_add(1, std::memory_order_acq_rel);
    TemperSlot* const s = &t3_slots[idx < T3_MAX_SLOTS ? idx : T3_MAX_SLOTS];
    t3_my_slot = s;
    return s;
}
void t3_drain_slots_locked(uint64_t area_sig) {
    Temper3& st = t3_state();
    TemperArea& a = st.areas[area_sig];
    const int n_raw = t3_slot_count.load(std::memory_order_acquire);
    const int n = std::min(n_raw, (int)T3_MAX_SLOTS);
    if ((int)st.snaps.size() < n) st.snaps.resize((size_t)n);
    for (int si = 0; si < n; si++) {
        TemperSlot& s = t3_slots[si];
        auto& snap = st.snaps[(size_t)si];
        for (int bi = 0; bi < T3_BUCKETS; bi++) {
            const uint64_t k = std::atomic_ref<uint64_t>(s.b[bi].key).load(std::memory_order_relaxed);
            if (!k || snap[(size_t)bi].key != k) continue;
            const uint32_t d = std::atomic_ref<uint32_t>(s.b[bi].draws).load(std::memory_order_relaxed);
            const uint32_t dd = std::atomic_ref<uint32_t>(s.b[bi].desc).load(std::memory_order_relaxed);
            const uint32_t du = std::atomic_ref<uint32_t>(s.b[bi].ubo).load(std::memory_order_relaxed);
            const uint32_t dd_diff = dd - snap[(size_t)bi].desc;
            const uint32_t du_diff = du - snap[(size_t)bi].ubo;
            const uint32_t d_diff = d - snap[(size_t)bi].draws;
            if (d_diff || dd_diff || du_diff) {
                TemperProg& tp = a.progs[k];
                tp.draws += d_diff, tp.desc_rebuilds += dd_diff, tp.uniform_rebuilds += du_diff;
                a.desc_rebuilds += dd_diff, a.uniform_rebuilds += du_diff;
                a.s_desc += dd_diff, a.s_ubo += du_diff;
                snap[(size_t)bi].draws = d, snap[(size_t)bi].desc = dd, snap[(size_t)bi].ubo = du;
            }
        }
    }
}
void t3_set_area(uint64_t sig) {
    Temper3& st = t3_state();
    std::lock_guard lk(st.mu);
    if (st.active_area != sig) {
        t3_drain_slots_locked(st.active_area);
        st.active_area = sig;
        t3_active_sig.store(sig, std::memory_order_release);
        if (heros_shade_on()) {
            const auto it = st.shade_learned.find(sig);
            if (it != st.shade_learned.end()) {
                hs_active_threads.store(it->second.threads, std::memory_order_release);
                hs_active_seg.store(it->second.seg, std::memory_order_release);
            } else {
                hs_active_threads.store(-1, std::memory_order_release);
                hs_active_seg.store(-1, std::memory_order_release);
            }
            st.shade_probe = ShadeProbe{};
        }
    }
}
void t3_note_draw_impl(uint64_t prog_key, bool desc_rebuilt, bool uniform_only) {
    if (!temper3_on() || !prog_key) return;
    TemperSlot* s = t3_my_slot;
    if (!s) s = t3_alloc_slot();
    if (s != &t3_slots[T3_MAX_SLOTS]) {
        const uint32_t set = (uint32_t)((prog_key * 0x9E3779B97F4A7C15ull) >> 57) & (T3_SETS - 1);
        TemperBucket& b0 = s->b[set * T3_WAYS];
        TemperBucket& b1 = s->b[set * T3_WAYS + 1];
        const uint64_t k0 = std::atomic_ref<uint64_t>(b0.key).load(std::memory_order_relaxed);
        if (k0 == prog_key) {
            std::atomic_ref<uint32_t>(b0.draws).store(b0.draws + 1, std::memory_order_relaxed);
            if (desc_rebuilt) std::atomic_ref<uint32_t>(b0.desc).store(b0.desc + 1, std::memory_order_relaxed);
            if (uniform_only) std::atomic_ref<uint32_t>(b0.ubo).store(b0.ubo + 1, std::memory_order_relaxed);
            return;
        }
        const uint64_t k1 = std::atomic_ref<uint64_t>(b1.key).load(std::memory_order_relaxed);
        if (k1 == prog_key) {
            std::atomic_ref<uint32_t>(b1.draws).store(b1.draws + 1, std::memory_order_relaxed);
            if (desc_rebuilt) std::atomic_ref<uint32_t>(b1.desc).store(b1.desc + 1, std::memory_order_relaxed);
            if (uniform_only) std::atomic_ref<uint32_t>(b1.ubo).store(b1.ubo + 1, std::memory_order_relaxed);
            return;
        }
        Temper3& st = t3_state();
        std::lock_guard lk(st.mu);
        const int si = (int)(s - t3_slots);
        if ((int)st.snaps.size() <= si) st.snaps.resize((size_t)si + 1);
        const int bi = !k0 ? (int)(set * T3_WAYS)
                           : (!k1 ? (int)(set * T3_WAYS + 1)
                                  : (b0.draws <= b1.draws ? (int)(set * T3_WAYS) : (int)(set * T3_WAYS + 1)));
        TemperBucket& victim = s->b[bi];
        TemperSnap& sp = st.snaps[(size_t)si][(size_t)bi];
        const uint64_t old_k = std::atomic_ref<uint64_t>(victim.key).load(std::memory_order_relaxed);
        if (old_k) {
            TemperArea& a = st.areas[st.active_area];
            const uint32_t d_diff = victim.draws - sp.draws;
            const uint32_t dd_diff = victim.desc - sp.desc;
            const uint32_t du_diff = victim.ubo - sp.ubo;
            if (d_diff || dd_diff || du_diff) {
                TemperProg& tp = a.progs[old_k];
                tp.draws += d_diff, tp.desc_rebuilds += dd_diff, tp.uniform_rebuilds += du_diff;
                a.desc_rebuilds += dd_diff, a.uniform_rebuilds += du_diff;
                a.s_desc += dd_diff, a.s_ubo += du_diff;
            }
        }
        std::atomic_ref<uint64_t>(victim.key).store(prog_key, std::memory_order_relaxed);
        std::atomic_ref<uint32_t>(victim.draws).store(1, std::memory_order_relaxed);
        std::atomic_ref<uint32_t>(victim.desc).store(desc_rebuilt ? 1u : 0u, std::memory_order_relaxed);
        std::atomic_ref<uint32_t>(victim.ubo).store(uniform_only ? 1u : 0u, std::memory_order_relaxed);
        sp = {prog_key, 0, 0, 0};
        return;
    }
    Temper3& st = t3_state();
    std::lock_guard lk(st.mu);
    TemperArea& a = st.areas[st.active_area];
    TemperProg& tp = a.progs[prog_key];
    tp.draws++;
    if (desc_rebuilt) tp.desc_rebuilds++, a.desc_rebuilds++, a.s_desc++;
    if (uniform_only) tp.uniform_rebuilds++, a.uniform_rebuilds++, a.s_ubo++;
}
void t3_record_frame(double total_ms, double submit_ms, int64_t par_locks_n, double par_wait_ms, int draws) {
    if (!temper3_on() && !heros_shade_on()) return;
    static const int log_env = [] { const char* e = getenv("RX_SLATE_TEMPER_LOG"); return e && *e ? atoi(e) : -1; }();
    const int log_every = log_env == 0 ? 0 : (log_env == 1 ? 120 : (log_env > 1 ? log_env : 600));
    const double t_ms = std::min(std::max(total_ms, 0.0), 500.0);
    const double sub_ms = std::min(std::max(submit_ms, 0.0), t_ms);
    const double host_ms = std::max(0.0, t_ms - sub_ms);
    const double pw_ms = std::min(std::max(par_wait_ms, 0.0), 500.0);
    std::string report, shade_report;
    {
        Temper3& st = t3_state();
        std::lock_guard lk(st.mu);
        TemperArea& a = st.areas[st.active_area];
        a.frames++;
        a.session_frames++;
        a.par_locks += (uint64_t)std::max<int64_t>(0, par_locks_n);
        a.draws += (uint64_t)std::max(0, draws);
        a.sum_total_ms += t_ms;
        a.sum_submit_ms += sub_ms;
        a.sum_host_ms += host_ms;
        a.sum_par_wait_ms += pw_ms;
        a.s_locks += (uint64_t)std::max<int64_t>(0, par_locks_n);
        a.s_draws += (uint64_t)std::max(0, draws);
        a.s_total_ms += t_ms;
        a.s_submit_ms += sub_ms;
        a.s_host_ms += host_ms;
        a.s_wait_ms += pw_ms;
        if (temper3_on() && log_every > 0 && (a.session_frames % (uint64_t)log_every) == 0) {
            t3_drain_slots_locked(st.active_area);
            std::vector<std::pair<uint64_t, TemperProg>> top(a.progs.begin(), a.progs.end());
            std::sort(top.begin(), top.end(), [](const auto& x, const auto& y) { return x.second.draws > y.second.draws; });
            if (top.size() > 3) top.resize(3);
            const double d = (double)a.session_frames;
            char buf[640];
            int n = snprintf(buf, sizeof buf,
                             "vk: Slate 3 Temper 3 (area %016llx, %llu fr): total %.2f ms (submit %.2f ms, game/host %.2f ms), "
                             "ParLock %.0f/%.2f ms, %.0f draws (%.0f desc, %.0f ubo)",
                             (unsigned long long)st.active_area, (unsigned long long)a.session_frames,
                             a.s_total_ms / d, a.s_submit_ms / d, a.s_host_ms / d,
                             (double)a.s_locks / d, a.s_wait_ms / d, (double)a.s_draws / d,
                             (double)a.s_desc / d, (double)a.s_ubo / d);
            if (!top.empty()) {
                n += snprintf(buf + n, sizeof buf - n, "; top:");
                for (const auto& [k, p] : top)
                    n += snprintf(buf + n, sizeof buf - n, " %016llx(%llu/%llud)", (unsigned long long)k,
                                  (unsigned long long)p.draws, (unsigned long long)p.desc_rebuilds);
            }
            report.assign(buf);
        }
        if (heros_shade_on() && st.active_area != 0) {
            const int def_t = hs_def_threads.load(std::memory_order_relaxed);
            const int def_s = hs_def_seg.load(std::memory_order_relaxed);
            static const int min_draws = [] {
                const char* e = getenv("RX_HEROS_SHADE_MIN_DRAWS");
                return e ? std::max(0, atoi(e)) : 48;
            }();
            if (def_t >= 2 && def_s >= 32 && !st.shade_learned.count(st.active_area) && a.session_frames >= 90) {
                ShadeProbe& sp = st.shade_probe;
                if (sp.area != st.active_area || sp.step < 0) {
                    if (draws >= min_draws) {
                        sp = ShadeProbe{};
                        sp.area = st.active_area;
                        sp.step = 0;
                        const auto add_cand = [&](int t, int s) {
                            for (const ShadeCand& c : sp.cands)
                                if (c.threads == t && c.seg == s) return;
                            sp.cands.push_back({t, s, {}, {}, 0});
                        };
                        add_cand(def_t, def_s);
                        add_cand(def_t, 64);
                        add_cand(def_t, 96);
                        add_cand(def_t, 240);
                        add_cand(std::max(2, def_t - 1), 96);
                        // (MasterSword Temper: the counts above the default, up to par_threads_max; before, the +1 was
                        // clamped back to the default and re-tested it)
                        {
                            const int top = par_threads_max_public();
                            for (int t = def_t + 1; t <= top; t++) add_cand(t, def_s), add_cand(t, 96);
                        }
                        hs_active_threads.store(sp.cands[0].threads, std::memory_order_release);
                        hs_active_seg.store(sp.cands[0].seg, std::memory_order_release);
                    }
                } else if (sp.step < (int)sp.cands.size()) {
                    if (draws < min_draws) {
                        if (++sp.idle >= 180) {
                            hs_active_threads.store(def_t, std::memory_order_release);
                            hs_active_seg.store(def_s, std::memory_order_release);
                            sp.step = -1;
                        }
                    } else {
                        ShadeCand& cur = sp.cands[(size_t)sp.step];
                        if (sp.settle < 4) {
                            sp.settle++;
                        } else {
                            cur.seen++;
                            if (t_ms < 100.0) cur.tot.push_back(t_ms), cur.sub.push_back(sub_ms);
                            if (cur.tot.size() >= 20 || cur.seen >= 32) {
                                sp.step++;
                                sp.settle = 0;
                                sp.idle = 0;
                                if (sp.step < (int)sp.cands.size()) {
                                    hs_active_threads.store(sp.cands[(size_t)sp.step].threads, std::memory_order_release);
                                    hs_active_seg.store(sp.cands[(size_t)sp.step].seg, std::memory_order_release);
                                } else {
                                    auto med_of = [](std::vector<double> v) {
                                        if (v.empty()) return 1e9;
                                        std::sort(v.begin(), v.end());
                                        return v[v.size() / 2];
                                    };
                                    const double base_tot = sp.cands[0].tot.size() >= 8 ? med_of(sp.cands[0].tot) : 0.0;
                                    const double base_sub = sp.cands[0].sub.size() >= 8 ? med_of(sp.cands[0].sub) : 0.0;
                                    int best = 0;
                                    double best_tot = base_tot, best_sub = base_sub;
                                    if (base_tot > 0.0) {
                                        for (int k = 1; k < (int)sp.cands.size(); k++) {
                                            if (sp.cands[(size_t)k].tot.size() < 8) continue;
                                            const double mt = med_of(sp.cands[(size_t)k].tot);
                                            const double ms = med_of(sp.cands[(size_t)k].sub);
                                            const bool gain = (mt <= base_tot * 0.98 && ms <= base_sub * 1.02) ||
                                                              (ms <= base_sub * 0.95 && mt <= base_tot * 1.005);
                                            if (gain && mt < best_tot) best = k, best_tot = mt, best_sub = ms;
                                        }
                                    }
                                    const ShadeCand& win = sp.cands[(size_t)best];
                                    ShadeEntry se{win.threads, win.seg, base_tot, best_tot, best ? 1u : 0u, 0u};
                                    st.shade_learned[st.active_area] = se;
                                    hs_active_threads.store(se.threads, std::memory_order_release);
                                    hs_active_seg.store(se.seg, std::memory_order_release);
                                    char sbuf[256];
                                    snprintf(sbuf, sizeof sbuf,
                                             "vk: MasterSword Hero's Shade (area %016llx): latched %s (threads %d, seg %d): "
                                             "median %.2f -> %.2f ms (submit %.2f -> %.2f ms)",
                                             (unsigned long long)st.active_area, best ? "winner" : "baseline",
                                             se.threads, se.seg, base_tot, best_tot, base_sub, best_sub);
                                    shade_report.assign(sbuf);
                                    sp.step = -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!report.empty()) logf("%s", report.c_str());
    if (!shade_report.empty()) logf("%s", shade_report.c_str());
}
struct Tmp3AreaDisk {
    uint64_t pic_sig, frames, par_locks, draws, desc_rebuilds, uniform_rebuilds;
    double sum_total_ms, sum_submit_ms, sum_host_ms, sum_par_wait_ms;
    uint32_t prog_count, reserved;
};
struct Tmp3ProgDisk {
    uint64_t key, draws, desc_rebuilds, uniform_rebuilds;
};
struct TmpShdeDisk {
    uint64_t pic_sig;
    int32_t threads, seg;
    double base_ms, best_ms;
    uint32_t flags, reserved;
};
struct Slate {
    std::mutex mu;
    std::unordered_map<uint64_t, float> score;      // (by the code's place in the pack)
    std::unordered_map<uint64_t, uint32_t> uses;    // (this session's)
    std::vector<Picture> pictures;
    uint64_t sessions = 0, learned_budget = 0;
    // (the picture being taken)
    bool taking = false, matched = false;
    uint64_t taking_until = 0;
    std::vector<uint64_t> taken;            // (shader codes)
    std::vector<uint64_t> taken_programs;   // (program keys)
    std::unordered_set<uint64_t> taken_seen;
    std::atomic<int64_t> replays{0}, pictures_taken{0};
    // (the Sensor's this session: programs drawn, score events, area bursts)
    std::atomic<int64_t> programs_drawn{0}, score_events{0}, bursts{0};
    std::string fingerprint, last_fingerprint;   // (the GPU and driver: this run's, and the one the Slate last learned on)
};
Slate& slate() {
    static Slate s;
    return s;
}
bool song_on() {
    static const bool on = !getenv("RX_TRIFORCE_SONG") || atoi(getenv("RX_TRIFORCE_SONG")) != 0;
    return on;
}
bool pictures_on() {
    static const bool on = !getenv("RX_SLATE_PICTURES") || atoi(getenv("RX_SLATE_PICTURES")) != 0;
    return on;
}
std::atomic<int64_t> song_played{0};

/// Codes made ready in the hot cache, in the order given (most wanted first), up to fill of the budget, on an E-core thread
void make_ready(std::vector<uint64_t> list, double fill, const char* why) {
    if (list.empty() || !hot_budget()) return;
    std::thread([list = std::move(list), fill, why] {
        SetThreadDescription(GetCurrentThread(), L"mkw_vk Sheikah Slate");
        jobs_worker_placement();
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        const auto t0 = GetTickCount64();
        std::vector<uint32_t> words;
        int64_t made = 0;
        // (the least wanted first, so the most wanted end up the most recently used in the LRU)
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            const uint64_t at = *it;
            uint32_t n = 0;
            {
                Pack& pk = pack();
                std::lock_guard lk(pk.mu);
                const auto h = pk.hash_at.find(at);
                if (h == pk.hash_at.end()) continue;   // (not a record of this pack)
                const auto r = pk.recs.find(h->second);
                if (r == pk.recs.end()) continue;
                n = r->second.words;
            }
            {
                std::lock_guard lk(hot_mu);
                if (hot.count(at) || hot_bytes + (uint64_t)n * 4 > (uint64_t)(hot_budget() * fill)) continue;
            }
            if (!pack_read(at, n, words)) continue;
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize = words.size() * 4;
            ci.pCode = words.data();
            VkShaderModule m = VK_NULL_HANDLE;
            {
                IntelDriverGate gate;
                if (extern_guarded_shader_module(&ci, &m) != VK_SUCCESS || !m) continue;
            }
            std::lock_guard lk(hot_mu);
            auto [h, fresh] = hot.try_emplace(at);
            if (!fresh) {
                vkDestroyShaderModule(dev.device, m, nullptr);
                continue;
            }
            h->second.module = m, h->second.words = n, h->second.refs = 0;
            h->second.lru = hot_lru.insert(hot_lru.end(), at);
            hot_bytes += ci.codeSize;
            made++;
        }
        song_played += made;
        logf("vk: MasterSword %s: %lld shaders made ready in the hot cache in %llu ms", why, (long long)made,
             (unsigned long long)(GetTickCount64() - t0));
    }).detach();
}

size_t overlap(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    std::unordered_set<uint64_t> in(b.begin(), b.end());
    size_t n = 0;
    for (const uint64_t x : a) n += in.count(x);
    return n;
}

// ---- MasterSword Sheikah Sensor (Temper 2): the Slate's eyes on play ----
// The research of 2026-11-14: the start-up builds nearly every pipeline, so a Slate that learned from builds learned the
// start-up, and its area pictures never formed (0 in 12 area streams). Now it learns from draws: every draw bumps its
// program's counter (one relaxed increment, no lock), and the Slate is told only when the count reaches a power of two
// (1, 2, 4, 8 ...: a score that grows with the log of the draws) or at the program's first draw since an area began
// streaming in (the picture). Programs are known by their keys, stable across runs.
std::atomic<uint32_t> g_burst{0};          // (bumped as an area begins streaming in)
std::atomic<bool> g_taking{false};         // (a picture being taken: worth telling the Slate about first draws)

void slate_note_draw(const Program& p, bool score, bool first_in_burst) {
    uint64_t ats[8];
    int n = 0;
    for (const Stage& st : p.stages)
        if (st.code_words && n < 8) ats[n++] = st.code_at;
    Slate& sl = slate();
    std::vector<uint64_t> replay;
    uint64_t matched_sig = 0;
    {
        std::lock_guard lk(sl.mu);
        if (score) {
            sl.score_events++;
            for (int i = 0; i < n; i++) sl.uses[ats[i]]++;
        }
        if (!first_in_burst || !sl.taking || sl.taken_programs.size() >= 4096) return;
        sl.taken_programs.push_back(p.key);
        for (int i = 0; i < n; i++)
            if (sl.taken_seen.insert(ats[i]).second) sl.taken.push_back(ats[i]);
        if (sl.matched || sl.taken_programs.size() != 24) return;
        sl.matched = true;   // (the area's first 24 programs: which picture is it?)
        size_t best = 0, best_i = SIZE_MAX;
        for (size_t i = 0; i < sl.pictures.size(); i++) {
            const size_t o = overlap(sl.taken_programs, sl.pictures[i].head);
            if (o > best) best = o, best_i = i;
        }
        if (best_i == SIZE_MAX || best < 12) return;
        Picture& pic = sl.pictures[best_i];
        pic.seen = sl.sessions;
        matched_sig = picture_sig(pic.head);
        for (const uint64_t x : pic.all)
            if (!sl.taken_seen.count(x)) replay.push_back(x);
        sl.replays++;
        static int told = 0;
        if (told++ < 20)
            logf("vk: MasterSword Sheikah Slate: an area streams in; it matches a picture (%zu of its first 24 programs): %zu more "
                 "of its shaders made ready ahead", best, replay.size());
    }
    if (matched_sig) t3_set_area(matched_sig);
    make_ready(std::move(replay), 1.0, "Sheikah Slate, an area's picture");
}
} // namespace

void sheikah_sensor(Program& p) {
    // (a plain load and store, not a locked add: the recording threads draw the same programs at once, and an increment lost
    // now and then changes nothing on a log scale; the draw path pays no locked instruction)
    const uint32_t n = p.sensor_draws.load(std::memory_order_relaxed) + 1;
    p.sensor_draws.store(n, std::memory_order_relaxed);
    bool first_in_burst = false;
    if (g_taking.load(std::memory_order_relaxed)) {
        const uint32_t b = g_burst.load(std::memory_order_relaxed);
        if (p.sensor_burst.load(std::memory_order_relaxed) != b) p.sensor_burst.store(b, std::memory_order_relaxed), first_in_burst = true;
    }
    const bool score = (n & (n - 1)) == 0;
    if (!score && !first_in_burst) return;
    if (n == 1) slate().programs_drawn++;
    slate_note_draw(p, score, first_in_burst);
}

namespace {
/// Each frame (vk_texture.cpp texture_frame): textures made so far; more than 8 in a frame: an area streams in
void slate_frame_locked_free(uint64_t frame_no, uint64_t made) {
    static uint64_t prev = 0;
    const bool burst = made > prev + 8;
    prev = made;
    if (!pictures_on()) return;
    Slate& sl = slate();
    // (Temper 1: no lock on a frame with no burst and no picture being taken: nearly every frame)
    static std::atomic<bool> busy{false};
    if (!burst && !busy.load(std::memory_order_relaxed)) return;
    uint64_t updated_sig = 0;
    {
        std::lock_guard lk(sl.mu);
        if (sl.taking && frame_no > sl.taking_until) {   // (the picture is taken)
            sl.taking = false;
            if (sl.taken_programs.size() >= 16) {
                Picture pic;
                pic.head.assign(sl.taken_programs.begin(), sl.taken_programs.begin() + std::min<size_t>(48, sl.taken_programs.size()));
                pic.all = sl.taken;
                pic.seen = sl.sessions;
                updated_sig = picture_sig(pic.head);
                size_t best = 0, best_i = SIZE_MAX;   // (the same area seen again: its picture is replaced)
                for (size_t i = 0; i < sl.pictures.size(); i++) {
                    const size_t o = overlap(pic.head, sl.pictures[i].head);
                    if (o > best) best = o, best_i = i;
                }
                if (best_i != SIZE_MAX && best * 2 >= pic.head.size()) sl.pictures[best_i] = std::move(pic);
                else {
                    if (sl.pictures.size() >= 64) {
                        size_t old = 0;
                        for (size_t i = 1; i < sl.pictures.size(); i++)
                            if (sl.pictures[i].seen < sl.pictures[old].seen) old = i;
                        sl.pictures.erase(sl.pictures.begin() + (ptrdiff_t)old);
                    }
                    sl.pictures.push_back(std::move(pic));
                }
                sl.pictures_taken++;
            }
        }
        if (burst && !sl.taking) {   // (an area streams in: a picture is begun)
            sl.taking = true, sl.matched = false;
            sl.taking_until = frame_no + 900;
            sl.taken.clear(), sl.taken_programs.clear(), sl.taken_seen.clear();
            sl.bursts++;
            g_burst.fetch_add(1, std::memory_order_relaxed);
        }
        busy.store(sl.taking, std::memory_order_relaxed);
        g_taking.store(sl.taking, std::memory_order_relaxed);
    }
    if (updated_sig) t3_set_area(updated_sig);
}

std::string slate_path() { return pack().dir + "mastersword_slate.bin"; }

void slate_save() {
    Pack& p = pack();
    if (!p.on) return;
    static std::mutex save_mu;   // (the periodic save and the one as the window closes: one at a time)
    std::lock_guard save_lk(save_mu);
    Slate& sl = slate();
    std::vector<uint8_t> out;
    const auto put = [&out](const void* d, size_t n) { out.insert(out.end(), (const uint8_t*)d, (const uint8_t*)d + n); };
    {
        std::lock_guard lk(sl.mu);
        // (this session's uses folded into the scores as they stand now; the session's own counts go on growing)
        std::unordered_map<uint64_t, float> sc = sl.score;
        for (auto& [at, v] : sc) v *= 0.75f;
        for (const auto& [at, n] : sl.uses) sc[at] += (float)n;
        const uint32_t head[2] = {0x32544C53u /* "SLT2" */, 2};
        put(head, sizeof head);
        const uint64_t budget = hot_budget_auto.load(), sessions = sl.sessions;
        put(&budget, 8), put(&sessions, 8);
        // (the scores, the best 65536: a code under 0.05 is forgotten)
        std::vector<std::pair<uint64_t, float>> best(sc.begin(), sc.end());
        std::sort(best.begin(), best.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (best.size() > 65536) best.resize(65536);
        while (!best.empty() && best.back().second < 0.05f) best.pop_back();
        const uint64_t n = best.size();
        put(&n, 8);
        for (const auto& [at, v] : best) put(&at, 8), put(&v, 4);
        const uint64_t np = sl.pictures.size();
        put(&np, 8);
        for (const Picture& pic : sl.pictures) {
            const uint64_t nh = pic.head.size(), na = pic.all.size();
            put(&nh, 8), put(&na, 8), put(&pic.seen, 8);
            put(pic.head.data(), nh * 8), put(pic.all.data(), na * 8);
        }
        // (SLT2's optional sections, each a tag and a length: a reader skips the ones it doesn't know)
        const uint32_t fp_tag = 0x54525046u /* "FPRT" */, fp_len = (uint32_t)sl.fingerprint.size();
        put(&fp_tag, 4), put(&fp_len, 4), put(sl.fingerprint.data(), fp_len);
    }
    if (temper3_on()) {
        Temper3& st = t3_state();
        std::lock_guard tk(st.mu);
        t3_drain_slots_locked(st.active_area);
        std::vector<std::pair<uint64_t, const TemperArea*>> areas;
        for (const auto& [sig, a] : st.areas)
            if (a.frames) areas.push_back({sig, &a});
        std::sort(areas.begin(), areas.end(), [](const auto& x, const auto& y) { return x.second->frames > y.second->frames; });
        if (areas.size() > 64) areas.resize(64);
        const uint32_t tag = 0x33504D54u /* "TMP3" */, ver = 1, n_areas = (uint32_t)areas.size();
        uint32_t len_zero = 0;
        put(&tag, 4);
        const size_t len_at = out.size();
        put(&len_zero, 4);
        put(&ver, 4), put(&n_areas, 4);
        for (const auto& [sig, ap] : areas) {
            std::vector<std::pair<uint64_t, TemperProg>> top(ap->progs.begin(), ap->progs.end());
            std::sort(top.begin(), top.end(), [](const auto& x, const auto& y) { return x.second.draws > y.second.draws; });
            if (top.size() > 16) top.resize(16);
            Tmp3AreaDisk ad{sig, ap->frames, ap->par_locks, ap->draws, ap->desc_rebuilds, ap->uniform_rebuilds,
                            ap->sum_total_ms, ap->sum_submit_ms, ap->sum_host_ms, ap->sum_par_wait_ms, (uint32_t)top.size(), 0};
            put(&ad, sizeof ad);
            for (const auto& [k, pr] : top) {
                Tmp3ProgDisk pd{k, pr.draws, pr.desc_rebuilds, pr.uniform_rebuilds};
                put(&pd, sizeof pd);
            }
        }
        const uint32_t sec_len = (uint32_t)(out.size() - (len_at + 4));
        std::memcpy(out.data() + len_at, &sec_len, 4);
    }
    {
        Temper3& st = t3_state();
        std::lock_guard tk(st.mu);
        if (!st.shade_learned.empty()) {
            const uint32_t tag = 0x45444853u /* "SHDE" */, ver = 3;   // (3: learned with counts above the default)
            const uint32_t count = (uint32_t)std::min<size_t>(st.shade_learned.size(), 64);
            uint32_t len_zero = 0;
            put(&tag, 4);
            const size_t len_at = out.size();
            put(&len_zero, 4);
            put(&ver, 4), put(&count, 4);
            uint32_t written = 0;
            for (const auto& [sig, se] : st.shade_learned) {
                if (written++ >= count) break;
                TmpShdeDisk sd{sig, se.threads, se.seg, se.base_ms, se.best_ms, se.flags, se.reserved};
                put(&sd, sizeof sd);
            }
            const uint32_t sec_len = (uint32_t)(out.size() - (len_at + 4));
            std::memcpy(out.data() + len_at, &sec_len, 4);
        }
    }
    const std::string path = slate_path(), tmp = path + ".tmp";
    const HANDLE f = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    const bool ok = WriteFile(f, out.data(), (DWORD)out.size(), &w, nullptr) && w == out.size();
    CloseHandle(f);
    if (ok) MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    else DeleteFileA(tmp.c_str());
}

void slate_load() {
    Slate& sl = slate();
    {   // (this run's GPU and driver: the Sheikah Slate's fingerprint, SLT2)
        char fp[320];
        snprintf(fp, sizeof fp, "%s, driver %u.%u (%#x)", dev.props.deviceName, dev.props.driverVersion >> 22,
                 (dev.props.driverVersion >> 14) & 0xff, dev.props.driverVersion);
        std::lock_guard lk(sl.mu);
        sl.fingerprint = fp;
    }
    const HANDLE f = CreateFileA(slate_path().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        logf("vk: MasterSword Sheikah Slate: none yet (it learns this session)");
        return;
    }
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    std::vector<uint8_t> in((size_t)std::min<LONGLONG>(size.QuadPart, 512ll << 20));
    DWORD got = 0;
    if (!in.empty()) ReadFile(f, in.data(), (DWORD)in.size(), &got, nullptr);
    CloseHandle(f);
    in.resize(got);
    size_t at = 0;
    const auto take = [&](void* d, size_t n) {
        if (at + n > in.size()) return false;
        std::memcpy(d, in.data() + at, n), at += n;
        return true;
    };
    uint32_t head[2]{};
    uint64_t budget = 0, sessions = 0, n = 0, np = 0;
    // (SLT1 still read: its scores and budget kept; its pictures were of codes, not programs, and are dropped)
    if (!take(head, sizeof head) ||
        !((head[0] == 0x31544C53u && head[1] == 1) || (head[0] == 0x32544C53u && head[1] == 2)) || !take(&budget, 8) ||
        !take(&sessions, 8) || !take(&n, 8) || n > (1u << 20)) {
        logf("vk: MasterSword Sheikah Slate: not readable: it starts learning again");
        return;
    }
    std::lock_guard lk(sl.mu);
    std::lock_guard pk(pack().mu);
    const auto known = [](uint64_t a) { return pack().hash_at.count(a) != 0; };
    for (uint64_t i = 0; i < n; i++) {
        uint64_t a = 0;
        float v = 0;
        if (!take(&a, 8) || !take(&v, 4)) break;
        if (known(a)) sl.score[a] = v;
    }
    if (take(&np, 8) && np <= 64)
        for (uint64_t i = 0; i < np; i++) {
            uint64_t nh = 0, na = 0;
            Picture pic;
            if (!take(&nh, 8) || !take(&na, 8) || !take(&pic.seen, 8) || nh > 48 || na > 4096) break;
            pic.head.resize(nh), pic.all.resize(na);
            if (!take(pic.head.data(), nh * 8) || !take(pic.all.data(), na * 8)) break;
            std::erase_if(pic.all, [&](uint64_t a) { return !known(a); });
            if (head[1] == 2) sl.pictures.push_back(std::move(pic));
        }
    // (SLT2's sections: the fingerprint, TMP3, SHDE; unknown tags skipped by their length)
    size_t tmp3_off = 0, tmp3_len = 0, shde_off = 0, shde_len = 0;
    for (uint32_t tag = 0, len = 0; head[1] == 2 && take(&tag, 4) && take(&len, 4) && at + len <= in.size(); at += len) {
        if (tag == 0x54525046u /* "FPRT" */) sl.last_fingerprint.assign((const char*)in.data() + at, len);
        else if (tag == 0x33504D54u /* "TMP3" */) tmp3_off = at, tmp3_len = len;
        else if (tag == 0x45444853u /* "SHDE" */) shde_off = at, shde_len = len;
    }
    if (tmp3_len >= 8) {
        size_t ta = tmp3_off;
        const size_t tend = tmp3_off + tmp3_len;
        auto take3 = [&](void* d, size_t sz) {
            if (ta + sz > tend) return false;
            std::memcpy(d, in.data() + ta, sz), ta += sz;
            return true;
        };
        uint32_t ver = 0, n_areas = 0;
        if (take3(&ver, 4) && ver == 1 && take3(&n_areas, 4)) {
            if (n_areas > 64) n_areas = 64;
            Temper3& st = t3_state();
            std::lock_guard tk(st.mu);
            for (uint32_t ai = 0; ai < n_areas; ai++) {
                Tmp3AreaDisk ad{};
                if (!take3(&ad, sizeof ad)) break;
                TemperArea& a = st.areas[ad.pic_sig];
                a.frames += ad.frames, a.par_locks += ad.par_locks, a.draws += ad.draws;
                a.desc_rebuilds += ad.desc_rebuilds, a.uniform_rebuilds += ad.uniform_rebuilds;
                a.sum_total_ms += ad.sum_total_ms, a.sum_submit_ms += ad.sum_submit_ms;
                a.sum_host_ms += ad.sum_host_ms, a.sum_par_wait_ms += ad.sum_par_wait_ms;
                const uint32_t np_c = std::min<uint32_t>(ad.prog_count, 64);
                for (uint32_t pi = 0; pi < np_c; pi++) {
                    Tmp3ProgDisk pd{};
                    if (!take3(&pd, sizeof pd)) break;
                    TemperProg& tp = a.progs[pd.key];
                    tp.draws += pd.draws, tp.desc_rebuilds += pd.desc_rebuilds, tp.uniform_rebuilds += pd.uniform_rebuilds;
                }
            }
        }
    }
    if (shde_len >= 8) {
        size_t sa = shde_off;
        const size_t send = shde_off + shde_len;
        auto takes = [&](void* d, size_t sz) {
            if (sa + sz > send) return false;
            std::memcpy(d, in.data() + sa, sz), sa += sz;
            return true;
        };
        uint32_t ver = 0, count = 0;
        if (takes(&ver, 4)) {
            if (ver == 3 && takes(&count, 4)) {
                if (count > 64) count = 64;
                Temper3& st = t3_state();
                std::lock_guard tk(st.mu);
                for (uint32_t i = 0; i < count; i++) {
                    TmpShdeDisk sd{};
                    if (!takes(&sd, sizeof sd)) break;
                    st.shade_learned[sd.pic_sig] = {sd.threads, sd.seg, sd.base_ms, sd.best_ms, sd.flags, sd.reserved};
                }
            } else if (ver != 3) {
                logf("vk: MasterSword Hero's Shade: ignoring v%u table (re-learning, now with up to %d workers)", (unsigned)ver,
                     par_threads_max_public());
            }
        }
    }
    sl.sessions = sessions + 1;
    if (budget && hot_budget_fixed() < 0) hot_budget_auto.store(budget);
    if (!sl.last_fingerprint.empty() && sl.last_fingerprint != sl.fingerprint)
        logf("vk: MasterSword Sheikah Slate: the GPU or its driver changed since it last learned (%s -> %s): a longer start-up "
             "this time is the driver's caches being made again, not MasterSword", sl.last_fingerprint.c_str(),
             sl.fingerprint.c_str());
    logf("vk: MasterSword Sheikah Slate: session %llu; %zu shaders it knows the worth of, %zu area pictures; Wisdom starts at "
         "%llu MB, as it settled last time", (unsigned long long)sl.sessions, sl.score.size(), sl.pictures.size(),
         (unsigned long long)(hot_budget() >> 20));
}

// Song of Time: the Slate played at a start: its best-scored codes made ready, up to 90% of the budget
void song_of_time_play() {
    if (!pack().on || !song_on() || !hot_budget()) return;
    std::vector<uint64_t> list;
    {
        Slate& sl = slate();
        std::lock_guard lk(sl.mu);
        std::vector<std::pair<uint64_t, float>> best(sl.score.begin(), sl.score.end());
        std::sort(best.begin(), best.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& [at, v] : best) list.push_back(at);
    }
    make_ready(std::move(list), 0.9, "Song of Time (the Sheikah Slate's best, at the start)");
}
} // namespace

void slate_frame(uint64_t frame_no, uint64_t textures_made) { slate_frame_locked_free(frame_no, textures_made); }
void slate_close() { slate_save(); }   // (vk_main.cpp, as the window closes)
void temper3_note_draw(uint64_t prog_key, bool desc_rebuilt, bool uniform_only) { t3_note_draw_impl(prog_key, desc_rebuilt, uniform_only); }
void temper3_frame(double total_ms, double submit_ms, int64_t par_locks_n, double par_wait_ms, int draws) {
    t3_record_frame(total_ms, submit_ms, par_locks_n, par_wait_ms, draws);
}
void heros_shade_tune(int default_threads, int default_seg, int* out_threads, int* out_seg) {
    if (!heros_shade_on()) {
        *out_threads = default_threads;
        *out_seg = default_seg;
        return;
    }
    if (hs_def_threads.load(std::memory_order_relaxed) != default_threads)
        hs_def_threads.store(default_threads, std::memory_order_relaxed);
    if (hs_def_seg.load(std::memory_order_relaxed) != default_seg)
        hs_def_seg.store(default_seg, std::memory_order_relaxed);
    const int t = hs_active_threads.load(std::memory_order_acquire);
    const int s = hs_active_seg.load(std::memory_order_acquire);
    *out_threads = t >= 0 ? std::clamp(t, 0, std::max(default_threads, par_threads_max_public())) : default_threads;
    *out_seg = s >= 32 ? s : default_seg;
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

// (vk_texture.cpp's memory-pressure line asks for a census soon after: mempack_pressure_seen)
std::atomic<bool> census_wanted{false};
void mempack_pressure_seen() { census_wanted = true; }

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
    // (Temper 1: the pages' addresses written over the query's own entries and sorted there: no second list of them,
    // ~45 MB less at a census on a 22 GB working set)
    const size_t np = (size_t)reinterpret_cast<const PSAPI_WORKING_SET_INFORMATION*>(buf.data())->NumberOfEntries;
    uintptr_t* const pg = reinterpret_cast<uintptr_t*>(buf.data() + 1);
    uint64_t shared = 0;
    for (size_t i = 0; i < np; i++) {
        PSAPI_WORKING_SET_BLOCK b;
        std::memcpy(&b, &pg[i], sizeof b);
        shared += b.Shared;
        pg[i] = (uintptr_t)b.VirtualPage << 12;
    }
    std::sort(pg, pg + np);
    const uintptr_t gbase = (uintptr_t)host->guest_base(), gend = gbase + (uintptr_t)host->guest_size();
    Census c;
    c.shared = shared;
    std::vector<Census::Alloc> allocs;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t region_end = 0;
    for (size_t pi = 0; pi < np; pi++) {
        const uintptr_t p = pg[pi];
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
         (unsigned long long)(GetTickCount64() - t0), mb(np), mb(c.guest), mb(c.priv), mb(c.mapped), mb(c.image),
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
            logf("vk: Triforce hot cache: %zu modules (%llu MB of %llu), %lld builds served from it, %lld made, %lld evicted; "
                 "Sheikah Slate made %lld ready (%lld area pictures replayed, %lld taken); Mirror Shield checked %lld codes; "
                 "Sheikah Sensor: %lld programs drawn, %lld score events, %lld area bursts",
                 hot.size(), (unsigned long long)(hot_bytes >> 20), (unsigned long long)(hot_budget() >> 20),
                 (long long)hot_hits.load(), (long long)hot_made.load(), (long long)hot_evicted.load(),
                 (long long)song_played.load(), (long long)slate().replays.load(), (long long)slate().pictures_taken.load(),
                 (long long)p.shield_checked.load(), (long long)slate().programs_drawn.load(),
                 (long long)slate().score_events.load(), (long long)slate().bursts.load());
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
    // (Temper 1: a census cost 360-375 ms of a worker and ~90 MB for a moment, every minute: now once at 600 frames, then
    // every RX_MEM_CENSUS frames (18000: 5 minutes at 60 fps), and once more after a memory-pressure line, at most a minute
    // after the last)
    static const int every = env_int("RX_MEM_CENSUS", 18000);
    static uint64_t last_census = 0;
    const bool due = every > 0 && frame_no >= 600 &&
                     (!last_census || frame_no - last_census >= (uint64_t)std::max(every, 600) ||
                      (census_wanted.exchange(false) && frame_no - last_census >= 3600));
    // (Temper 2, Owl Statue awareness: no census and no Slate save while a game save is being flushed; next tick instead)
    using OwlBusyFn = int (*)(void);
    static const OwlBusyFn owl_busy = reinterpret_cast<OwlBusyFn>(GetProcAddress(GetModuleHandleA(nullptr), "hos_owl_busy"));
    const bool saving = owl_busy && owl_busy();
    if (due && !saving) last_census = frame_no, jobs_push([] { census_run(); });
    if (frame_no % 300 < 30) wisdom_adapt();   // (Wisdom, adaptive: the hot cache's budget by the PC's free RAM)
    static uint64_t last_slate_save = 0;
    if (frame_no >= 1800 && frame_no - last_slate_save >= 1800 && !saving)   // (the Sheikah Slate, kept)
        last_slate_save = frame_no, jobs_push([] { slate_save(); });
}

} // namespace mkwvk
