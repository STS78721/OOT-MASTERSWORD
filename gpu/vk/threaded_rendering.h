#pragma once
#include <cstdint>
#include <thread>
#ifdef _WIN32
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
constexpr unsigned kAutoCpus = 12;

enum class Why {
    On,
    Off,
    AutoGate,
    AutoUnsafe,
    AutoFewCpus,
    AutoCpus,
};
struct Choice {
    bool on;
    Why why;
};

constexpr bool auto_picks(unsigned cpus) { return cpus >= kAutoCpus; }

constexpr Choice choose(int setting, unsigned cpus, bool unsafe, bool auto_enabled = kAutoEnabled) {
    if (setting == ON) return {true, Why::On};
    if (setting == OFF) return {false, Why::Off};
    if (!auto_enabled) return {false, Why::AutoGate};
    if (unsafe) return {false, Why::AutoUnsafe};
    return auto_picks(cpus) ? Choice{true, Why::AutoCpus} : Choice{false, Why::AutoFewCpus};
}

inline unsigned usable_cpus() {
#ifdef _WIN32
    DWORD_PTR proc = 0, sys = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys) && proc) {
        unsigned n = 0;
        for (uint64_t m = (uint64_t)proc; m; m &= m - 1) n++;
        return n;
    }
#endif
    return std::thread::hardware_concurrency();
}

} // namespace mkwvk::threaded
