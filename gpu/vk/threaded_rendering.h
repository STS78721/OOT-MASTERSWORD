#pragma once
#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Private option ID queried via rx_option(HOS_OPT_MS_RENDER_THREAD, -1) in host/nvn_native.c
// so gpu/vk/vk_api.h and gpu/vk/vk_menu.cpp stay 100% untouched.
constexpr int HOS_OPT_MS_RENDER_THREAD = 0x4D5301;

namespace mkwvk::threaded {

enum Setting { AUTO = 0, ON = 1, OFF = 2 };

constexpr bool kAutoEnabled = true;
constexpr unsigned kAutoRenderCores = 4;
constexpr unsigned kAutoRecorderCores = 6;
constexpr unsigned kAutoCpus = kAutoRenderCores;

enum class Why {
    On,
    Off,
    AutoGate,
    AutoUnsafe,
    AutoFewCores,
    AutoRenderThread,
    AutoBothThreads,
};
struct Choice {
    bool render_thread;
    bool recorder_thread;
    Why why;
};

constexpr Choice auto_picks(unsigned cores) {
    if (cores >= kAutoRecorderCores) return {true, true, Why::AutoBothThreads};
    if (cores >= kAutoRenderCores) return {true, false, Why::AutoRenderThread};
    return {false, false, Why::AutoFewCores};
}

constexpr Choice choose(int setting, unsigned cores, bool unsafe, bool auto_enabled = kAutoEnabled) {
    if (setting == ON) return {true, true, Why::On};
    if (setting == OFF) return {false, false, Why::Off};
    if (!auto_enabled) return {false, false, Why::AutoGate};
    if (unsafe) return {false, false, Why::AutoUnsafe};
    return auto_picks(cores);
}

constexpr bool auto_picked(const Choice& c) {
    return c.why == Why::AutoRenderThread || c.why == Why::AutoBothThreads;
}

constexpr unsigned cores_of(const uint64_t* core_masks, unsigned n, uint64_t usable) {
    unsigned cores = 0;
    for (unsigned i = 0; i < n; i++) cores += (core_masks[i] & usable) != 0;
    return cores;
}

inline unsigned usable_cpus() {
    const unsigned all = std::max(1u, std::thread::hardware_concurrency());
#ifdef _WIN32
    DWORD_PTR proc = 0, sys = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys) || !proc) return all;
    const unsigned n = (unsigned)__builtin_popcountll((unsigned long long)proc);
    return n < 64 ? std::clamp(n, 1u, all) : all;
#else
    return all;
#endif
}

inline unsigned usable_cores() {
    const unsigned cpus = usable_cpus();
#ifdef _WIN32
    DWORD_PTR proc = 0, sys = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys) || !proc) proc = ~(DWORD_PTR)0;
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info;
    DWORD len = 0;
    if (!GetLogicalProcessorInformation(nullptr, &len) && len) {
        info.resize(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) + 1);
        len = (DWORD)(info.size() * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(info.data(), &len)) info.resize(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        else info.clear();
    }
    std::vector<uint64_t> masks;
    for (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION& e : info)
        if (e.Relationship == RelationProcessorCore) masks.push_back((uint64_t)e.ProcessorMask);
    const unsigned cores = cores_of(masks.data(), (unsigned)masks.size(), (uint64_t)proc);
    return cores ? std::min(cores, cpus) : cpus;
#else
    return cpus;
#endif
}

} // namespace mkwvk::threaded
