// The Insert menu. Drawn on the CPU (GDI text into a bitmap) and copied over the window's image at the last step of a present,
// so it costs nothing while closed and it never touches the game's picture, nor what frame interpolation sees. Keys: Insert
// opens and closes it, Up and Down choose, Left and Right change, Esc closes; the game gets no keyboard input while it is open.
// (hookshot) A gamepad's Home button, or Select and Start together, opens it too; the D-pad chooses and changes, the bottom
// face button changes too, the right one closes. The game gets no gamepad input while it is open either.
// Its Controls page binds the Switch's buttons to gamepad inputs and keys.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "vk_paths.h"
#include "vk_dlss.h"
#include "vk_fsr3.h"
#include "vk_fsr4.h"
#include "vk_temporal.h"
#include "upscaler_rows.h"
#include "vk_menu.h"
#include "vk_menu_hit.h"
#include "vk_menu_keys.h"
#include "menu_restart.h"
#include "menu_theme.h"
#include "vk_imgui.h"
#include "vk_ui.h"
#include "vk_reflex.h"
#include "vk_antilag.h"
#include "vk_framegen.h"
#include "vk_progress.h"
#include "vk_draw.h"
#include "vk_work.h"   // (after the others: counts the commands that do GPU work, for full_barrier)
#ifndef HOOKSHOT_VERSION   // (nx2/build.py gives it to this file: nx2/version.py)
#define HOOKSHOT_VERSION "unknown"
#endif

namespace mkwvk {
HWND app_window();

Options opts;
int menu_intel_compiles() { return opts.intel_compiles; }

namespace {

enum MenuTab {
    TAB_GRAPHICS = 0,
    TAB_GAMEPLAY = 1,
    TAB_COLOUR = 2,
    TAB_CONTROLS = 3,
    TAB_DEBUG = 4,
    TAB_COUNT = 5
};

struct Row {
    MenuTab tab;
    const char* name;
    int* value;
    int lo, hi, step;
    const char* (*text)(int v);
    const char* help;   // (shown in the help panel while the row is selected)
    bool preset_exempt = false;
    bool restart = false;
};
const char* tx_fg_backend(int v) {
    static const char* t[] = {"Off", "MasterSword", "SMFG 1.3 (Nodix)", "DLSS MFG", "FSR 3.1 FG", "XeSS MFG",
                              "Apex FG (Fast) (recommended)"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_framegen(int v) {
    static const char* t[] = {"Off", "", "2x", "3x", "4x", "5x", "6x", "7x", "8x"};
    return t[v];
}
const char* tx_framegen_preset(int v) {
    // NVIDIA DLSS FG models: A original, B UI recomposition, C Performance (queue parallelism / lighter path)
    static const char* t[] = {"A (original)", "B (UI)", "C (Performance)"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[1];
    return t[v];
}
const char* tx_fsr_quality(int v) {
    // (#2147) above Quality: 100/92/83/75 then AMD's modes (matches upscaler_rows::kFsr3Steps without Auto)
    static const char* t[] = {"Native AA 100%", "92%", "83%", "75%", "Quality 67%", "Balanced 58%",
                              "Performance 50%", "Ultra Performance 33%"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[4];
    return t[v];
}
const char* tx_nodix_quality(int v) {
    static const char* t[] = {"Quality", "Balanced", "Performance"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[1];
    return t[v];
}
const char* tx_onoff(int v) { return v ? "On" : "Off"; }
const char* tx_hybrid(int v) { return v ? "On (Recommended)" : "Off"; }   // (0.1.9c: Intel Hybrid CPU Patch)
const char* tx_menu_theme(int v) {
    static const char* t[] = {"OOT Mastersword", "Light", "Dark", "Longshot", "OOT Mastersword (Amber)"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_dlss_model(int v) {
    static const char* t[] = {"K (Transformer)", "M (Transformer 2)", "L (Transformer 2)", "NVIDIA Default"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_aniso(int v) {
    static const char* t[] = {"Game's own", "2x", "4x", "8x", "16x"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_grass(int v) {
    static const char* t[] = {"Default", "1.5x", "2x", "3x"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_hdr(int v) {
    static const char* t[] = {"Off", "Expanded", "Native"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    if (v > 0 && !dev.hdr) {
        static char b[48];
        const char* why = dev.hdr_sdr_why == Device::hdr_sdr_os_off       ? "Windows HDR off"
                        : dev.hdr_sdr_why == Device::hdr_sdr_no_extension ? "no HDR ext"
                        : dev.hdr_sdr_why == Device::hdr_sdr_driver       ? "driver SDR"
                                                                          : "SDR fallback";
        snprintf(b, sizeof b, "%s (%s)", t[v], why);
        return b;
    }
    return t[v];
}
const char* tx_nits(int v) {
    static char b[16];
    snprintf(b, sizeof b, "%d nits", v);
    return b;
}
const char* tx_crt_filter(int v) {
    static const char* t[] = {"Off", "Lottes Aperture", "Lottes TV", "Hyllian", "ZFast"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    if (v > 0 && crt_native_pq()) {
        static char b[48];
        snprintf(b, sizeof b, "%s (off in Native HDR)", t[v]);
        return b;
    }
    return t[v];
}
const char* tx_crt_lines(int v) {
    static const char* t[] = {"240p", "480p", "720p"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_threaded(int v) {
    static const char* t[] = {"Auto", "On", "Off"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_colour_vision(int v) {
    static const char* t[] = {"Off", "Protan (Red-weak)", "Deutan (Green-weak)", "Tritan (Blue-weak)"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_fps_mode(int v) {
    static const char* t[] = {"Off", "FPS", "Frame time", "Graph", "Detailed"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_fps_corner(int v) {
    static const char* t[] = {"Top right", "Top left", "Bottom right", "Bottom left"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_audio_output(int v) {
    static const char* t[] = {"Auto", "Stereo", "5.1 Surround"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_mic(int v) {
    if (v <= 0) return "Off";
    if (v == 1) return "System default";
    const int idx = v - 2;
    if (host && host->mic_count && host->mic_name && idx < host->mic_count()) {
        if (const char* name = host->mic_name(idx)) return name;
    }
    static char b[24];
    snprintf(b, sizeof b, "Microphone %d", idx + 1);
    return b;
}
const char* tx_menu_shortcut(int v) {
    static const char* t[] = {"Insert / F1", "F1 only", "Insert / F1 / Esc"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_pad_menu(int v) { return v ? "Home / Select+Start" : "Select+Start only"; }
const char* tx_rumble(int v) {
    if (v <= 0) return "Off";
    static char b[12];
    snprintf(b, sizeof b, "%d%%", v);
    return b;
}
const char* tx_deadzone(int v) {
    static const char* t[] = {"Off (0%)", "5%", "10%", "15%", "20%", "25%", "30%"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_gyro_deadzone(int v) {
    static const char* t[] = {"Off", "Normal", "High"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_gyro(int v) {
    if (!v) return "Off";
    if (host && host->gyro_status) {
        const int st = host->gyro_status();
        if (st == 0) return "On (no controller)";
        if (st == 1) return "On (no gyro)";
    }
    return "On";
}
const char* tx_prompts(int v) {
    static const char* t[] = {"Auto", "Switch", "Xbox", "PlayStation 4", "PlayStation 5", "Steam Deck"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    if (v == 0 && host && host->prompt_face) {
        const int face = host->prompt_face();
        if (face >= 0 && face < 5) {
            static char b[40];
            snprintf(b, sizeof b, "Auto (%s)", t[face + 1]);
            return b;
        }
    }
    return t[v];
}
const char* tx_present_queue(int v) {
    static const char* t[] = {"Auto", "On", "Off"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_intel_compiles(int v) {
    static const char* t[] = {"Auto", "Parallel", "Serial"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_antilag(int v) {
    if (!v) return "Off";
    return antilag_available() ? "On" : "On (unavailable)";
}
const char* tx_ui_size(int v) {
    static const char* t[] = {"Small", "Medium", "Large"};
    if (v < 1 || v > 3) return t[1];
    return t[v - 1];
}
const char* tx_reflex(int v) {
    static const char* t[] = {"Off", "On", "Boost"};
    return t[std::min(std::max(v, 0), 2)];
}
const char* tx_mousecam(int v) { return v ? "Native" : "As the stick"; }
const char* tx_sens(int v) {
    static char b[12];
    snprintf(b, sizeof b, "%.2f", v / 100.0);
    return b;
}
const char* tx_upscale(int v) {
    static const char* t[] = {"Off", "Catmull-Rom", "Lanczos", "XeSS", "FSR 3.1", "FSR 1.0", "SMRS (Nodix)", "SMAI (Nodix)",
                              "DLSS SR", "FSR 4"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_dlss_quality(int v) {
    static const char* t[] = {"Quality (K)", "Balanced (K)", "Performance (M)", "Ultra Perf (L)", "DLAA (K)"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[1];
    return t[v];
}
const char* tx_fsr4_quality(int v) {
    static const char* t[] = {"Quality", "Balanced", "Performance", "Ultra Performance", "Native AA"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[1];
    return t[v];
}
/// True when Upscaling uses the #2000 Resolution / Native temporal path (DLSS SR or FSR 4).
bool temporal_sr_upscale() { return opts.upscale == 8 || opts.upscale == 9; }
const char* tx_xess_quality(int v) {
    static const char* t[] = {"Native AA", "Ultra Quality+", "Ultra Quality", "Quality", "Balanced", "Performance",
                              "Ultra Performance"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[3];
    return t[v];
}
const char* tx_percent(int v) {
    static char b[8];
    snprintf(b, sizeof b, "%d%%", v);
    return b;
}
const char* tx_signed_percent(int v) {
    static char b[12];
    snprintf(b, sizeof b, "%+d%%", v);
    return b;
}
const char* tx_gamma(int v) {
    static char b[12];
    snprintf(b, sizeof b, "%.2f", v / 100.0);
    return b;
}
const char* tx_color_preset(int v) {
    static const char* t[] = {"Default", "Vivid (OLED)", "Clarity", "Custom"};
    return t[std::min(std::max(v, 0), 3)];
}
// Output the menu sizes are shown for (Output resolution row; applied next start). Handheld uses the game's 1536x864.
struct Out {
    uint32_t w, h;
};
Out monitor_size() {
    uint32_t w = 1920, h = 1080;
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof mi};
    if (GetMonitorInfo(mon, &mi)) {
        int ww = mi.rcMonitor.right - mi.rcMonitor.left;
        int hh = mi.rcMonitor.bottom - mi.rcMonitor.top;
        ww &= ~7;
        hh &= ~7;
        if (ww >= 640 && hh >= 480) w = (uint32_t)ww, h = (uint32_t)hh;
    }
    return {w, h};
}
Out shown_output() {
    if (opts.match_monitor && !opts.handheld) return monitor_size();
    static const Out t[] = {{1920, 1080}, {2560, 1440}, {3840, 2160}};
    return opts.handheld ? Out{1536, 864} : t[std::min(std::max(opts.output_res, 0), 2)];
}
constexpr int kResPercent[] = {0, 100, 75, 67, 50};
// 3D size for a fixed scale of the output, in twelfths (exact at 1920/2560/3840).
int twelfths(float scale) { return std::min(std::max((int)(scale * 12.0f + 0.5f), 1), 12); }
void game_size(float scale, uint32_t& w, uint32_t& h) {
    const Out o = shown_output();
    const int k = twelfths(scale);
    w = (o.w * k + 11) / 12, h = o.h * k / 12;
}
const char* tx_resolution(int v) {
    static char b[56];
    // Under DLSS / FSR 4 (#2000): Resolution is the render-scale slider (Auto / Native / twelfths), not "Set by".
    if (temporal_sr_upscale()) {
        sync_temporal_from_upscale();
        if (opts.dlss_quality == 4 || opts.dlss == upscaler_rows::DLAA || opts.dlss == upscaler_rows::FSR4_AA) {
            uint32_t w, h;
            game_size(1.0f, w, h);
            snprintf(b, sizeof b, opts.upscale == 9 ? "Native AA (%ux%u)" : "Native (DLAA, %ux%u)", w, h);
            return b;
        }
        const int p = upscaler_rows::percent(v);
        if (p > 0) {
            uint32_t w, h;
            game_size(p / 100.0f, w, h);
            if (const char* mode = upscaler_rows::dlss_mode_name(v))
                snprintf(b, sizeof b, "%s %d%% (%ux%u)", mode, p, w, h);
            else
                snprintf(b, sizeof b, "%d%% (%ux%u)", p, w, h);
            return b;
        }
        return "Auto (dynamic)";
    }
    const int up = upscaler_render_percent();
    if (up > 0) {
        uint32_t w, h;
        game_size(up / 100.0f, w, h);
        const char* who = opts.upscale == 3                         ? "XeSS"
                          : opts.upscale == 4 || opts.upscale == 5   ? "FSR"
                          : opts.upscale == 6 || opts.upscale == 7   ? "Nodix"
                                                                     : "Upscaler";
        snprintf(b, sizeof b, "Set by %s (%d%% · %ux%u)", who, up, w, h);
        return b;
    }
    if (v <= 0 || v > 4) return "Auto (dynamic)";
    uint32_t w, h;
    game_size(kResPercent[v] / 100.0f, w, h);
    snprintf(b, sizeof b, "%d%% (%ux%u)", kResPercent[v], w, h);
    return b;
}
const char* tx_output_res(int v) {
    static const char* t[] = {"1920x1080", "2560x1440", "3840x2160"};
    return t[std::min(std::max(v, 0), 2)];
}
const char* tx_speed(int v) { return v ? "Real time" : "Per frame"; }
const char* tx_text_speed(int v) {
    static const char* t[] = {"Normal", "2x", "4x", "Instant"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_fov(int v) { return v == 100 ? "100% (original)" : tx_percent(v); }
const char* tx_handheld(int v) { return v ? "On" : "Off"; }
const char* tx_ultrawide(int v) { return v ? "Monitor's shape" : "16:9"; }
const char* tx_gpu(int v) {
    static char buf[300];
    const int idx = v - 1;
    if (v <= 0) snprintf(buf, sizeof buf, "Auto");
    else if (idx < (int)gpu_device_names.size()) snprintf(buf, sizeof buf, "%d: %s", idx, gpu_device_names[idx].c_str());
    else snprintf(buf, sizeof buf, "%d: Device %d", idx, idx);
    return buf;
}
const char* tx_pad_layout(int v);
const char* tx_robust(int v) {
    static const char* t[] = {"Auto", "Off", "On"};
    return (unsigned)v < 3 ? t[v] : t[0];
}
const char* tx_mesh_mode(int v) {
    static const char* t[] = {"Automatic", "Native", "Compute"};
    return (unsigned)v < 3 ? t[v] : t[0];
}
const char* tx_vsync(int v) {
    if (v == 1) return "On";
    if (v == 2) return "Off";
    return "Mailbox";
}
/* Saved numbers stay put: 1 On, 2 Off, 0 Mailbox (this used to be Auto). The row walks On, Mailbox, Off. */
static int vsync_cycle(int cur, int dir) {
    static const int order[] = {1, 0, 2};
    int i = 0;
    for (int n = 0; n < 3; n++) if (order[n] == cur) { i = n; break; }
    return order[(i + (dir < 0 ? 2 : 1)) % 3];
}
const char* tx_fps_cap(int v) {
    static const char* const t[] = {"60 (the console's)", "90", "120", "144", "165", "240", "Uncapped"};
    return t[v < 0 ? 0 : v > 6 ? 6 : v];
}
const char* tx_preset(int v) {
    static const char* t[] = {"Custom", "Minimum (4GB)", "Low (6GB)", "Balanced (8GB)", "High (12GB)", "Ultra (16GB+)"};
    return t[std::min(std::max(v, 0), 5)];
}
void sync_preset_budgets(int p) {
    switch (p) {
    case 1: opts.vram_mb = 2200;  opts.mirror_mb = 256;  break;
    case 2: opts.vram_mb = 3600;  opts.mirror_mb = 512;  break;
    case 3: opts.vram_mb = 4800;  opts.mirror_mb = 1024; break;
    case 4: opts.vram_mb = 7500;  opts.mirror_mb = 2048; break;
    case 5: opts.vram_mb = 11000; opts.mirror_mb = 3072; break;
    default: opts.vram_mb = 0;    opts.mirror_mb = 0;    break;
    }
}
void apply_color_preset(int p) {
    // STS78721 / TOTK / DKB vivid OLED presets (issue #136)
    static const struct {
        int b, c, s, g;
    } presets[] = {{0, 100, 100, 100}, {0, 115, 135, 95}, {2, 110, 115, 100}};
    if (p < 0 || p > 2) return;
    opts.color_brightness = presets[p].b;
    opts.color_contrast = presets[p].c;
    opts.color_saturation = presets[p].s;
    opts.color_gamma = presets[p].g;
    opts.color_preset = p;
}
void apply_preset(int p) {
    switch (p) {
    case 1: // Minimum (4GB VRAM / iGPU) — Hookshot FG by default; switch engine in the menu for Nodix
        opts.resolution = 4; opts.upscale = 2; opts.sharpness = 50;
        opts.fg_backend = 1; opts.framegen = 2; opts.pace = 1; opts.realtime = 1; opts.handheld = 1;
        break;
    case 2: // Low (6GB VRAM)
        opts.resolution = 2; opts.upscale = 2; opts.sharpness = 35;
        opts.fg_backend = 1; opts.framegen = 2; opts.pace = 1; opts.realtime = 1; opts.handheld = 1;
        break;
    case 3: // Balanced (8GB VRAM)
        opts.resolution = 1; opts.upscale = 1; opts.sharpness = 25;
        opts.fg_backend = 1; opts.framegen = 2; opts.pace = 1; opts.realtime = 1; opts.handheld = 0;
        break;
    case 4: // High (12GB VRAM)
        opts.resolution = 1; opts.upscale = 2; opts.sharpness = 30;
        opts.fg_backend = 1; opts.framegen = 2; opts.pace = 1; opts.realtime = 1; opts.handheld = 0;
        break;
    case 5: // Ultra (16GB+ VRAM)
        opts.resolution = 1; opts.upscale = 2; opts.sharpness = 40;
        opts.fg_backend = 1; opts.framegen = 3; opts.pace = 1; opts.realtime = 1; opts.handheld = 0;
        break;
    default:
        break;
    }
    sync_preset_budgets(p);
}

Row rows[] = {
    // Graphics
    {TAB_GRAPHICS, "Menu theme", &opts.menu_theme, 0, 4, 1, tx_menu_theme,
     "OOT Mastersword: Lost Woods greens with Master Sword blue and gold. Light: Switch settings grey and cyan. "
     "Dark: near-black panels with crimson accents. Longshot: warm bronze, parchment, and gold. "
     "OOT Mastersword (Amber): warm mahogany leather, terracotta copper, and glowing amber accents inspired by the in-game Options screen.",
     true, false},
    {TAB_GRAPHICS, "Menu size", &opts.menu_size, 1, 3, 1, tx_ui_size,
     "Size of the Insert menu and start-up screen (Small, Medium, or Large). Separate from the FPS overlay size.",
     true, false},
    {TAB_GRAPHICS, "Quality preset", &opts.preset, 0, 5, 1, tx_preset,
     "Automatically configures resolution, upscaling, frame generation, and dynamic VRAM limits tailored to your GPU tier."},
    {TAB_GRAPHICS, "Fullscreen", &opts.fullscreen, 0, 1, 1, tx_onoff,
     "Borderless fullscreen at the monitor size; cursor auto-hides. Same as F11. Starts from this setting."},
    {TAB_GRAPHICS, "GPU device", &opts.gpu_device, 0, 8, 1, tx_gpu,
     "Selects which graphics card Vulkan uses. Auto picks the best dedicated GPU. Applies after restart.",
     true, true},
    {TAB_GRAPHICS, "HDR", &opts.hdr, 0, 2, 1, tx_hdr,
     "High Dynamic Range (HDR10 ST.2084) when Windows HDR is active on your display. Expanded lifts SDR highlights cleanly; "
     "Native asks the game for its own 10-bit PQ output. Falls back cleanly to SDR on non-HDR screens.",
     true, false},
    {TAB_GRAPHICS, "HDR paper white", &opts.hdr_white, 80, 400, 10, tx_nits,
     "Brightness of normal white text, UI, and diffuse surfaces in HDR (default 200 nits).",
     true, false},
    {TAB_GRAPHICS, "HDR peak brightness", &opts.hdr_peak, 400, 4000, 100, tx_nits,
     "Maximum highlight brightness for your HDR monitor or TV (default 1000 nits).",
     true, false},
    {TAB_GRAPHICS, "HDR contrast", &opts.hdr_contrast, 50, 200, 5, tx_percent,
     "Luminance contrast around mid-grey with smooth highlight roll-off to peak brightness. 100% leaves HDR tones unchanged.",
     true, false},
    {TAB_GRAPHICS, "FG engine", &opts.fg_backend, 0, 6, 1, tx_fg_backend,
     "MasterSword = Hookshot native FG only when chosen. DLSS MFG / Nodix SMFG / FSR / XeSS / Apex FG (Fast) are "
     "separate engine rows (interpolator stand-in until vendor FG evaluates). The multiplier you pick is the one that is made. Off disables FG. "
     "SMAI is under Upscaling."},
    {TAB_GRAPHICS, "Frame generation", &opts.framegen, 0, 8, 1, tx_framegen,
     "Extra frames between the game's own. MasterSword: 2x/3x/4x/6x/8x. DLSS MFG: 2x/3x/4x/6x. "
     "Nodix SMFG and Apex FG: 2x/3x/4x/5x/6x. XeSS MFG: 2x/3x/4x. FSR 3.1 FG: 2x. Needs an FG engine."},
    {TAB_GRAPHICS, "DLSS FG preset", &opts.framegen_preset, 0, 2, 1, tx_framegen_preset,
     "DLSS MFG model (A/B/C). Shown only when FG engine is DLSS MFG. Streamline DLSS-G evaluate is not wired yet."},
    {TAB_GRAPHICS, "Flow scale", &opts.fg_flow, 10, 100, 5, tx_percent,
     "Nodix Flow / motion strength (shown with SMFG). Higher follows fast camera moves more aggressively."},
    {TAB_GRAPHICS, "Frame pacing", &opts.pace, 0, 1, 1, tx_onoff,
     "Shows each frame at an even interval instead of as soon as it is ready. Less stutter, at up to one frame of delay (10 ms at most without Frame generation; not with Low latency on)."},
    {TAB_GRAPHICS, "Mastersword Pacer (Experimental)", &opts.ms_pacer, 0, 1, 1, tx_onoff,
     "Experimental. Holds the game's own frames to an even beat. A frame more than 2 ms late starts a new beat, so the next one is not held for a full extra frame. Off uses the previous start-time cap."},
    {TAB_GRAPHICS, "Intel Hybrid CPU Patch", &opts.hybrid_cpu, 0, 1, 1, tx_hybrid,
     "Recommended for Intel hybrid CPUs: 12th, 13th and 14th gen Core and Core Ultra. Runs the game's threads on the "
     "performance cores (P-cores) and background work such as shader building on the efficiency cores (E-cores), like the "
     "Switch 2's six identical cores. Steadier frames and fewer glitches. No effect on CPUs with one kind of core (most AMD). "
     "Applies after a restart.",
     true, true},
    {TAB_GRAPHICS, "V-Sync", &opts.vsync, 0, 2, 1, tx_vsync,
     "On waits for the display refresh (FIFO). Mailbox shows the latest finished frame on the next refresh, without tearing. "
     "Off presents immediately and can tear. Changes at once. RX_VK_FIFO / RX_VK_IMMEDIATE still choose for a run."},
    {TAB_GRAPHICS, "NVIDIA Reflex", &opts.reflex, 0, 2, 1, tx_reflex,
     "Low-latency pacing on NVIDIA GPUs (VK_NV_low_latency2). Boost also raises GPU clocks. No effect on AMD/Intel."},
    {TAB_GRAPHICS, "AMD Anti-Lag 2", &opts.antilag, 0, 1, 1, tx_antilag,
     "Low-latency pacing on AMD Radeon GPUs (VK_AMD_anti_lag). Reduces input latency when GPU-bound; inactive on non-AMD GPUs.",
     true, false},
    {TAB_GRAPHICS, "Resolution", &opts.resolution, 0, 4, 1, tx_resolution,
     "Internal 3D size as a percent of Output resolution (like a PC title). Auto lets the game lower it when the GPU is busy. "
     "With XeSS/FSR/Nodix selected, their quality row sets this live (e.g. FSR Quality = 67% of 1440p/4K)."},
    {TAB_GRAPHICS, "Output resolution", &opts.output_res, 0, 2, 1, tx_output_res,
     "The size the game draws at. 1080p, 1440p, or 4K. Applies after a restart. Match monitor overrides this.",
     false, true},
    {TAB_GRAPHICS, "Match monitor", &opts.match_monitor, 0, 1, 1, tx_onoff,
     "Draws at this screen's size. Each side is rounded down to a multiple of 8, so a 1366x768 screen becomes 1360x768. "
     "Applies after a restart. Off uses Output resolution."},
    {TAB_GRAPHICS, "Upscaling", &opts.upscale, 0, 9, 1, tx_upscale,
     "Scales the internal 3D up to Output resolution (1080p / 1440p / 4K). Off leaves the game's TAAU. Catmull-Rom / Lanczos "
     "are spatial filters. XeSS, FSR 3.1/1.0, SMRS, SMAI, DLSS SR and FSR 4 each show a quality row that sets render scale. "
     "DLSS needs nvngx_dlss.dll in ngx/; FSR 4 needs amd_fidelityfx_*_vk.dll beside the exe (AMD FSR SDK 2.4)."},
    {TAB_GRAPHICS, "XeSS quality", &opts.xess_quality, 0, 6, 1, tx_xess_quality,
     "XeSS quality: sets 3D render scale of Output (Native AA 100% … Quality ~59% … Ultra Performance 25%)."},
    {TAB_GRAPHICS, "FSR quality", &opts.fsr_quality, 0, 7, 1, tx_fsr_quality,
     "FSR render scale of Output: Native AA 100%, 92%, 83%, 75%, then AMD Quality 67% / Balanced 58% / Performance 50% / Ultra 33%."},
    {TAB_GRAPHICS, "Nodix quality", &opts.nodix_quality, 0, 2, 1, tx_nodix_quality,
     "Nodix quality: sets 3D render scale of Output (Quality 67%, Balanced 58%, Performance 50%)."},
    {TAB_GRAPHICS, "DLSS quality", &opts.dlss_quality, 0, 4, 1, tx_dlss_quality,
     "DLSS SR quality (transformer presets): Quality/Balanced/DLAA use preset K, Performance uses M, Ultra Performance uses L. "
     "Also sets 3D render scale of Output."},
    {TAB_GRAPHICS, "DLSS model", &opts.dlss_model, 0, 3, 1, tx_dlss_model,
     "Chooses which NVIDIA DLSS neural network preset runs (K Transformer default, M/L Transformer Gen 2, or NVIDIA Default).",
     true, false},
    {TAB_GRAPHICS, "FSR 4 quality", &opts.dlss_quality, 0, 4, 1, tx_fsr4_quality,
     "FSR 4 quality: same render-scale shares as DLSS (Quality 67% … Ultra Performance 33%), plus Native AA at full Output. "
     "Needs AMD's FSR 4 runtime and a GPU AMD's library lists an FSR 4 model for."},
    {TAB_GRAPHICS, "Upscaler sharpness", &opts.up_sharpness, 0, 100, 10, tx_percent,
     "Contrast-adaptive sharpening applied right after DLSS, FSR 3.1, or FSR 4 — before the HUD is drawn, so UI text stays clean.",
     true, false},
    {TAB_GRAPHICS, "SMAA", &opts.smaa, 0, 1, 1, tx_onoff,
     "Subpixel Morphological Anti-Aliasing on the present (iryoku SMAA Medium). Softens shimmering edges after the game "
     "draws, before upscaling. The game's own title SMAA is separate."},
    {TAB_GRAPHICS, "Sharpness", &opts.sharpness, 0, 100, 10, tx_percent,
     "Sharpens the picture as it goes to the window, with or without an upscaling filter. Too much shows halos around edges."},
    {TAB_GRAPHICS, "Anisotropic filtering", &opts.anisotropy, 0, 4, 1, tx_aniso,
     "Sharpens ground, road, and wall textures viewed at shallow angles. Applies live without a restart.",
     true, false},
    {TAB_GRAPHICS, "Grass distance", &opts.grass_distance, 0, 3, 1, tx_grass,
     "Pushes out the fade-out distance of instanced grass and foliage in open areas (1.5x, 2x, or 3x). Applies live.",
     true, false},
    {TAB_GRAPHICS, "CRT filter", &opts.crt_filter, 0, 4, 1, tx_crt_filter,
     "Retro CRT tube simulation (scanlines and phosphor mask) applied after scaling and before menus. Disabled automatically in Native HDR.",
     true, false},
    {TAB_GRAPHICS, "CRT lines", &opts.crt_lines, 0, 2, 1, tx_crt_lines,
     "Virtual scanline resolution for the CRT filter (240p classic, 480p EDTV, or 720p crisp scanlines; clamped to half output height).",
     true, false},
    {TAB_GRAPHICS, "Threaded rendering", &opts.threaded_rendering, 0, 2, 1, tx_threaded,
     "Runs Vulkan command replay on a dedicated render thread for steadier frame pacing. Auto enables on 6+ core CPUs. Applies after a restart.",
     true, true},
    {TAB_GRAPHICS, "Handheld profile", &opts.handheld, 0, 1, 1, tx_handheld,
     "Runs the game as if the console were undocked: lower resolutions, lighter on the GPU. Applies after a restart.",
     false, true},
    {TAB_GRAPHICS, "Ultrawide", &opts.ultrawide, 0, 1, 1, tx_ultrawide,
     "Monitor's shape widens the camera to fill a screen wider than 16:9. The HUD is still stretched. Applies after a restart.",
     false, true},
    {TAB_GRAPHICS, "Robust buffers", &opts.robust, 0, 2, 1, tx_robust,
     "Bounds-checked buffer reads: a read past a buffer's end is dropped instead of stopping the GPU. Auto is on, on every card; "
     "Off can be faster on some NVIDIA cards. Applies after restart.",
     false, true},
    {TAB_GRAPHICS, "Mesh shaders", &opts.mesh_mode, 0, 2, 1, tx_mesh_mode,
     "How the game's mesh programs (much of the scenery) run: on the GPU's own mesh shaders, or as compute shaders, which "
     "every GPU has. Automatic is the old behaviour. Applies after a restart.",
     false, true},
    {TAB_GRAPHICS, "Quit to Desktop", nullptr, 0, 0, 0, nullptr,
     "Saves options and closes MasterSword. Enter or the bottom face button quits."},

    // Gameplay
    {TAB_GAMEPLAY, "FPS counter", &opts.show_fps, 0, 4, 1, tx_fps_mode,
     "On-screen performance overlay: Off, compact FPS, Frame time + 1% lows, real-time Frame-time Graph, or Detailed (VRAM, active tech, GPU).",
     true, false},
    {TAB_GAMEPLAY, "FPS corner", &opts.fps_corner, 0, 3, 1, tx_fps_corner,
     "Which corner of the screen displays the FPS counter.",
     true, false},
    {TAB_GAMEPLAY, "Overlay size", &opts.overlay_size, 1, 3, 1, tx_ui_size,
     "Size of the in-game FPS overlay (Small, Medium, or Large). Separate from the Insert menu size.",
     true, false},
    {TAB_GAMEPLAY, "Game speed", &opts.realtime, 0, 1, 1, tx_speed,
     "Per frame: the game slows down whenever it runs below 60 fps. Real time: it keeps normal speed down to 15 fps by taking "
     "longer steps."},
    {TAB_GAMEPLAY, "Game frame rate cap", &opts.fps_cap, 0, 6, 1, tx_fps_cap,
     "Caps the game's own frames (not made ones). Uncapped: as fast as the PC can, still at real time; with V-Sync on, the display's "
     "refresh rate is the limit. Applies after a restart.",
     false, true},
    {TAB_GAMEPLAY, "Field of view", &opts.fov, 50, 150, 5, tx_fov,
     "How much of the world you see: 100% is the original view. Higher is wider, lower is closer. Changes apply at once "
     "during gameplay. Cutscenes keep their own view."},
    {TAB_GAMEPLAY, "Text speed", &opts.text_speed, 0, 3, 1, tx_text_speed,
     "How fast the letters of dialogue and other messages appear. Normal is the game's own pace, with its pauses. Instant shows "
     "each page at once, as pressing A does."},
    {TAB_GAMEPLAY, "Audio output", &opts.audio_output, 0, 2, 1, tx_audio_output,
     "Audio channel layout handed to the game (Auto matches Windows, Stereo forces 2.0, 5.1 forces surround). Applies after a restart.",
     true, true},
    {TAB_GAMEPLAY, "Microphone", &opts.mic, 0, 16, 1, tx_mic,
     "Microphone device for in-game audio input (such as Ocarina mic support). Changes apply immediately.",
     true, false},
    {TAB_GAMEPLAY, "Discord status", &opts.discord, 0, 1, 1, tx_onoff,
     "Shares your play session via Discord Rich Presence while Discord is running.",
     true, false},
    {TAB_GAMEPLAY, "Menu shortcut", &opts.menu_shortcut, 0, 2, 1, tx_menu_shortcut,
     "Which keyboard keys open this menu. Esc always closes the menu when open.",
     true, false},
    {TAB_GAMEPLAY, "Controller menu button", &opts.pad_menu, 0, 1, 1, tx_pad_menu,
     "Allows the controller Home / Guide button to open this menu. Select + Start always opens it as a fallback.",
     true, false},
    {TAB_GAMEPLAY, "Quit to Desktop", nullptr, 0, 0, 0, nullptr,
     "Saves options and closes MasterSword. Enter or the bottom face button quits."},

    // Colour
    {TAB_COLOUR, "Colour profile", &opts.color_preset, 0, 3, 1, tx_color_preset,
     "Sets the four values below. Vivid: more contrast and saturation, shadows opened a little (made for OLED screens). "
     "Clarity: a little more contrast and saturation. Moving a slider switches to Custom."},
    {TAB_COLOUR, "Brightness", &opts.color_brightness, -30, 30, 1, tx_signed_percent,
     "Adds or subtracts overall brightness after the game draws. 0% leaves the picture unchanged."},
    {TAB_COLOUR, "Contrast", &opts.color_contrast, 70, 150, 1, tx_percent,
     "Stretch around mid-grey. 100% is unchanged; higher pops highlights and shadows."},
    {TAB_COLOUR, "Saturation", &opts.color_saturation, 50, 200, 5, tx_percent,
     "Colour intensity. 100% is unchanged; higher is more vivid."},
    {TAB_COLOUR, "Gamma", &opts.color_gamma, 80, 120, 1, tx_gamma,
     "Power curve after the other three; 1.00 leaves it. Lower opens shadows (Vivid uses 0.95)."},
    {TAB_COLOUR, "Colour vision", &opts.colour_vision, 0, 3, 1, tx_colour_vision,
     "Luminance-preserving Daltonization filter for colour-vision deficiency (Protan, Deutan, or Tritan). Works in both SDR and HDR.",
     true, false},

    // Controls
    {TAB_CONTROLS, "Controller in background", &opts.pad_background, 0, 1, 1, tx_onoff,
     "On: the controller keeps playing the game while another window has the focus, for streaming or playing beside other "
     "apps. The keyboard and mouse still need the game's window."},
    {TAB_CONTROLS, "Mouse camera", &opts.mouse_native, 0, 1, 1, tx_mousecam,
     "Native: the captured mouse turns the camera itself, one to one, as in a PC game. As the stick: the mouse stands in for "
     "the right stick, with its curve and top speed."},
    {TAB_CONTROLS, "Mouse sensitivity", &opts.mouse_sens, 10, 500, 10, tx_sens,
     "How far the camera turns for a given mouse movement, for either Mouse camera. At 1.00 native look turns a tenth of a "
     "degree a count. The - and = keys change it in the game too."},
    {TAB_CONTROLS, "Disable camera centering", &opts.disable_camera_centering, 0, 1, 1, tx_onoff,
     "Stops the camera from automatically pulling back behind Link while moving in Native mouse camera mode.",
     true, false},
    {TAB_CONTROLS, "Left stick speed", &opts.stick_l_sens, 10, 500, 10, tx_sens,
     "How far the left analog stick pushes movement for a given deflection. 100% is stock. Keyboard WASD is unchanged."},
    {TAB_CONTROLS, "Right stick speed", &opts.stick_r_sens, 10, 500, 10, tx_sens,
     "How far the right analog stick turns the camera for a given deflection. 100% is stock. Mouse look uses Mouse sensitivity."},
    {TAB_CONTROLS, "Rumble strength", &opts.rumble, 0, 150, 25, tx_rumble,
     "Controller vibration and HD rumble intensity (0% Off up to 150%). Changes apply immediately.",
     true, false},
    {TAB_CONTROLS, "Stick dead zone", &opts.stick_dead_zone, 0, 6, 1, tx_deadzone,
     "Inner dead zone for analog sticks (0% to 30%) to stop stick drift. Changes apply immediately.",
     true, false},
    {TAB_CONTROLS, "Invert mouse vertical", &opts.mouse_invert, 0, 1, 1, tx_onoff,
     "Swaps up and down for the mouse: moving it forward tilts the camera down."},
    {TAB_CONTROLS, "Invert camera X", &opts.invert_cam_x, 0, 1, 1, tx_onoff,
     "Inverts horizontal look (left/right) on the right analog stick."},
    {TAB_CONTROLS, "Invert camera Y", &opts.invert_cam_y, 0, 1, 1, tx_onoff,
     "Inverts vertical look (up/down) on the right analog stick."},
    {TAB_CONTROLS, "Motion controls", &opts.gyro, 0, 1, 1, tx_gyro,
     "Uses your controller's gyroscope for aiming (bow, slingshot, hookshot) when a gyro-capable gamepad is connected.",
     true, false},
    {TAB_CONTROLS, "Motion sensitivity X", &opts.gyro_sens_x, 10, 300, 10, tx_percent,
     "Horizontal gyroscope aiming sensitivity (100% is default).",
     true, false},
    {TAB_CONTROLS, "Motion sensitivity Y", &opts.gyro_sens_y, 10, 300, 10, tx_percent,
     "Vertical gyroscope aiming sensitivity (100% is default).",
     true, false},
    {TAB_CONTROLS, "Invert motion X", &opts.gyro_inv_x, 0, 1, 1, tx_onoff,
     "Inverts horizontal gyroscope aiming.",
     true, false},
    {TAB_CONTROLS, "Invert motion Y", &opts.gyro_inv_y, 0, 1, 1, tx_onoff,
     "Inverts vertical gyroscope aiming.",
     true, false},
    {TAB_CONTROLS, "Motion deadzone", &opts.gyro_deadzone, 0, 2, 1, tx_gyro_deadzone,
     "Filters out small hand tremors during gyroscope aiming.",
     true, false},
    {TAB_CONTROLS, "Button prompts", &opts.prompts, 0, 5, 1, tx_prompts,
     "Controller button icons shown in-game (Auto matches your connected pad, or pick Switch/Xbox/PS4/PS5/Steam Deck). Applies after a restart.",
     true, true},
    {TAB_CONTROLS, "Layout preset", &opts.pad_layout, 0, 3, 1, tx_pad_layout,
     "Nintendo (default Switch order), Xbox / PC (A for action/talk), or Swap Face. Individual bindings switch this to Custom."},
    {TAB_CONTROLS, "Controls", nullptr, 0, 0, 0, nullptr,   // (hookshot: opens its page)
     "Which gamepad button and which keys press each of the Switch's buttons."},

    // Debug
    {TAB_DEBUG, "ASTC as RGBA8", &opts.astc_rgba8, 0, 1, 1, tx_onoff,
     "Decodes ASTC textures to uncompressed RGBA8 instead of BC3/BC7. Uses more VRAM; useful if a driver shows texture compression artefacts. Applies after a restart.",
     true, true},
    {TAB_DEBUG, "Present queue", &opts.present_queue, 0, 2, 1, tx_present_queue,
     "Controls whether swapchain presents use a dedicated compute queue when available to reduce main-queue stalls. Applies after a restart.",
     true, true},
    {TAB_DEBUG, "Intel shader compiles", &opts.intel_compiles, 0, 2, 1, tx_intel_compiles,
     "Controls shader compilation serialization on Intel GPUs (Auto serializes on Intel drivers to prevent driver crashes; Parallel runs multi-threaded). Applies after a restart.",
     true, true},
    {TAB_DEBUG, "Heart protection", &opts.cheat_hearts, 0, 1, 1, tx_onoff,
     "Session-only debug toggle (not saved): prevents Link's hearts from dropping to zero.",
     true, false},
    {TAB_DEBUG, "No-clip flight", &opts.cheat_flight, 0, 1, 1, tx_onoff,
     "Session-only debug toggle (not saved): lets Link fly freely through geometry.",
     true, false},
    {TAB_DEBUG, "Free camera", &opts.cheat_camera, 0, 1, 1, tx_onoff,
     "Session-only debug toggle (not saved): detaches the camera to explore scenes freely.",
     true, false},
    {TAB_DEBUG, "Quit to Desktop", nullptr, 0, 0, 0, nullptr,
     "Saves options and closes MasterSword. Enter or the bottom face button quits."},
};
constexpr int kRows = sizeof rows / sizeof rows[0];

int cur_tab = TAB_GRAPHICS;
int tab_selected[TAB_COUNT] = {0, 0, 0, 0, 0};

/// A Frame generation step this engine shows. Off, then 2 up to opts_fg_mult_max().
/// 5x is a Nodix and Apex step (both publish ×2–×6). MasterSword and the others skip 5 and 7.
static bool fg_step_ok(int v) {
    if (v == 0) return true;
    const int hi = opts_fg_mult_max();
    if (v < 2 || v > hi || v == 7) return false;
    if (v == 5 && opts.fg_backend != 2 && opts.fg_backend != 6) return false;
    return true;
}

/// Snap a stepped value onto the next step this engine allows (dir > 0 walks up, wrapping through Off).
int snap_framegen(int v, int dir) {
    const int step = dir < 0 ? -1 : 1;
    for (int n = 0; n < 9; n++) {
        if (v > 8) v = 0;
        if (v < 0) v = 8;
        if (fg_step_ok(v)) return v;
        v += step;
    }
    return 0;
}

void clamp_framegen_steps() {
    if (opts.fg_backend == 0) {
        opts.framegen = 0;
        return;
    }
    if (fg_step_ok(opts.framegen)) return;
    // An old ini, or a higher step from another engine: the highest legal step at or below it.
    for (int v = opts.framegen; v >= 2; v--)
        if (fg_step_ok(v)) {
            opts.framegen = v;
            return;
        }
    opts.framegen = 2;
}

/// Sub-options only appear for the engine / upscaler / mode they belong to (keeps tabs readable).
bool row_visible(const Row& r) {
    if (r.value == &opts.hdr_white || r.value == &opts.hdr_peak || r.value == &opts.hdr_contrast) return opts.hdr != 0;
    if (r.value == &opts.framegen) return opts.fg_backend > 0;                 // multiplier only with an FG engine
    // DLSS FG preset (A/B/C): hide for MasterSword / SMFG / FSR / XeSS / Apex — DLSS MFG only.
    if (r.value == &opts.framegen_preset || !strcmp(r.name, "DLSS FG preset")) return opts.fg_backend == 3;
    if (r.value == &opts.fg_flow) return opts.fg_backend == 2;                 // Nodix Flow: SMFG only
    if (r.value == &opts.xess_quality) return opts.upscale == 3;               // XeSS quality ↔ XeSS
    if (r.value == &opts.fsr_quality) return opts.upscale == 4 || opts.upscale == 5;
    if (r.value == &opts.nodix_quality) return opts.upscale == 6 || opts.upscale == 7;
    if (r.value == &opts.dlss_quality) {
        if (!strcmp(r.name, "FSR 4 quality")) return opts.upscale == 9;
        return opts.upscale == 8;   // DLSS quality
    }
    if (r.value == &opts.dlss_model) return opts.upscale == 8;
    if (r.value == &opts.up_sharpness) return opts.upscale == 8 || opts.upscale == 9 || opts.upscale == 4;
    if (r.value == &opts.crt_lines) return opts.crt_filter != 0;
    if (r.value == &opts.fps_corner) return opts.show_fps > 0;
    if (r.value == &opts.gyro_sens_x || r.value == &opts.gyro_sens_y || r.value == &opts.gyro_inv_x ||
        r.value == &opts.gyro_inv_y || r.value == &opts.gyro_deadzone)
        return opts.gyro != 0;
    return true;
}

int tab_row_count(int t) {
    int c = 0;
    for (int i = 0; i < kRows; i++)
        if (rows[i].tab == (MenuTab)t && row_visible(rows[i])) c++;
    return c;
}
int tab_row_to_global(int t, int local_idx) {
    int count = 0;
    for (int i = 0; i < kRows; i++) {
        if (rows[i].tab == (MenuTab)t && row_visible(rows[i])) {
            if (count == local_idx) return i;
            count++;
        }
    }
    return 0;
}

void clamp_tab_selection(int t) {
    const int n = tab_row_count(t);
    if (n <= 0) tab_selected[t] = 0;
    else if (tab_selected[t] >= n) tab_selected[t] = n - 1;
}

// Menu and overlay sizes use opts.menu_size / opts.overlay_size (1 Small, 2 Medium, 3 Large) via ui::unit.
constexpr int kControlsSlot = TAB_COUNT;
std::atomic<bool> open_now{false};
bool menu_refresh = false;
int page = 0;            // 0 options, 1 Controls bindings page, 5 Restart Needed dialog
int csel = 0, ccol = 0;  // Controls row (kControls = Reset) and column (menu_hit::CtlColumn)
int listening = 0;       // waiting for new input: 1 gamepad, 2 keyboard
bool listen_armed = false;
int64_t listen_end = 0;

// ---- (longshot/tock) mouse hit-test, smooth scroll, gamepad prompt dot, restart tracking ----
menu_hit::Map hits;
int hover_tab = -1;
int drag_row = -1;
bool mouse_read = false;
float scroll_at[TAB_COUNT + 1] = {};
float scroll_want[TAB_COUNT + 1] = {};
int scrolled_to[TAB_COUNT + 1] = {-1, -1, -1, -1, -1, -1};
int wheel_rows[TAB_COUNT + 1] = {};
float ease_step = 1.0f;
bool pad_last = false;
int pad_confirm = 3, pad_back = 1;
int ask_sel = 0;
float menu_ox = 0;
std::atomic<float> startup_fade{0.0f};

namespace mr = menu_restart;
mr::State restart_state;
int restart_slot[kRows];
const char* restart_names[mr::kMax];

void restart_init() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int i = 0; i < kRows; i++) {
        restart_slot[i] = rows[i].restart && rows[i].value ? restart_state.add(*rows[i].value) : -1;
        if (restart_slot[i] >= 0) restart_names[restart_slot[i]] = rows[i].name;
    }
}
int restart_slot_of(const Row& r) { return restart_slot[&r - rows]; }
bool restart_changed(const Row& r) {
    const int slot = restart_slot_of(r);
    return r.value && r.restart && slot >= 0 && *r.value != restart_state.start[slot];
}
void restart_values(int* now) {
    for (int i = 0; i < kRows; i++)
        if (restart_slot[i] >= 0) now[restart_slot[i]] = *rows[i].value;
}
unsigned restart_pending() {
    int now[mr::kMax];
    restart_values(now);
    return restart_state.pending(now);
}
bool restart_ask() {
    int now[mr::kMax];
    restart_values(now);
    return app_window() && restart_state.ask(now);
}
std::string restart_list(const char* last) {
    return mr::names(restart_names, restart_state.n, restart_pending(), ", ", last);
}

bool is_slider(const Row& r) {
    return r.value == &opts.sharpness || r.value == &opts.up_sharpness || r.value == &opts.mouse_sens ||
           r.value == &opts.stick_l_sens || r.value == &opts.stick_r_sens || r.value == &opts.fg_flow ||
           r.value == &opts.fov || r.value == &opts.color_brightness || r.value == &opts.color_contrast ||
           r.value == &opts.color_saturation || r.value == &opts.color_gamma ||
           r.value == &opts.hdr_white || r.value == &opts.hdr_peak || r.value == &opts.hdr_contrast ||
           r.value == &opts.rumble || r.value == &opts.gyro_sens_x || r.value == &opts.gyro_sens_y ||
           (r.value && r.text == tx_percent && r.hi - r.lo >= 50 && r.step >= 5);
}
bool stops_at_ends(const Row& r) {
    return is_slider(r) || r.value == &opts.mouse_sens || r.value == &opts.stick_l_sens ||
           r.value == &opts.stick_r_sens || r.value == &opts.fov;
}
bool is_session_only(const Row& r) {
    return r.value == &opts.cheat_hearts || r.value == &opts.cheat_flight || r.value == &opts.cheat_camera;
}
void mic_sync_hi() {
    const int hi = host && host->mic_count ? std::min(16, 1 + std::max(0, host->mic_count())) : 1;
    for (Row& r : rows)
        if (r.value == &opts.mic) r.hi = hi;
    if (opts.mic > hi) opts.mic = hi >= 1 ? 1 : 0;
}
void mic_apply() {
    if (!host || !host->mic_select) return;
    if (opts.mic <= 0) {
        host->mic_select(0, "", -1);
    } else if (opts.mic == 1) {
        host->mic_select(1, "", -1);
    } else {
        const int idx = opts.mic - 2;
        const int cnt = host->mic_count ? host->mic_count() : 0;
        const char* name = (idx < cnt && host->mic_name) ? host->mic_name(idx) : "";
        host->mic_select(2, name ? name : "", idx < cnt ? idx : -1);
    }
}

// (a value too long for its row, a GPU's name: the selected row's slides through it, a character a step, resting at
// each end; the other rows end it with "...". paint_menu says how far it can slide, menu_tick moves it)
int marquee_pos = 0, marquee_max = 0;
int64_t marquee_next = 0;
constexpr int64_t kMarqueeStep = 150000000LL, kMarqueeRest = 1200000000LL;   // (ns)
std::string ini_path() {
    char exe[MAX_PATH];
    const DWORD n = game_module_path(exe, MAX_PATH);
    std::string dir(exe, n);
    return dir.substr(0, dir.find_last_of("\\/") + 1) + "mastersword_options.ini";
}
const char* ini_key(const Row& r) {
    if (r.value == &opts.output_res) return "output_res";   // host/nn_applet.c reads this at start
    if (r.value == &opts.match_monitor) return "match_monitor";
    if (r.value == &opts.reflex) return "reflex";           // launcher Low-Latency Mode (boost/on/off or 0/1/2)
    if (r.value == &opts.vsync) return "vsync";
    if (r.value == &opts.fps_cap) return "frame_rate_cap";  // host/nn_vi.c
    if (r.value == &opts.ms_pacer) return "ms_pacer";       // Mastersword Pacer, host/nn_vi.c
    if (r.value == &opts.hybrid_cpu) return "hybrid_cpu";   // Intel Hybrid CPU Patch, host/hos_thread.c reads it at start
    if (r.value == &opts.mesh_mode) return "mesh_shaders";  // gpu/vk/vk_main.cpp reads at device create
    if (r.value == &opts.framegen_preset) return "fg_model"; // stable key (row renamed to DLSS FG preset)
    if (r.value == &opts.prompts) return "button_prompts";   // host/prompts.c reads this at start
    if (r.value == &opts.audio_output) return "audio_output"; // host/nn_audio.c reads this at start
    if (r.value == &opts.rumble) return "rumble_strength";
    if (r.value == &opts.stick_dead_zone) return "stick_dead_zone";
    if (r.value == &opts.up_sharpness) return "upscaler_sharpness";
    if (r.value == &opts.antilag) return "amd_antilag";
    static char k[48];
    size_t n = 0;
    for (const char* p = r.name; *p && n + 1 < sizeof k; p++) k[n++] = *p == ' ' ? '_' : (char)tolower(*p);
    k[n] = 0;
    return k;
}
// (hookshot) the controls, kept by the host (host/nn_hid.c reads them back from here): pad_<button>, key_<button>
const char* const bind_names[HOS_KEY_N] = {"a", "b", "x", "y", "ls", "rs", "l", "r", "zl", "zr", "plus", "minus",
                                           "left", "up", "right", "down", "move_up", "move_down", "move_left",
                                           "move_right", "cam_up", "cam_down", "cam_left", "cam_right"};
bool can_bind() { return host && host->binding && host->bind && host->pad_raw; }
const char* tx_pad_layout(int v) {
    static const char* t[] = {"Nintendo (Def)", "Xbox / PC", "Swap Face", "Custom"};
    return t[std::min(std::max(v, 0), 3)];
}
void apply_pad_layout(int layout) {
    if (!can_bind()) return;
    if (layout == 0) { // Nintendo (Default: Bottom=B, Right=A, Top=X, Left=Y)
        host->bind(HOS_BIND_PAD, HOS_SW_A, 0, HOS_PAD_EAST);
        host->bind(HOS_BIND_PAD, HOS_SW_B, 0, HOS_PAD_SOUTH);
        host->bind(HOS_BIND_PAD, HOS_SW_X, 0, HOS_PAD_NORTH);
        host->bind(HOS_BIND_PAD, HOS_SW_Y, 0, HOS_PAD_WEST);
    } else if (layout == 1) { // Xbox / PC Standard (Bottom=A, Right=B, Left=X, Top=Y)
        host->bind(HOS_BIND_PAD, HOS_SW_A, 0, HOS_PAD_SOUTH);
        host->bind(HOS_BIND_PAD, HOS_SW_B, 0, HOS_PAD_EAST);
        host->bind(HOS_BIND_PAD, HOS_SW_X, 0, HOS_PAD_WEST);
        host->bind(HOS_BIND_PAD, HOS_SW_Y, 0, HOS_PAD_NORTH);
    } else if (layout == 2) { // Swap Face Buttons (Bottom=A, Right=B, Top=X, Left=Y)
        host->bind(HOS_BIND_PAD, HOS_SW_A, 0, HOS_PAD_SOUTH);
        host->bind(HOS_BIND_PAD, HOS_SW_B, 0, HOS_PAD_EAST);
        host->bind(HOS_BIND_PAD, HOS_SW_X, 0, HOS_PAD_NORTH);
        host->bind(HOS_BIND_PAD, HOS_SW_Y, 0, HOS_PAD_WEST);
    }
}
int detect_pad_layout() {
    if (!can_bind()) return opts.pad_layout;
    const int a = host->binding(HOS_BIND_PAD, HOS_SW_A, 0);
    const int b = host->binding(HOS_BIND_PAD, HOS_SW_B, 0);
    const int x = host->binding(HOS_BIND_PAD, HOS_SW_X, 0);
    const int y = host->binding(HOS_BIND_PAD, HOS_SW_Y, 0);
    if (a == HOS_PAD_EAST && b == HOS_PAD_SOUTH && x == HOS_PAD_NORTH && y == HOS_PAD_WEST) return 0;
    if (a == HOS_PAD_SOUTH && b == HOS_PAD_EAST && x == HOS_PAD_WEST && y == HOS_PAD_NORTH) return 1;
    if (a == HOS_PAD_SOUTH && b == HOS_PAD_EAST && x == HOS_PAD_NORTH && y == HOS_PAD_WEST) return 2;
    return 3;
}
bool is_menu_ini_key(const char* key) {
    if (!key || !*key) return false;
    if (!strcmp(key, "options_version") || !strcmp(key, "fg_backend") || !strcmp(key, "fg_engine") ||
        !strcmp(key, "framegen_preset") || !strcmp(key, "frame_gen_preset") || !strcmp(key, "dynamic_resolution"))
        return true;
    for (const Row& r : rows)
        if (r.value && !strcmp(key, ini_key(r))) return true;
    return false;
}
void save() {
    // Keep launcher / foreign lines (Reflex, window_size, sectioned keys, …). Only rewrite Insert-menu rows and
    // pad_/key_ binds — rewriting the whole file used to drop MasterSwordLauncher settings on every menu close.
    std::string kept;
    if (FILE* f = fopen(ini_path().c_str(), "r")) {
        char line[256];
        while (fgets(line, sizeof line, f)) {
            char key[80];
            const char* eq = strchr(line, '=');
            if (!eq) {
                kept += line;
                continue;
            }
            size_t n = 0;
            for (const char* p = line; p < eq && n + 1 < sizeof key; p++)
                if (*p != ' ' && *p != '\t') key[n++] = (char)tolower(*p);
            key[n] = 0;
            if (!strncmp(key, "pad_", 4) || !strncmp(key, "key_", 4)) {
                if (!can_bind()) kept += line;   // host will rewrite binds when the Controls page is live
                continue;
            }
            if (is_menu_ini_key(key)) continue;
            kept += line;
        }
        fclose(f);
    }
    if (FILE* f = fopen(ini_path().c_str(), "w")) {
        for (const Row& r : rows)
            if (r.value && !is_session_only(r)) fprintf(f, "%s=%d\n", ini_key(r), *r.value);
        // (2: game_speed below 2 was only ever the old default, see menu_init; 3: the mouse's buttons are bindings, see
        // host/nn_hid.c binds_load)
        // (v7: SMAI moved to Upscaling; FG is Off/Hookshot/SMFG/DLSS/FSR/XeSS. v6 Hookshot=1. v5 colour.)
        fprintf(f, "options_version=9\n");
        for (int i = 0; i < HOS_KEY_N && can_bind(); i++) {   // (the keys-only controls, HOS_KEY_*, have no pad_ line)
            if (i < HOS_SW_N) fprintf(f, "pad_%s=%d\n", bind_names[i], host->binding(HOS_BIND_PAD, i, 0));
            fprintf(f, "key_%s=%d,%d\n", bind_names[i], host->binding(HOS_BIND_KEY, i, 0), host->binding(HOS_BIND_KEY, i, 1));
        }
        fputs(kept.c_str(), f);
        fclose(f);
    }
}

// ---- (hookshot) the Controls page: what presses each of the Switch's buttons, a gamepad input and up to two keys ----
struct Control {
    int button;   // HOS_SW_*, or HOS_KEY_* (keys only)
    const char* name;
};
const Control controls[] = {{HOS_SW_A, "A"}, {HOS_SW_B, "B"}, {HOS_SW_X, "X"}, {HOS_SW_Y, "Y"}, {HOS_SW_L, "L"},
                            {HOS_SW_R, "R"}, {HOS_SW_ZL, "ZL"}, {HOS_SW_ZR, "ZR"}, {HOS_SW_PLUS, "Plus"},
                            {HOS_SW_MINUS, "Minus"}, {HOS_SW_LS, "Left stick press"}, {HOS_SW_RS, "Right stick press"},
                            {HOS_SW_UP, "D-pad up"}, {HOS_SW_DOWN, "D-pad down"}, {HOS_SW_LEFT, "D-pad left"},
                            {HOS_SW_RIGHT, "D-pad right"}, {HOS_KEY_MOVE_UP, "Move forward"},
                            {HOS_KEY_MOVE_DOWN, "Move back"}, {HOS_KEY_MOVE_LEFT, "Move left"},
                            {HOS_KEY_MOVE_RIGHT, "Move right"}, {HOS_KEY_CAM_UP, "Camera up"},
                            {HOS_KEY_CAM_DOWN, "Camera down"}, {HOS_KEY_CAM_LEFT, "Camera left"},
                            {HOS_KEY_CAM_RIGHT, "Camera right"}};
constexpr int kControls = sizeof controls / sizeof controls[0];   // (then "Reset to defaults")
bool keys_only(int i) { return i < kControls && controls[i].button >= HOS_SW_N; }   // (no gamepad input for those)
bool touchpad_bound() {
    for (int i = 0; host && host->binding && i < kControls; i++)
        for (int slot = 0; slot < 2 && !keys_only(i); slot++)
            if (host->binding(HOS_BIND_PAD, controls[i].button, slot) == HOS_PAD_TOUCHPAD) return true;
    return false;
}
const char* pad_name(int p) {
    static const char* t[HOS_PAD_N] = {"Bottom button", "Right button", "Left button", "Top button", "Select / Back",
                                       "Home", "Start", "Left stick press", "Right stick press", "L1 / LB", "R1 / RB",
                                       "D-pad up", "D-pad down", "D-pad left", "D-pad right", "L2 / LT", "R2 / RT",
                                       "Touchpad"};
    return p >= 0 && p < HOS_PAD_N ? t[p] : "None";
}
const char* key_name(int vk) {
    static char b[16];
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return b[0] = (char)vk, b[1] = 0, b;
    if (vk >= VK_F1 && vk <= VK_F12) return snprintf(b, sizeof b, "F%d", vk - VK_F1 + 1), b;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD0 + 9) return snprintf(b, sizeof b, "Num %d", vk - VK_NUMPAD0), b;
    switch (vk) {
    case VK_SPACE: return "Space";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_BACK: return "Backspace";
    case VK_CAPITAL: return "Caps Lock";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_ADD: return "Num +";
    case VK_SUBTRACT: return "Num -";
    case VK_MULTIPLY: return "Num *";
    case VK_DIVIDE: return "Num /";
    case VK_DECIMAL: return "Num .";
    case VK_OEM_1: return ";";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    case VK_OEM_4: return "[";
    case VK_OEM_5: return "\\";
    case VK_OEM_6: return "]";
    case VK_OEM_7: return "'";
    case VK_LBUTTON: return "Left click";
    case VK_RBUTTON: return "Right click";
    case VK_MBUTTON: return "Middle click";
    case 0x05: return "Mouse 4";   // (VK_XBUTTON1)
    case 0x06: return "Mouse 5";
    }
    return snprintf(b, sizeof b, "Key %d", vk), b;
}
// the keys a binding can take (Esc cancels, Insert and F1 are the menu's, Delete clears)
const std::vector<int>& bindable_keys() {
    static std::vector<int> k;
    if (k.empty()) {
        for (int c = 'A'; c <= 'Z'; c++) k.push_back(c);
        for (int c = '0'; c <= '9'; c++) k.push_back(c);
        for (int c = VK_F2; c <= VK_F12; c++) k.push_back(c);
        for (int c = VK_NUMPAD0; c <= VK_NUMPAD0 + 9; c++) k.push_back(c);
        for (int c : {VK_SPACE, VK_SHIFT, VK_CONTROL, VK_MENU, VK_TAB, VK_RETURN, VK_BACK, VK_CAPITAL, VK_UP, VK_DOWN,
                      VK_LEFT, VK_RIGHT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_ADD, VK_SUBTRACT, VK_MULTIPLY,
                      VK_DIVIDE, VK_DECIMAL, VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
                      VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
                      0x05, 0x06})   // (the mouse's buttons too: they count while the mouse is captured)
            k.push_back(c);
    }
    return k;
}
std::string key_slot_text(int button, int slot) {
    const int k = host->binding(HOS_BIND_KEY, button, slot);
    return k ? key_name(k) : "None";
}

// ---- what is drawn: Dear ImGui draw lists over the window's image (vk_ui.h, vk_imgui.cpp) ----
std::atomic<int64_t> game_frames{0}, shown_frames{0};
const char* const tab_titles[TAB_COUNT] = {"Graphics", "Gameplay", "Colour", "Controls", "Debug"};
constexpr int kVersionMinPt = 10;

ui::Ctx ui_ctx(uint32_t w, uint32_t h) {
    ui::Ctx c{ImGui::GetForegroundDrawList(), ui::fonts(), 1, (float)w, (float)h, 0, pad_last,
              menu_keys::dot(pad_confirm), menu_keys::dot(pad_back), menu_theme::of(opts.menu_theme)};
    c.s = ui::unit((float)h, opts.menu_size, &c.safe);
    return c;
}
float banner_h(const ui::Ctx& c) { return std::max(58 * c.s, c.safe + 40 * c.s); }

const char* fg_short_name(int backend) {
    switch (backend) {
    case 1: return "MasterSword FG";
    case 2: return "SMFG";
    case 3: return "DLSS MFG";
    case 4: return "FSR 3.1 FG";
    case 5: return "XeSS MFG";
    case 6: return "Apex FG";
    default: return nullptr;
    }
}

/// Tallest help text across all rows at `width`, so the help box does not jump as rows change.
float measure_help(ui::Ctx& c, float width) {
    float most = 0;
    for (const Row& r : rows)
        if (r.help) most = std::max(most, ui::para(c, c.f.body, 18 * c.s, ImVec2(), 0, r.help, width, false));
    return most;
}
std::vector<size_t> char_starts(const std::string& t) {
    std::vector<size_t> at;
    for (size_t i = 0; i < t.size(); i++)
        if (((unsigned char)t[i] & 0xC0) != 0x80) at.push_back(i);
    at.push_back(t.size());
    return at;
}
std::string fit(ui::Ctx& c, ImFont* f, float px, float room, const std::string& label, bool sel) {
    auto width = [&](const std::string& t) { return ui::size_of(f, px, t.c_str()).x; };
    if (width(label) <= room) return label;
    const std::vector<size_t> at = char_starts(label);
    const int chars = (int)at.size() - 1;
    if (sel) {
        int start = chars;
        while (start > 0 && width(label.substr(at[start - 1])) <= room) start--;
        marquee_max = start;
        const int from = std::min(marquee_pos, marquee_max);
        int end = chars;
        while (end > from + 1 && width(label.substr(at[from], at[end] - at[from])) > room) end--;
        return label.substr(at[from], at[end] - at[from]);
    }
    int end = chars;
    while (end > 1 && width(label.substr(0, at[end]) + "...") > room) end--;
    std::string t = label.substr(0, at[end]);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    return t + "...";
}
float scroll_rows(int slot, int n, int sel, float top, float bottom, float pitch, int* shown) {
    const int can_fit = std::max(1, (int)((bottom - top) / pitch));
    *shown = can_fit;
    float& at = scroll_at[slot];
    float& want = scroll_want[slot];
    const int wheel = std::exchange(wheel_rows[slot], 0);
    if (n <= can_fit) want = 0;
    else {
        want -= (float)wheel;
        static int fits[sizeof scroll_at / sizeof *scroll_at];
        if (sel != scrolled_to[slot] || can_fit != std::exchange(fits[slot], can_fit)) {
            const int margin = can_fit > 4 ? 1 : 0;
            if (sel - margin < want) want = (float)(sel - margin);
            if (sel + margin + 1 > want + can_fit) want = (float)(sel + margin + 1 - can_fit);
        }
        want = std::round(std::clamp(want, 0.0f, (float)(n - can_fit)));
    }
    scrolled_to[slot] = sel;
    at += (want - at) * ease_step;
    if (std::fabs(want - at) < 0.01f) at = want;
    return at;
}
void scroll_bar(ui::Ctx& c, float x, float top, float bottom, int n, int shown, float first) {
    if (n <= shown) return;
    const float s = c.s, span = bottom - top, y0 = top + span * first / n, y1 = top + span * (first + shown) / n;
    c.d->AddRectFilled(ImVec2(x - 2 * s, top), ImVec2(x + 2 * s, bottom), c.theme.scroll_track, 2 * s);
    c.d->AddRectFilled(ImVec2(x - 2 * s, y0), ImVec2(x + 2 * s, y1), c.theme.accent, 2 * s);
}
float title_banner(ui::Ctx& c, const char* title, const char* right) {
    const float hh = banner_h(c), s = c.s;
    ui::banner(c, hh);
    ui::text_mid_centre(c, c.f.heading, 24 * s, c.w * 0.5f, hh * 0.5f + s, c.theme.banner_ink, title);
    if (right) ui::text_mid_right(c, c.f.body, 17 * s, c.w - c.safe, hh * 0.5f + s, c.theme.banner_ink, right);
    return hh;
}
void help_box(ui::Ctx& c, float x0, float x1, float y0, const char* title, const char* help, const char* hint, const char* gold_line,
              float min_h = 0, bool after_restart = false, float y_min = 0, float y_max = FLT_MAX) {
    const float s = c.s, w = x1 - x0 - 48 * s;
    const float th = help ? ui::para(c, c.f.body, 18 * s, ImVec2(), 0, help, w, false) : 0,
                hh = hint ? ui::para(c, c.f.body, 16 * s, ImVec2(), 0, hint, w - 18 * s, false) : 0;
    const float text_h = th + (hint ? 14 * s + hh : 0), note = after_restart ? 30 * s : 0;
    const float fixed = 22 * s + (title ? 32 * s : 0) + note + (gold_line ? 52 * s : 0) + 22 * s;
    float body = std::max(min_h, text_h);
    if (y0 + fixed + body > y_max) y0 = std::max(y_min, y_max - fixed - body);
    if (y0 + fixed + body > y_max) body = std::max(text_h, y_max - y0 - fixed);
    const float y1 = std::min(y0 + fixed + body, y_max);
    ui::box(c, ImVec2(x0, y0), ImVec2(x1, y1));
    c.d->PushClipRect(ImVec2(x0, y0), ImVec2(x1, y1), true);
    float y = y0 + 22 * s;
    if (title) ui::text(c, c.f.heading, 21 * s, ImVec2(x0 + 24 * s, y), c.theme.ink, title), y += 32 * s;
    if (after_restart) {
        ui::diamond(c, ImVec2(x0 + 29 * s, y + 10 * s), 4 * s, c.theme.heading_col);
        ui::text_mid(c, c.f.strong, 16 * s, x0 + 42 * s, y + 10 * s, c.theme.heading_col, "Takes effect after a restart");
        y += note;
    }
    if (help) ui::para(c, c.f.body, 18 * s, ImVec2(x0 + 24 * s, y), c.theme.ink, help, w);
    if (hint) {
        const float hy = y + th + 14 * s;
        c.d->AddRectFilled(ImVec2(x0 + 24 * s, hy + 2 * s), ImVec2(x0 + 26.5f * s, hy + hh - 2 * s), c.theme.accent, s);
        ui::para(c, c.f.body, 16 * s, ImVec2(x0 + 40 * s, hy), c.theme.soft, hint, w - 18 * s);
    }
    if (gold_line) {
        y += body + 20 * s;
        c.d->AddLine(ImVec2(x0 + 24 * s, y), ImVec2(x1 - 24 * s, y), ui::alpha(c.theme.edge, 0.8f), std::max(1.0f, s));
        ui::diamond(c, ImVec2(x0 + 29 * s, y + 20 * s), 4 * s, c.theme.heading_col);
        ui::text_mid(c, c.f.strong, 16 * s, x0 + 42 * s, y + 20 * s, c.theme.heading_col, gold_line);
    }
    c.d->PopClipRect();
}
void columns(const ui::Ctx& c, float* x0, float* x1, float* hx0, float* hx1, bool* narrow) {
    const float s = c.s;
    *x0 = c.safe + 6 * s;
    *x1 = std::min(c.w - c.safe, std::max(c.w * 0.585f, *x0 + 560 * s));
    *hx0 = *x1 + 28 * s, *hx1 = c.w - c.safe;
    *narrow = *hx1 - *hx0 < 260 * s;
    if (*narrow) *hx0 = *x0 + 20 * s, *hx1 = *x1 - 20 * s;
}
void column(ui::Ctx& c, float x0, float x1, float top) {
    const float s = c.s;
    c.d->AddRectFilled(ImVec2(x0, top), ImVec2(x1, c.h), c.theme.column);
    c.d->AddLine(ImVec2(x0, top), ImVec2(x0, c.h), ui::alpha(c.theme.edge, 1), 2 * s);
    c.d->AddLine(ImVec2(x1, top), ImVec2(x1, c.h), ui::alpha(c.theme.edge, 1), 2 * s);
}
void name_cell(ui::Ctx& c, ImVec2 a, ImVec2 b, const char* name, bool sel, bool pending = false, float px = 18.5f) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f;
    c.d->AddRectFilled(a, b, sel ? c.theme.cell_name_sel : c.theme.cell_name);
    c.d->AddRect(a, b, c.theme.edge, 0, std::max(1.0f, s));
    if (sel) c.d->AddRectFilled(a, ImVec2(a.x + 4 * s, b.y), c.theme.gold_hi);
    float room = b.x - a.x - 36 * s;
    if (pending) {
        ui::diamond(c, ImVec2(b.x - 12 * s, mid), 4 * s, c.theme.heading_col);
        room -= 14 * s;
    }
    ImFont* f = sel ? c.f.strong : c.f.body;
    const std::string shown = fit(c, f, px * s, room, name, false);
    ui::text_mid(c, f, px * s, a.x + 20 * s, mid, sel ? c.theme.ink_bright : c.theme.ink, shown.c_str());
}
void choice_cell(ui::Ctx& c, ImVec2 a, ImVec2 b, const std::string& value, bool sel, bool left, bool right, bool fixed = false) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f, in = 4 * s, px = 18 * s;
    c.d->AddRectFilled(a, b, c.theme.cell_dark);
    c.d->AddRect(a, b, c.theme.edge, 0, std::max(1.0f, s));
    if (sel && !fixed) ui::gold(c, a + ImVec2(in, in), b - ImVec2(in, in));
    const ImU32 ink = sel && !fixed ? c.theme.gold_ink : fixed ? c.theme.dim : c.theme.ink,
                arrow = sel && !fixed ? c.theme.gold_ink : c.theme.dim;
    if (left) ui::triangle(c, ImVec2(a.x + in + 11 * s, mid), 9 * s, 0, arrow);
    if (right) ui::triangle(c, ImVec2(b.x - in - 11 * s, mid), 9 * s, 1, arrow);
    ImFont* f = sel ? c.f.strong : c.f.body;
    const std::string shown = fit(c, f, px, b.x - a.x - 2 * in - (left || right ? 48 : 16) * s, value, sel);
    ui::text_mid_centre(c, f, px, (a.x + b.x) * 0.5f, mid, ink, shown.c_str());
    if (sel) ui::glow(c, a, b);
}
void halves_cell(ui::Ctx& c, ImVec2 a, ImVec2 b, const char* lo, const char* hi, bool high, bool sel, bool fixed) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f, in = 4 * s, px = 18 * s, xm = (a.x + b.x) * 0.5f;
    c.d->AddRectFilled(a, b, c.theme.cell_dark);
    c.d->AddRect(a, b, c.theme.edge, 0, std::max(1.0f, s));
    const ImVec2 fa = high ? ImVec2(xm, a.y + in) : a + ImVec2(in, in), fb = high ? b - ImVec2(in, in) : ImVec2(xm, b.y - in);
    if (sel && !fixed) ui::gold(c, fa, fb);
    else c.d->AddRectFilled(fa, fb, c.theme.bronze);
    const ImU32 on = sel && !fixed ? c.theme.gold_ink : c.theme.ink;
    ui::text_mid_centre(c, high ? c.f.body : c.f.strong, px, (a.x + xm) * 0.5f, mid, high ? c.theme.dim : on, lo);
    ui::text_mid_centre(c, high ? c.f.strong : c.f.body, px, (xm + b.x) * 0.5f, mid, high ? on : c.theme.dim, hi);
    if (sel) ui::glow(c, a, b);
}
void slider_cell(ui::Ctx& c, ImVec2 a, ImVec2 b, const char* value, int permille, bool sel, float* t0, float* t1) {
    const float s = c.s, mid = (a.y + b.y) * 0.5f, in = 4 * s, px = 18 * s;
    c.d->AddRectFilled(a, b, c.theme.cell_dark);
    c.d->AddRect(a, b, c.theme.edge, 0, std::max(1.0f, s));
    if (sel) ui::gold(c, a + ImVec2(in, in), b - ImVec2(in, in));
    const float x0 = a.x + in + 24 * s, x1 = b.x - in - 24 * s, xv = x0 + (x1 - x0) * permille / 1000.0f, ty = b.y - in - 5.5f * s;
    const ImU32 track = sel ? c.theme.slider_track_sel : c.theme.slider_track, done = sel ? c.theme.gold_ink : c.theme.accent;
    c.d->AddRectFilled(ImVec2(x0, ty - 1.5f * s), ImVec2(x1, ty + 1.5f * s), track, 1.5f * s);
    c.d->AddRectFilled(ImVec2(x0, ty - 1.5f * s), ImVec2(xv, ty + 1.5f * s), done, 1.5f * s);
    ui::diamond(c, ImVec2(xv, ty), 4.5f * s, done);
    ui::text_mid_centre(c, c.f.strong, px, (a.x + b.x) * 0.5f, mid - 3 * s, sel ? c.theme.gold_ink : c.theme.ink, value);
    const ImU32 arrow = sel ? c.theme.gold_ink : c.theme.dim;
    ui::triangle(c, ImVec2(a.x + in + 11 * s, mid), 9 * s, 0, arrow), ui::triangle(c, ImVec2(b.x - in - 11 * s, mid), 9 * s, 1, arrow);
    if (sel) ui::glow(c, a, b);
    *t0 = x0, *t1 = x1;
}
std::string restart_line(ui::Ctx& c, float px, float width) {
    const unsigned all = restart_pending();
    std::string line;
    for (int keep = restart_state.n; keep >= 0; keep--) {
        unsigned shown = 0;
        int named = 0, more = 0;
        for (int i = 0; i < restart_state.n; i++)
            if ((all >> i) & 1) (named < keep ? (shown |= 1u << i, named++) : more++);
        const char* names[mr::kMax];
        for (int i = 0; i < restart_state.n; i++) names[i] = i18n::tr(restart_names[i]);
        line = std::string(i18n::tr("Restart needed: ")) + mr::names(names, restart_state.n, shown);
        if (more) {
            const int form = i18n::plural(more);
            line += std::string(named ? i18n::tr(" and ") : "") + std::to_string(more) +
                    i18n::tr(named ? " more" : form == 0 ? " setting" : form == 1 ? " settings" : " settings (many)");
        }
        if (ui::size_of(c.f.strong, px, line.c_str()).x <= width) break;
    }
    return line;
}
void row_value_cell(ui::Ctx& c, const Row& r, ImVec2 va, ImVec2 vb, bool selected, float* t0, float* t1) {
    const float s = c.s;
    if (!r.value) {
        const char* label = !strcmp(r.name, "Controls") ? "Open" : "";
        choice_cell(c, va, vb, label, selected, false, false);
        return;
    }
    const bool fixed = r.hi <= r.lo;
    if (is_slider(r)) {
        slider_cell(c, va, vb, r.text(*r.value), menu_hit::slider_permille(*r.value, r.lo, r.hi), selected, t0, t1);
        return;
    }
    const std::string lo = r.text(r.lo), hi = r.text(r.hi);
    const float half = (vb.x - va.x) * 0.5f - 18 * s;
    if (r.hi - r.lo == r.step && !fixed && ui::size_of(c.f.strong, 18 * s, lo.c_str()).x <= half &&
        ui::size_of(c.f.strong, 18 * s, hi.c_str()).x <= half) {
        halves_cell(c, va, vb, lo.c_str(), hi.c_str(), *r.value == r.hi, selected, fixed);
    } else {
        const bool left = !fixed && (!stops_at_ends(r) || *r.value > r.lo);
        const bool right = !fixed && (!stops_at_ends(r) || *r.value < r.hi);
        choice_cell(c, va, vb, r.text(*r.value), selected, left, right, fixed);
    }
}

void draw_options(ui::Ctx& c) {
    const float s = c.s, hh = banner_h(c);
    ui::banner(c, hh);
    float tw[TAB_COUNT], total = 0;
    for (int i = 0; i < TAB_COUNT; i++) tw[i] = ui::size_of(c.f.heading, 23 * s, tab_titles[i]).x + 56 * s, total += tw[i];
    float x = (c.w - total) * 0.5f;
    const float ty = hh * 0.5f + s;
    const char* const keys[2] = {c.pad ? "L" : "Q", c.pad ? "R" : "E"};
    for (int i = 0; i < 2; i++) {
        const float cw = 26 * s, cx = i ? x + total + 22 * s : x - 22 * s - cw;
        const bool hover = hover_tab == TAB_COUNT + i;
        c.d->AddRectFilled(ImVec2(cx, ty - 13 * s), ImVec2(cx + cw, ty + 13 * s), hover ? c.theme.tab_inset : c.theme.keycap, 3 * s);
        ui::text_mid_centre(c, c.f.strong, 14 * s, cx + cw * 0.5f, ty, hover ? c.theme.tab_ink : c.theme.keycap_ink, keys[i]);
        hits.add(i ? cx - 10 * s : 0, 0, i ? c.w : cx + cw + 10 * s, hh, i ? menu_hit::kTabNext : menu_hit::kTabPrev, -1);
    }
    for (int i = 0; i < TAB_COUNT; i++) {
        const bool shown = i == cur_tab;
        if (shown || i == hover_tab) {
            c.d->AddRectFilled(ImVec2(x, 0), ImVec2(x + tw[i], hh - 3 * s),
                               shown ? c.theme.tab_inset : ui::alpha(c.theme.tab_inset, 0.35f));
            if (shown) {
                c.d->AddLine(ImVec2(x + 1, 0), ImVec2(x + 1, hh - 3 * s), c.theme.tab_inset_edge, std::max(1.0f, s));
                c.d->AddLine(ImVec2(x + tw[i] - 1, 0), ImVec2(x + tw[i] - 1, hh - 3 * s), c.theme.tab_inset_edge, std::max(1.0f, s));
            }
        }
        ui::text_mid(c, c.f.heading, 23 * s, x + 28 * s, ty, shown ? c.theme.tab_ink : c.theme.banner_ink, tab_titles[i]);
        hits.add(x, 0, x + tw[i], hh, menu_hit::kTab, i);
        x += tw[i];
    }
    {
        std::string ver = std::string("MasterSword ") + version_text(HOOKSHOT_VERSION);
        const float room = (c.w - total) * 0.5f - 60 * s - c.safe, px = 15 * s;
        const auto width = [&](float p) { return ui::size_of(c.f.body, p, ver.c_str()).x; };
        const float fitted = ui::fit_px(px, kVersionMinPt * s, 0.5f * s, room, width);
        if (fitted > 0) ui::text_mid(c, c.f.body, fitted, c.safe, ty, c.theme.banner_ink, ver.c_str());
    }
    float cx0, cx1, hx0, hx1;
    bool narrow;
    columns(c, &cx0, &cx1, &hx0, &hx1, &narrow);
    column(c, cx0, cx1, hh);
    const int count = std::max(1, tab_row_count(cur_tab)), sel = tab_selected[cur_tab];
    const Row& selr = rows[tab_row_to_global(cur_tab, sel)];
    const std::string pending_line =
        (cur_tab == TAB_GRAPHICS || cur_tab == TAB_GAMEPLAY || cur_tab == TAB_CONTROLS || cur_tab == TAB_DEBUG) &&
                restart_pending()
            ? restart_line(c, 16 * s, hx1 - hx0 - 66 * s)
            : "";
    static float help_tall = 0, help_tall_w = 0;
    if (help_tall_w != hx1 - hx0) help_tall_w = hx1 - hx0, help_tall = measure_help(c, hx1 - hx0 - 48 * s);
    ui::Prompt prompt_list[6] = {{ui::kUpDown, "Choose"}};
    int prompts_n = 1;
    if (selr.value) prompt_list[prompts_n++] = {ui::kLeftRight, "Change"};
    else prompt_list[prompts_n++] = {ui::kConfirm, !strcmp(selr.name, "Quit to Desktop") ? "Quit" : "Open"};
    prompt_list[prompts_n++] = {ui::kTabs, "Tabs"};
    prompt_list[prompts_n++] = {ui::kBack, "Close"};
    const float bar_top = ui::prompt_bar_top(c);
    float bottom = std::min(c.h - c.safe, bar_top) - 10 * s;
    const bool after = restart_changed(selr);
    if (narrow) {
        const float help_h =
            std::min(22 * s + 32 * s + help_tall + (after ? 30 * s : 0) + (pending_line.empty() ? 0 : 52 * s) + 22 * s,
                     (bottom - hh) * 0.5f);
        bottom = std::min(bar_top, c.h - c.safe) - help_h - 16 * s;
        help_box(c, hx0, hx1, bottom + 8 * s, selr.name, selr.help, nullptr,
                 pending_line.empty() ? nullptr : pending_line.c_str(), help_tall, after, bottom + 8 * s, bar_top - 10 * s);
    } else {
        help_box(c, hx0, hx1, std::max(hh + 26 * s, c.h * 0.30f), selr.name, selr.help, nullptr,
                 pending_line.empty() ? nullptr : pending_line.c_str(), help_tall, after, hh + 18 * s, bar_top - 12 * s);
    }
    const float rh = 40 * s, pitch = 48 * s, top = hh + 24 * s, rx0 = cx0 + 44 * s, rx1 = cx1 - 44 * s,
                split = rx0 + (rx1 - rx0) * 0.55f;
    int shown = 0;
    const float first = scroll_rows(cur_tab, count, sel, top, bottom, pitch, &shown);
    scroll_bar(c, cx0 + 20 * s, top, top + shown * pitch - (pitch - rh), count, shown, first);
    marquee_max = 0;
    c.d->PushClipRect(ImVec2(cx0, top - 8 * s), ImVec2(cx1, bottom + 8 * s));
    for (int i = 0; i < count; i++) {
        const float y = top + (i - first) * pitch;
        if (y + rh < top - 8 * s || y > bottom + 8 * s) continue;
        const Row& r = rows[tab_row_to_global(cur_tab, i)];
        const bool selected = i == sel;
        name_cell(c, ImVec2(rx0, y), ImVec2(split, y + rh), r.name, selected, restart_changed(r));
        const ImVec2 va(split, y), vb(rx1, y + rh);
        float t0 = 0, t1 = 0;
        row_value_cell(c, r, va, vb, selected, &t0, &t1);
        hits.add(rx0, std::max(y, top - 8 * s), rx1, std::min(y + rh, bottom + 8 * s), menu_hit::kRow, i, va.x, vb.x, t0, t1);
    }
    c.d->PopClipRect();
    ui::prompt_bar(c, prompt_list, prompts_n);
}

void draw_controls(ui::Ctx& c) {
    using enum menu_hit::CtlColumn;
    const float s = c.s, hh = title_banner(c, "Controls", nullptr);
    float cx0, cx1, hx0, hx1;
    bool narrow;
    columns(c, &cx0, &cx1, &hx0, &hx1, &narrow);
    column(c, cx0, cx1, hh);
    const int n = kControls + 1;
    const ui::Prompt idle[] = {{ui::kUpDown, "Choose"}, {ui::kLeftRight, "Column"}, {ui::kConfirm, "Bind"},
                               {ui::kKey, "Clear", "Del"}, {ui::kBack, "Back"}};
    const ui::Prompt wait[] = {{ui::kBack, "Cancel"}};
    const float bar_top = ui::prompt_bar_top(c);
    const char* title = csel < kControls ? controls[csel].name : "Reset to defaults";
    const char* help = listening
                           ? (listening == 1 ? "Press the gamepad button to use. Esc cancels." : "Press the key to use. Esc cancels.")
                           : "Either the gamepad button or either assigned keyboard key presses the selected Switch button. "
                             "Enter binds the selected cell and Delete clears it. Swapping a gamepad button exchanges it with "
                             "any button that held it so nothing is left unbound.";
    const char* hint = "Gamepad: the D-pad moves, bottom button binds, right button goes back.";
    float bottom = std::min(c.h - c.safe, bar_top) - 10 * s;
    if (narrow) {
        bottom = std::min(bar_top, c.h - c.safe) - 200 * s;
        help_box(c, hx0, hx1, bottom + 8 * s, nullptr, help, hint, nullptr, 0, false, bottom + 8 * s, bar_top - 10 * s);
    } else {
        help_box(c, hx0, hx1, std::max(hh + 26 * s, c.h * 0.30f), title, help, hint, nullptr, 0, false, hh + 18 * s,
                 bar_top - 12 * s);
    }
    const float rx0 = cx0 + 44 * s, rx1 = cx1 - 44 * s, width = rx1 - rx0;
    const float pad_x = rx0 + width * 0.34f, key1_x = rx0 + width * 0.56f, key2_x = rx0 + width * 0.78f;
    const float col_x[] = {pad_x, key1_x, key2_x, rx1};
    float top = hh + 22 * s;
    ui::text_mid_centre(c, c.f.body, 15 * s, (key1_x + rx1) * 0.5f, top + 8 * s, c.theme.dim, "Keyboard");
    ui::text_mid(c, c.f.body, 15 * s, rx0 + 20 * s, top + 27 * s, c.theme.dim, "Button");
    const char* titles[kCtlColumns] = {"Gamepad", "Key 1", "Key 2"};
    for (int col = kCtlPrimary; col < kCtlColumns; col++)
        ui::text_mid_centre(c, c.f.body, 15 * s, (col_x[col] + col_x[col + 1]) * 0.5f, top + 27 * s, c.theme.dim, titles[col]);
    top += 43 * s;
    const float rh = 36 * s, pitch = 42 * s;
    int shown = 0;
    const float first = scroll_rows(kControlsSlot, n, csel, top, bottom, pitch, &shown);
    scroll_bar(c, cx0 + 20 * s, top, top + shown * pitch - (pitch - rh), n, shown, first);
    c.d->PushClipRect(ImVec2(cx0, top - 8 * s), ImVec2(cx1, bottom + 8 * s));
    for (int i = 0; i < n; i++) {
        const float y = top + (i - first) * pitch;
        if (y + rh < top - 8 * s || y > bottom + 8 * s) continue;
        const bool sel = i == csel;
        const float y0 = std::max(y, top - 8 * s), y1 = std::min(y + rh, bottom + 8 * s);
        if (i == kControls) {
            ui::button(c, ImVec2(rx0, y), ImVec2(pad_x - 8 * s, y + rh), "Reset to defaults", sel);
            hits.add(rx0, y0, pad_x - 8 * s, y1, menu_hit::kCtlRow, i);
            continue;
        }
        const Control& ctl = controls[i];
        name_cell(c, ImVec2(rx0, y), ImVec2(pad_x, y + rh), ctl.name, sel, false, 17);
        hits.add(rx0, y0, pad_x, y1, menu_hit::kCtlRow, i);
        const bool ko = keys_only(i);
        const int active_col = ko && ccol == kCtlPrimary ? (int)kCtlKeyboard : ccol;
        for (int col = kCtlPrimary; col < kCtlColumns; col++) {
            const bool is_pad = col == kCtlPrimary, disabled = ko && is_pad;
            const float a = col_x[col] + (is_pad ? 0 : 3 * s), b = col_x[col + 1] - (col == kCtlKeyboard ? 0 : 3 * s);
            const std::string v = disabled ? "-"
                                : is_pad   ? std::string(pad_name(host->binding(HOS_BIND_PAD, ctl.button, 0)))
                                           : key_slot_text(ctl.button, col - 1);
            const bool here = sel && col == active_col;
            const ImVec2 va(a, y), vb(b, y + rh);
            if (here && listening) {
                const float in = 4 * s;
                c.d->AddRectFilled(va, vb, c.theme.cell_dark);
                c.d->AddRect(va, vb, c.theme.edge, 0, std::max(1.0f, s));
                c.d->AddRectFilled(va + ImVec2(in, in), vb - ImVec2(in, in), c.theme.listen);
                const std::string prompt = fit(c, c.f.strong, 16.5f * s, b - a - 2 * in, is_pad ? "Press button" : "Press key", false);
                ui::text_mid_centre(c, c.f.strong, 16.5f * s, (a + b) * 0.5f, y + rh * 0.5f, c.theme.ink_bright, prompt.c_str());
                ui::glow(c, va, vb);
            } else {
                choice_cell(c, va, vb, v, here, false, false, disabled);
            }
            if (!disabled) hits.add(a, y0, b, y1, menu_hit::kCtlCell, i, 0, 0, 0, 0, col);
        }
    }
    c.d->PopClipRect();
    (void)(listening ? ui::prompt_bar(c, wait, 1) : ui::prompt_bar(c, idle, 5));
}

void draw_ask(ui::Ctx& c) {
    const float s = c.s, W = 560 * s, H = 232 * s;
    const ImVec2 p((c.w - W) * 0.5f, (c.h - H) * 0.5f), q = p + ImVec2(W, H);
    ui::shadow(c, p, q, 30 * s, 1.0f, 8 * s);
    c.d->AddRectFilled(p, q, c.theme.box);
    c.d->AddRect(p, q, c.theme.box_edge, 0, 2 * s);
    const float hb = 50 * s;
    c.d->AddRectFilledMultiColor(p, ImVec2(q.x, p.y + hb), c.theme.banner_top, c.theme.banner_top, c.theme.banner, c.theme.banner);
    c.d->AddRectFilled(ImVec2(p.x, p.y + hb - 3 * s), ImVec2(q.x, p.y + hb), c.theme.banner_edge);
    ui::text_mid_centre(c, c.f.heading, 23 * s, (p.x + q.x) * 0.5f, p.y + hb * 0.5f, c.theme.banner_ink, "Restart Needed");
    const std::string ask = std::string(i18n::tr("Restart to apply ")) + restart_list(i18n::tr(" and ")) + "?";
    ui::para(c, c.f.body, 19 * s, ImVec2(p.x + 28 * s, p.y + 64 * s), c.theme.ink, ask.c_str(), W - 56 * s);
    ui::text_mid(c, c.f.body, 16 * s, p.x + 28 * s, p.y + 124 * s, c.theme.soft, "Progress since your last save is lost.");
    const float bw = 210 * s, bh = 46 * s, gap = 24 * s, by = p.y + 160 * s, x0 = (p.x + q.x) * 0.5f - bw - gap * 0.5f;
    const char* labels[2] = {"Later", "Restart Now"};
    for (int i = 0; i < 2; i++) {
        const ImVec2 a(x0 + i * (bw + gap), by), b = a + ImVec2(bw, bh);
        ui::button(c, a, b, labels[i], i == ask_sel);
        hits.add(a.x, a.y, b.x, b.y, menu_hit::kButton, i);
    }
    const ui::Prompt prompt_list[] = {{ui::kLeftRight, "Choose"}, {ui::kConfirm, "Confirm"}, {ui::kBack, "Later"}};
    ui::prompt_bar(c, prompt_list, 3);
}

void draw_menu(ui::Ctx& c);
float prebaked[3] = {0, 0, 0};
bool prebake_needed(uint32_t w, uint32_t h) {
    float m = 0;
    return ui::unit((float)h, opts.menu_size, &m) != prebaked[0] || (float)w != prebaked[1] ||
           ui::unit((float)h, opts.overlay_size, &m) != prebaked[2];
}
void prebake(const ui::Ctx& c) {
    float m = 0;
    const float os = ui::unit(c.h, opts.overlay_size, &m);
    if (c.s == prebaked[0] && c.w == prebaked[1] && os == prebaked[2]) return;
    prebaked[0] = c.s, prebaked[1] = c.w, prebaked[2] = os;
    static const float points[] = {12, 13, 14, 14.5f, 15, 16, 16.5f, 17, 18, 18.5f, 19, 20, 21, 23, 24};
    constexpr int n = sizeof points / sizeof points[0];
    float px[n + 2];
    for (int i = 0; i < n; i++) px[i] = points[i] * c.s;
    px[n] = 28.0f * os;
    px[n + 1] = 13.0f * os;
    ui::prebake(px, n + 2);
    if (open_now.load(std::memory_order_relaxed)) return;
    const menu_hit::Map saved_hits = hits;
    const int saved_marquee = marquee_max;
    const float saved_ease = ease_step;
    float saved_scroll[sizeof scroll_at / sizeof *scroll_at], saved_want[sizeof scroll_at / sizeof *scroll_at];
    int saved_to[sizeof scroll_at / sizeof *scroll_at];
    memcpy(saved_scroll, scroll_at, sizeof scroll_at);
    memcpy(saved_want, scroll_want, sizeof scroll_want);
    memcpy(saved_to, scrolled_to, sizeof scrolled_to);
    ImDrawList scratch(ImGui::GetDrawListSharedData());
    scratch._ResetForNewFrame();
    scratch.PushTexture(ImGui::GetIO().Fonts->TexRef);
    scratch.PushClipRectFullScreen();
    ease_step = 0;
    {
        ui::Ctx dry = c;
        dry.d = &scratch;
        draw_menu(dry);
    }
    hits = saved_hits, marquee_max = saved_marquee, ease_step = saved_ease;
    memcpy(scroll_at, saved_scroll, sizeof scroll_at);
    memcpy(scroll_want, saved_want, sizeof scroll_want);
    memcpy(scrolled_to, saved_to, sizeof scrolled_to);
}

void draw_menu(ui::Ctx& c) {
    c.d->AddRectFilled(ImVec2(0, 0), ImVec2(c.w, c.h), c.theme.dim_bg);
    ui::Ctx k = c;
    k.w = std::min(c.w, std::floor(c.h * 16.0f / 9.0f + 0.5f));
    menu_ox = std::floor((c.w - k.w) * 0.5f);
    const int vtx0 = c.d->VtxBuffer.Size, cmd0 = c.d->CmdBuffer.Size;
    hits.clear(0, 0, k.w, k.h);
    if (page == 5) draw_ask(k);
    else if (page == 1) draw_controls(k);
    else draw_options(k);
    if (menu_ox <= 0) return;
    for (int i = vtx0; i < c.d->VtxBuffer.Size; i++) c.d->VtxBuffer[i].pos.x += menu_ox;
    for (int i = std::max(0, cmd0 - 1); i < c.d->CmdBuffer.Size; i++) {
        ImVec4& r = c.d->CmdBuffer[i].ClipRect;
        if (r.x <= 0 && r.z >= c.w) continue;
        r.x += menu_ox, r.z += menu_ox;
    }
}

char fps_text[48] = "";
char fps_sub[64] = "";
char fps_detail[64] = "";
char fps_extra1[96] = "";
char fps_extra2[96] = "";
constexpr int kFtSamples = 120;
std::mutex ft_mu;
float ft_ring[kFtSamples] = {};
int ft_head = 0, ft_count = 0;
int64_t ft_last_ns = 0;
float ft_snap[kFtSamples] = {};
int ft_snap_n = 0;

struct Tab {
    ImFont* font;
    float px, digit;
    Tab(ImFont* f, float size) : font(f), px(size), digit(0) {
        char d[2] = {0, 0};
        for (char k = '0'; k <= '9'; k++) d[0] = k, digit = std::max(digit, ui::size_of(f, size, d).x);
    }
    static bool dig(char c) { return c >= '0' && c <= '9'; }
    template <class Fn>
    float pieces(const char* t, float x, Fn fn) const {
        const float x0 = x;
        for (const char* q = t; *q;) {
            const char* e = q + 1;
            if (!dig(*q))
                while (*e && !dig(*e)) e++;
            const float w = dig(*q) ? digit : font->CalcTextSizeA(px, FLT_MAX, 0, q, e).x;
            fn(q, e, x, w);
            x += w, q = e;
        }
        return x - x0;
    }
    float width(const char* t) const { return pieces(t, 0, [](const char*, const char*, float, float) {}); }
    void draw(ImDrawList* d, float x, float y, ImU32 col, const char* t, float s) const {
        const float o = std::max(1.0f, s);
        const int a = std::max(200, (int)((col >> IM_COL32_A_SHIFT) & 0xff));
        const ImU32 shade = ui::rgba(0, 0, 0, a * 210 / 255), faint = ui::rgba(0, 0, 0, a * 160 / 255);
        pieces(t, x, [&](const char* q, const char* e, float px0, float w) {
            if (*q == ' ' && e == q + 1) return;
            const float gx = px0 + (dig(*q) ? (w - font->CalcTextSizeA(px, FLT_MAX, 0, q, e).x) * 0.5f : 0);
            static const float dx[4] = {-1, 1, -1, 1}, dy[4] = {-1, -1, 1, 1};
            for (int k = 0; k < 4; k++) d->AddText(font, px, ImVec2(gx + dx[k] * o * 0.8f, y + dy[k] * o * 0.8f), faint, q, e);
            d->AddText(font, px, ImVec2(gx, y + o), shade, q, e);
            d->AddText(font, px, ImVec2(gx, y), col, q, e);
        });
    }
};

void draw_fps(ui::Ctx& c, bool under_banner) {
    float ov_safe = c.safe;
    const float s = ui::unit(c.h, opts.overlay_size, &ov_safe), big = 28 * s, small = 13 * s;
    const int mode = std::clamp(opts.show_fps, 1, 4);
    const int corner = std::clamp(opts.fps_corner, 0, 3);
    const bool is_right = (corner == 0 || corner == 2);
    const bool is_bottom = (corner == 2 || corner == 3);
    const ImU32 green = ui::rgba(72, 255, 110);
    const ImU32 soft_ink = ui::rgba(226, 232, 226);
    const ImU32 muted_ink = ui::rgba(190, 198, 195);
    const Tab num(c.f.strong, big), tab(c.f.strong, small);
    const float nw = num.width(fps_text), lw = ui::size_of(c.f.strong, small, "FPS").x;
    const float top_row_w = nw + 4 * s + lw;
    const float sw = fps_sub[0] ? tab.width(fps_sub) : 0;
    const float dw = (mode >= 2 && fps_detail[0]) ? tab.width(fps_detail) : 0;
    const float e1w = (mode >= 4 && fps_extra1[0]) ? tab.width(fps_extra1) : 0;
    const float e2w = (mode >= 4 && fps_extra2[0]) ? tab.width(fps_extra2) : 0;
    const float graph_w = mode >= 3 ? 196 * s : 0;
    const float graph_h = mode >= 3 ? 44 * s : 0;
    const float line_h = 17 * s;
    const float pad = mode >= 3 ? 10 * s : 0;

    float content_w = std::max({top_row_w, sw, dw, e1w, e2w, graph_w});
    float content_h = 30 * s;
    if (fps_sub[0]) content_h += line_h;
    if (mode >= 2 && fps_detail[0]) content_h += line_h;
    if (mode >= 3) content_h += 6 * s + graph_h;
    if (mode >= 4 && fps_extra1[0]) content_h += 4 * s + line_h;
    if (mode >= 4 && fps_extra2[0]) content_h += line_h;
    const float box_w = content_w + 2 * pad;
    const float box_h = content_h + 2 * pad;

    const bool beside = menu_ox >= box_w + ov_safe + c.safe + 8 * s;
    const float x0 = is_right ? (c.w - ov_safe - box_w) : ov_safe;
    float y0 = ov_safe;
    if (is_bottom) {
        const float bottom_edge = under_banner && page != 5 && !beside ? ui::prompt_bar_top(c) - 10 * s : c.h - ov_safe;
        y0 = std::max(ov_safe, bottom_edge - box_h);
    } else if (under_banner && page != 5 && !beside) {
        y0 = banner_h(c) + 10 * s;
    }

    if (mode >= 3) {
        c.d->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + box_w, y0 + box_h), ui::rgba(10, 14, 14, 185), 6 * s);
        c.d->AddRect(ImVec2(x0, y0), ImVec2(x0 + box_w, y0 + box_h), ui::rgba(255, 255, 255, 36), 6 * s, 0, std::max(1.0f, s));
    }

    const float ix0 = x0 + pad, ix1 = x0 + box_w - pad;
    float y = y0 + pad;
    {
        const float x = is_right ? (ix1 - top_row_w) : ix0;
        const float o = std::max(1.0f, s);
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (dx || dy) {
                    float gx = x;
                    char one[2] = {0, 0};
                    for (const char* q = fps_text; *q; q++) {
                        one[0] = *q;
                        const float adv = ui::size_of(c.f.strong, big, one).x;
                        c.d->AddText(c.f.strong, big, ImVec2(gx + (num.digit - adv) * 0.5f + dx * o, y + dy * o), ui::rgba(0, 0, 0, 230), one);
                        gx += num.digit;
                    }
                    c.d->AddText(c.f.strong, small, ImVec2(x + nw + 4 * s + dx * o, y + 14 * s + dy * o), ui::rgba(0, 0, 0, 230), "FPS");
                }
        float gx = x;
        char one[2] = {0, 0};
        for (const char* q = fps_text; *q; q++) {
            one[0] = *q;
            const float adv = ui::size_of(c.f.strong, big, one).x;
            c.d->AddText(c.f.strong, big, ImVec2(gx + (num.digit - adv) * 0.5f, y), green, one);
            gx += num.digit;
        }
        c.d->AddText(c.f.strong, small, ImVec2(x + nw + 4 * s, y + 14 * s), green, "FPS");
        y += 30 * s;
    }
    auto draw_line = [&](const char* txt, float w, ImU32 col) {
        const float lx = is_right ? (ix1 - w) : ix0;
        tab.draw(c.d, lx, y, col, txt, s);
        y += line_h;
    };
    if (fps_sub[0]) draw_line(fps_sub, sw, soft_ink);
    if (mode >= 2 && fps_detail[0]) draw_line(fps_detail, dw, soft_ink);
    if (mode >= 3) {
        y += 6 * s;
        const float gx0 = ix0, gx1 = ix1, gy0 = y, gy1 = y + graph_h;
        c.d->AddRectFilled(ImVec2(gx0, gy0), ImVec2(gx1, gy1), ui::rgba(0, 0, 0, 120), 3 * s);
        constexpr float kMaxMs = 50.0f;
        const float y16 = gy1 - (16.67f / kMaxMs) * graph_h;
        const float y33 = gy1 - (33.33f / kMaxMs) * graph_h;
        c.d->AddLine(ImVec2(gx0, y16), ImVec2(gx1, y16), ui::rgba(72, 255, 110, 55), std::max(1.0f, s * 0.7f));
        c.d->AddLine(ImVec2(gx0, y33), ImVec2(gx1, y33), ui::rgba(255, 196, 64, 55), std::max(1.0f, s * 0.7f));
        if (ft_snap_n >= 2) {
            ImVec2 pts[kFtSamples];
            const float dx = (gx1 - gx0) / (float)(kFtSamples - 1);
            const int start = kFtSamples - ft_snap_n;
            float worst = 0.0f;
            for (int i = 0; i < ft_snap_n; i++) {
                const float ms = std::clamp(ft_snap[i], 0.0f, kMaxMs);
                worst = std::max(worst, ms);
                pts[i] = ImVec2(gx0 + (start + i) * dx, gy1 - (ms / kMaxMs) * (graph_h - 2 * s) - s);
            }
            const ImU32 line_col = worst > 34.0f ? ui::rgba(255, 96, 80, 235)
                                 : worst > 19.0f ? ui::rgba(255, 200, 64, 235)
                                                 : ui::rgba(72, 255, 110, 235);
            c.d->AddPolyline(pts, ft_snap_n, line_col, 0, std::max(1.5f, 1.6f * s));
        }
        y = gy1;
    }
    if (mode >= 4) {
        if (fps_extra1[0]) {
            y += 4 * s;
            draw_line(fps_extra1, e1w, soft_ink);
        }
        if (fps_extra2[0]) draw_line(fps_extra2, e2w, muted_ink);
    }
}

// ---- start-up prewarm screen: Dear ImGui over dark/themed backdrop ----
StartupView startup;
std::mutex startup_mu;
std::atomic<bool> startup_on{false};

void draw_startup(ui::Ctx& c) {
    StartupView v;
    {
        std::lock_guard lk(startup_mu);
        v = startup;
    }
    const float s = c.s, w = c.w, h = c.h, cx = w * 0.5f, k = s * 0.86f;
    c.d->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), c.theme.startup_bg);
    const float lh = 112 * s, top = h * 0.5f - 176 * s;
    for (int i = 40; i >= 1; i--) c.d->AddCircleFilled(ImVec2(cx, top + lh * 0.5f + 70 * s), h * 0.021f * i, c.theme.startup_glow, 128);
    float aspect = 1;
    ImTextureData* logo = ui::logo(&aspect);
    if (logo) c.d->AddImage(logo->GetTexRef(), ImVec2(cx - lh * aspect * 0.5f, top), ImVec2(cx + lh * aspect * 0.5f, top + lh));
    float y = top + lh + 16 * s;
    ui::spaced(c, c.f.heading, 42 * k, ImVec2(cx, y), c.theme.startup_title, "MASTERSWORD", 6 * k);
    ui::flourish(c, cx - 190 * k, cx + 190 * k, y + 66 * k, c.theme.accent);
    ui::text_centre(c, c.f.body, 14.5f * k, ImVec2(cx, y + 78 * k), c.theme.dim, v.version.c_str());
    if (!v.title.empty()) {
        y += 120 * k;
        if (!v.step.empty()) {
            std::string step = v.step;
            for (size_t i = 1; i < step.size(); i++) step[i] = (char)tolower((unsigned char)step[i]);
            ui::text_centre(c, c.f.body, 15 * k, ImVec2(cx, y), c.theme.accent, step.c_str());
        }
        ui::text_centre(c, c.f.heading, 24 * k, ImVec2(cx, y + 20 * k), c.theme.ink, v.title.c_str());
        const float x0 = cx - 270 * k, x1 = cx + 270 * k, by = y + 64 * k;
        float under = by;
        if (v.fraction >= 0) {
            const float bh = 26 * k, in = 4 * k, fx = x0 + in + (x1 - x0 - 2 * in) * (float)std::min(v.fraction, 1.0);
            c.d->AddRectFilled(ImVec2(x0, by), ImVec2(x1, by + bh), c.theme.cell_dark);
            c.d->AddRect(ImVec2(x0, by), ImVec2(x1, by + bh), c.theme.edge, 0, std::max(1.0f, s));
            if (fx > x0 + in) ui::gold(c, ImVec2(x0 + in, by + in), ImVec2(fx, by + bh - in));
            const float split = std::max(fx, x0 + in);
            c.d->PushClipRect(ImVec2(x0, by), ImVec2(split, by + bh), true);
            ui::text_mid_centre(c, c.f.strong, 15 * k, cx, by + bh * 0.5f, c.theme.gold_ink, v.pct.c_str());
            c.d->PopClipRect();
            c.d->PushClipRect(ImVec2(split, by), ImVec2(x1, by + bh), true);
            ui::text_mid_centre(c, c.f.strong, 15 * k, cx, by + bh * 0.5f, c.theme.ink, v.pct.c_str());
            c.d->PopClipRect();
            under = by + bh;
        }
        const float px = 16.5f * k;
        ui::text(c, c.f.body, px, ImVec2(v.fraction >= 0 ? x0 : cx - ui::size_of(c.f.body, px, v.left.c_str()).x * 0.5f, under + 12 * k),
                 c.theme.soft, v.left.c_str());
        if (!v.right.empty())
            ui::text(c, c.f.body, px, ImVec2(x1 - ui::size_of(c.f.body, px, v.right.c_str()).x, under + 12 * k), c.theme.soft, v.right.c_str());
        if (!v.hint.empty()) ui::text_centre(c, c.f.body, 15.5f * k, ImVec2(cx, by + 92 * k), c.theme.dim, v.hint.c_str());
    }
    if (const float f = startup_fade.load(std::memory_order_relaxed); f > 0)
        c.d->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), ui::rgba(0, 0, 0, (int)(std::min(f, 1.0f) * 255 + 0.5f)));
}

} // namespace

void startup_fade_to(float f) { startup_fade.store(f, std::memory_order_relaxed); }

// (#1916) Upscaler one-time probes on a background thread during start-up, so the first Insert open does not hitch.
static void probe_upscalers_soon() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (!dev.device) return;
        std::thread([] {
            const auto t0 = std::chrono::steady_clock::now();
            dlss_probe();
            fsr4_probe();
            (void)fsr3::supported();
            logf("menu: the upscalers asked in the background while the start-up screen shows (%.0f ms)",
                 std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        }).detach();
    });
}

void upscalers_probe_start() { probe_upscalers_soon(); }

void startup_screen(const StartupView* view) {
    if (!view) {
        startup_on.store(false, std::memory_order_release);
        startup_fade.store(0, std::memory_order_relaxed);
        return;
    }
    StartupView v = *view;
    if (v.version.empty()) v.version = version_text(HOOKSHOT_VERSION);
    {
        std::lock_guard lk(startup_mu);
        startup = v;
    }
    startup_on.store(true, std::memory_order_release);
    probe_upscalers_soon();
}

int menu_vsync() { return getenv("RX_VK_FIFO") ? 1 : getenv("RX_VK_IMMEDIATE") ? 2 : opts.vsync; }

static bool first_start = false;   // (menu_init: no options file yet → menu_first_start_for_gpu)

void menu_init() {
    static bool done;
    if (done) return;
    done = true;
    bool has_options = false;
    if (FILE* f = fopen(ini_path().c_str(), "r")) {
        has_options = true;
        char line[256];
        bool has_resolution = false;
        bool has_fg_backend = false;
        int options_ver = 0;
        bool has_menu_theme = false;
        int old_dynres = -1;   // (the row before Resolution: 0, the full scale, is its 100%; 1 is Auto)
        while (fgets(line, sizeof line, f)) {
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            if (!strcmp(line, "menu_theme")) has_menu_theme = true;
            for (Row& r : rows)
                if (r.value && !is_session_only(r) && !strcmp(line, ini_key(r))) {
                    if (r.value == &opts.reflex) {
                        const char* v = eq + 1;
                        while (*v == ' ' || *v == '\t') v++;
                        if (!_strnicmp(v, "boost", 5)) opts.reflex = 2;
                        else if (!_strnicmp(v, "on", 2)) opts.reflex = 1;
                        else if (!_strnicmp(v, "off", 3)) opts.reflex = 0;
                        else opts.reflex = std::min(std::max(atoi(v), r.lo), r.hi);
                    } else {
                        *r.value = std::min(std::max(atoi(eq + 1), r.lo), r.hi);
                    }
                }
            // Launcher alias: low_latency=boost|on|off
            if (!strcmp(line, "low_latency")) {
                const char* v = eq + 1;
                while (*v == ' ' || *v == '\t') v++;
                if (!_strnicmp(v, "boost", 5)) opts.reflex = 2;
                else if (!_strnicmp(v, "on", 2)) opts.reflex = 1;
                else if (!_strnicmp(v, "off", 3)) opts.reflex = 0;
                else opts.reflex = std::min(std::max(atoi(v), 0), 2);
            }
            has_resolution |= !strcmp(line, "resolution");
            // ("fg_engine" from the current row name; "fg_backend" from older Nodix builds)
            if (!strcmp(line, "fg_engine") || !strcmp(line, "fg_backend")) {
                opts.fg_backend = std::min(std::max(atoi(eq + 1), 0), 6);
                has_fg_backend = true;
            }
            // (older Nodix ini keys before the DLSS FG / Flow rows were renamed)
            if (!strcmp(line, "framegen_preset") || !strcmp(line, "frame_gen_preset") || !strcmp(line, "fg_model") ||
                !strcmp(line, "dlss_fg_preset"))
                opts.framegen_preset = std::min(std::max(atoi(eq + 1), 0), 2);
            if (!strcmp(line, "fg_flow")) opts.fg_flow = std::min(std::max(atoi(eq + 1), 10), 100);
            if (!strcmp(line, "options_version")) options_ver = atoi(eq + 1);
            if (!strcmp(line, "dynamic_resolution")) old_dynres = atoi(eq + 1);
        }
        fclose(f);
        if (!has_resolution && old_dynres >= 0) opts.resolution = old_dynres ? 0 : 1;
        // (Game speed is Real time by default now: Per frame played at half speed at 30 fps, the "slow motion" players
        // reported. An options file from before kept the old default whether chosen or not: Real time for it too)
        if (options_ver < 2) opts.realtime = 1;
        // (v6: Hookshot is choice 1. Pre-v6 Nodix numbering was 1 DLSS, 2 FSR, 3 XeSS, 4 SMFG, 5 SMAI-as-FG.)
        if (options_ver > 0 && options_ver < 6 && has_fg_backend) {
            static const int map[] = {0, 4, 5, 6, 2, 3};
            if (opts.fg_backend >= 0 && opts.fg_backend <= 5) opts.fg_backend = map[opts.fg_backend];
        }
        // (v7: SMAI left FG — was slot 3 under v6. Shift DLSS/FSR/XeSS down; old SMAI → SMFG + Upscale SMAI.)
        if (options_ver > 0 && options_ver < 7 && has_fg_backend) {
            if (opts.fg_backend == 3) {
                opts.fg_backend = 2;   // SMFG
                if (opts.upscale < 7) opts.upscale = 7;   // SMAI under Upscaling
            } else if (opts.fg_backend >= 4) {
                opts.fg_backend -= 1;
            }
        }
        if (opts.fg_backend > 6) opts.fg_backend = 6;
        // (v8 / #2147) FSR quality grew 92/83/75 between Native AA and Quality: old 1..4 → 4..7
        if (options_ver > 0 && options_ver < 8 && opts.fsr_quality >= 1 && opts.fsr_quality <= 4)
            opts.fsr_quality += 3;
        // (v9) Menu theme: 0 OOT Mastersword (default), 1 Light, 2 Dark, 3 Longshot, 4 OOT Mastersword (Amber).
        // First load of v9 takes OOT; after that the player's own Menu theme row is kept (saved as options_version 9).
        if (options_ver < 9) opts.menu_theme = 0;
        opts.menu_theme = std::min(std::max(opts.menu_theme, 0), 4);
        opts.menu_size = std::min(std::max(opts.menu_size, 1), 3);
        opts.overlay_size = std::min(std::max(opts.overlay_size, 1), 3);
        (void)has_menu_theme;
        // Older files only had the multiplier — that meant Hookshot's native FG.
        if (!has_fg_backend && opts.framegen >= 2) opts.fg_backend = 1;
        if (opts.framegen >= 2 && opts.fg_backend == 0) opts.fg_backend = 1;
        clamp_framegen_steps();   // drop a step this engine does not ship (FSR → 2x, DLSS 8x → 6x)
        // (#2000) DLSS / FSR 4 quality presets seed the Resolution slider when the file has no stop yet (Auto).
        if (temporal_sr_upscale() && opts.dlss_quality != 4 && !upscaler_rows::percent(opts.resolution)) {
            static const int steps[] = {3, 5, 4, 6};  // Q/B/P/Ultra
            opts.resolution = steps[std::min(std::max(opts.dlss_quality, 0), 3)];
        }
        if (opts.color_preset <= 2) apply_color_preset(opts.color_preset);
        else if (opts.color_preset == 3) { /* Custom: keep loaded sliders */ }
    }
    first_start = !has_options;
    // An opt-in profile for 4 GB cards: keep existing options authoritative, but make a first run usable on
    // low-VRAM hardware without requiring the menu to render first.
    if (!has_options && getenv("RX_VK_LOW_SPEC") && atoi(getenv("RX_VK_LOW_SPEC")) != 0) {
        apply_preset(1);
        opts.fg_backend = 0;
        opts.framegen = 0;
        opts.upscale = 0;
        opts.sharpness = 0;
    }
    if (opts.preset > 0) sync_preset_budgets(opts.preset);
    if (opts.framegen == 1) opts.framegen = 2;
    if (const char* e = getenv("RX_VK_FRAMEGEN_MULT")) {
        opts.framegen = std::min(std::max(atoi(e), 2), 8);
        if (opts.fg_backend == 0) opts.fg_backend = 1;
    }
    clamp_framegen_steps();
    if (const char* e = getenv("RX_VK_PACE")) opts.pace = atoi(e) != 0;
    if (const char* e = getenv("RX_VK_UPSCALE"))
        for (Row& r : rows)
            if (r.value == &opts.upscale) *r.value = std::min(std::max(atoi(e), r.lo), r.hi);
    if (const char* e = getenv("HOS_REALTIME_STEP")) opts.realtime = atoi(e) != 0;
    if (const char* e = getenv("HOS_DYNRES")) opts.resolution = atoi(e) != 0 ? 0 : 1;
    if (const char* e = getenv("HOS_OUTPUT_RES")) opts.output_res = std::min(std::max(atoi(e), 0), 2);
    mic_sync_hi();
    mic_apply();
    restart_init();
}

/// (#2187) First start with no options file: Quality preset from VRAM so small cards do not start at full docked budget.
/// Integrated / ≤4.5 GB → Minimum; ≤6.5 GB → Low; larger unchanged. RX_VK_AUTO_PRESET=0 off.
void menu_first_start_for_gpu(const VkPhysicalDeviceProperties& props, const VkPhysicalDeviceMemoryProperties& mem) {
    if (!first_start) return;
    first_start = false;
    if (getenv("RX_VK_AUTO_PRESET") && atoi(getenv("RX_VK_AUTO_PRESET")) == 0) return;
    if (getenv("RX_VK_LOW_SPEC") && atoi(getenv("RX_VK_LOW_SPEC")) != 0) return;
    uint64_t vram = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; i++)
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) vram = std::max<uint64_t>(vram, mem.memoryHeaps[i].size);
    const bool integrated = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    const uint64_t mb = getenv("RX_VK_AUTO_PRESET_VRAM_MB") ? (uint64_t)atoll(getenv("RX_VK_AUTO_PRESET_VRAM_MB")) : vram >> 20;
    const int preset = integrated || mb <= 4608 ? 1 : mb <= 6656 ? 2 : 0;
    if (!preset) return;
    opts.preset = preset;
    apply_preset(preset);
    sync_preset_budgets(preset);
    if (preset == 1) opts.upscale = 0, opts.sharpness = 0;
    opts.handheld = 0;   // docked: handheld Minimum gave a black picture on a GTX 1050
    {
        int now[mr::kMax];
        restart_values(now);
        restart_state.choose_later(now);
    }
    save();
    logf("vk: first start on %s (%s, %llu MB of VRAM): the %s preset", props.deviceName, integrated ? "integrated" : "dedicated",
         (unsigned long long)mb, tx_preset(preset));
}

void menu_output_size(uint32_t& w, uint32_t& h) {
    const Out o = shown_output();
    w = o.w;
    h = o.h;
}


/// Map MasterSword Upscaling onto upstream temporal opts.dlss (DLSS / FSR 4 / #2097 Off).
/// Under DLSS / FSR 4, Resolution is the #2000 render slider; quality presets snap that slider in set_row.
void sync_temporal_from_upscale() {
    namespace ur = upscaler_rows;
    if (opts.upscale == 8) {
        opts.dlss = (opts.dlss_quality == 4) ? ur::DLAA : ur::DLSS;
    } else if (opts.upscale == 9) {
        // FSR 4 only when the build + AMD library can run it (else leave Game's own).
        if (fsr4_why() != FSR_OK) opts.dlss = ur::GAME;
        else opts.dlss = (opts.dlss_quality == 4) ? ur::FSR4_AA : ur::FSR4;
    } else if (opts.upscale == 4) {
        opts.dlss = ur::FSR3;
    } else if (opts.upscale == 0) {
        opts.dlss = ur::OFF;   // #2097: stretch, leave the game's network out
    } else if (opts.dlss == ur::DLSS || opts.dlss == ur::DLAA || opts.dlss == ur::OFF ||
               opts.dlss == ur::FSR3 || opts.dlss == ur::FSR4 || opts.dlss == ur::FSR4_AA) {
        opts.dlss = ur::GAME;
    }
}
/// Nearest DLSS quality index for a render-percent (keeps the DLSS quality row in sync with the slider).
int dlss_quality_from_percent(int p) {
    if (p >= 100) return 4;   // DLAA
    if (p >= 67) return 0;
    if (p >= 58) return 1;
    if (p >= 50) return 2;
    return 3;
}
int upscaler_render_percent() {
    sync_temporal_from_upscale();
    // Vendor quality → internal 3D percent of Output (PC-title ratios). 0: Resolution row owns scale.
    switch (opts.upscale) {
    case 3: {   // XeSS
        static const int pct[] = {100, 77, 67, 59, 50, 33, 25};
        return pct[std::min(std::max(opts.xess_quality, 0), 6)];
    }
    case 4:
    case 5: {   // FSR 3.1 / 1.0 (#2147: 100/92/83/75 then AMD modes; matches kFsr3Steps percents)
        static const int pct[] = {100, 92, 83, 75, 67, 58, 50, 33};
        return pct[std::min(std::max(opts.fsr_quality, 0), 7)];
    }
    case 6:
    case 7: {   // Nodix SMRS / SMAI
        static const int pct[] = {67, 58, 50};
        return pct[std::min(std::max(opts.nodix_quality, 0), 2)];
    }
    case 8:   // DLSS: Resolution slider / DLAA (#2000). Quality row snaps the same scale.
    case 9: { // FSR 4: same shares / Native AA.
        if (opts.dlss_quality == 4 || opts.dlss == upscaler_rows::DLAA || opts.dlss == upscaler_rows::FSR4_AA) return 100;
        if (const int p = upscaler_rows::percent(opts.resolution)) return p;
        static const int pct[] = {67, 58, 50, 33, 100};
        return pct[std::min(std::max(opts.dlss_quality, 0), 4)];
    }
    default:
        return 0;
    }
}
bool resolution_auto() {
    if (upscaler_render_percent() > 0) return false;   // vendor quality owns the fixed 3D scale
    return opts.resolution == 0;
}
int resolution_scale() {
    sync_temporal_from_upscale();
    int pct = upscaler_render_percent();
    if (pct <= 0) {
        if (temporal_sr_upscale()) {
            pct = upscaler_rows::percent(opts.resolution);
            if (pct <= 0) return 0;
        } else {
            if (opts.resolution <= 0 || opts.resolution > 4) return 0;
            pct = kResPercent[opts.resolution];
        }
    }
    return (int)(twelfths(pct / 100.0f) * 10000.0f / 12.0f + 0.5f);   // twelfth of Output (game_size)
}

bool upscaler_wants_upscale() {
    // Same methodology as DLSS Perf/Quality: internal 3D < Output, upscaler fills Output.
    // Skip Off, Native AA / DLAA (100%), and Resolution 100%.
    const int pct = upscaler_render_percent();
    if (pct > 0 && pct < 100) return true;
    if (opts.upscale == 1 || opts.upscale == 2) {
        // Catmull / Lanczos: Resolution row owns scale (2=75%, 3=67%, 4=50%).
        return opts.resolution >= 2 && opts.resolution <= 4;
    }
    return false;
}

namespace {
struct UpscaleInput {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    uint32_t w = 0, h = 0;
} g_upscale_in;
} // namespace

void upscale_stash_input(VkImage img, VkImageView view, VmaAllocation alloc, VkFormat fmt, uint32_t w, uint32_t h) {
    g_upscale_in = {img, view, alloc, fmt, w, h};
}
bool upscale_take_input(VkImage* img, VkImageView* view, VmaAllocation* alloc, VkFormat* fmt, uint32_t* w, uint32_t* h) {
    if (!g_upscale_in.image || !g_upscale_in.alloc || !g_upscale_in.w || !g_upscale_in.h) return false;
    if (img) *img = g_upscale_in.image;
    if (view) *view = g_upscale_in.view;
    if (alloc) *alloc = g_upscale_in.alloc;
    if (fmt) *fmt = g_upscale_in.fmt;
    if (w) *w = g_upscale_in.w;
    if (h) *h = g_upscale_in.h;
    g_upscale_in = {};
    return true;
}

void menu_set_gpu_count(int count) {
    restart_init();
    for (int i = 0; i < kRows; i++) {
        Row& r = rows[i];
        if (r.value == &opts.gpu_device) {
            r.hi = count;
            if (opts.gpu_device > count) opts.gpu_device = 0;
            if (restart_slot[i] >= 0) restart_state.restart_value(restart_slot[i], opts.gpu_device);
        }
    }
}


/// A row's new value v (dir: 1 up, -1 down); keep=false defers save (slider drag)
void set_row(Row& r, int v, int dir, bool keep = true) {
    if (!r.value) return;
    if (r.value == &opts.framegen) v = snap_framegen(v, dir);
    if (r.value == &opts.vsync) v = vsync_cycle(*r.value, dir);
    if (stops_at_ends(r)) v = std::min(std::max(v, r.lo), r.hi);
    auto finish = [&] {
        int now[mr::kMax];
        restart_values(now);
        restart_state.note(now);
        clamp_tab_selection(cur_tab);
        if (keep) save();
    };
    // (#2000) Under DLSS / FSR 4, Resolution steps Auto ↔ Native ↔ real-size twelfths.
    if (r.value == &opts.resolution && temporal_sr_upscale()) {
        namespace ur = upscaler_rows;
        const bool fsr4 = opts.upscale == 9;
        const int cur_up = (opts.dlss_quality == 4) ? (fsr4 ? ur::FSR4_AA : ur::DLAA)
                                                    : (fsr4 ? ur::FSR4 : ur::DLSS);
        int new_up = cur_up, new_res = opts.resolution;
        ur::step_render(cur_up, opts.resolution, dir, &new_up, &new_res);
        opts.dlss = new_up;
        opts.resolution = new_res;
        if (new_up == ur::DLAA || new_up == ur::FSR4_AA) opts.dlss_quality = 4;
        else opts.dlss_quality = dlss_quality_from_percent(ur::percent(new_res));
        opts.preset = 0;
        sync_preset_budgets(0);
        temporal_reset_failure();
        finish();
        return;
    }
    if (r.value == &opts.dlss_quality && temporal_sr_upscale()) {
        namespace ur = upscaler_rows;
        const bool fsr4 = opts.upscale == 9;
        *r.value = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
        if (*r.value == 4) {
            opts.dlss = fsr4 ? ur::FSR4_AA : ur::DLAA;
        } else {
            static const int steps[] = {3, 5, 4, 6};  // Q/B/P/Ultra
            opts.dlss = fsr4 ? ur::FSR4 : ur::DLSS;
            opts.resolution = ur::snap_render(opts.dlss, steps[std::min(*r.value, 3)]);
        }
        opts.preset = 0;
        sync_preset_budgets(0);
        temporal_reset_failure();
        finish();
        return;
    }
    if (r.value == &opts.pad_layout) {
        if (v > 2) v = dir > 0 ? 0 : 2;
        if (v < 0) v = 2;
        *r.value = v;
        apply_pad_layout(*r.value);
    } else if (r.value == &opts.preset) {
        *r.value = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
        apply_preset(*r.value);
    } else if (r.value == &opts.color_preset) {
        *r.value = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
        if (*r.value <= 2) apply_color_preset(*r.value);
    } else if (r.value == &opts.framegen) {
        *r.value = v;
        if (*r.value >= 2 && opts.fg_backend == 0) opts.fg_backend = 1;
        opts.preset = 0;
        sync_preset_budgets(0);
    } else {
        *r.value = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
        if (r.value == &opts.fg_backend) {
            if (*r.value > 0 && opts.framegen < 2) opts.framegen = 2;
            if (*r.value == 0) opts.framegen = 0;
            clamp_framegen_steps();   // engine change: keep only that engine's steps
        }
        if (r.value == &opts.reflex) reflex_apply_mode(*r.value);
        if (r.value == &opts.mic) mic_apply();
        if (r.value == &opts.color_brightness || r.value == &opts.color_contrast ||
            r.value == &opts.color_saturation || r.value == &opts.color_gamma)
            opts.color_preset = 3;
        if (!r.preset_exempt) {
            if (r.value != &opts.fullscreen && r.value != &opts.show_fps && r.value != &opts.gpu_device &&
                r.value != &opts.menu_size && r.value != &opts.overlay_size &&
                r.value != &opts.invert_cam_x && r.value != &opts.invert_cam_y && r.value != &opts.mouse_native &&
                r.value != &opts.mouse_sens && r.value != &opts.stick_l_sens && r.value != &opts.stick_r_sens &&
                r.value != &opts.mouse_invert && r.value != &opts.reflex &&
                r.value != &opts.fov && r.value != &opts.text_speed && r.value != &opts.pad_background &&
                r.value != &opts.ms_pacer &&
                r.value != &opts.hybrid_cpu &&
                r.value != &opts.color_preset && r.value != &opts.color_brightness &&
                r.value != &opts.color_contrast && r.value != &opts.color_saturation &&
                r.value != &opts.color_gamma) {
                opts.preset = 0;
                sync_preset_budgets(0);
            }
        }
    }
    if (r.value == &opts.upscale || r.value == &opts.dlss || r.value == &opts.resolution ||
        r.value == &opts.output_res || r.value == &opts.dlss_quality || r.value == &opts.dlss_model ||
        r.value == &opts.fsr_quality)
        temporal_reset_failure();
    finish();
}

bool menu_is_open() { return open_now.load(std::memory_order_relaxed); }

void menu_note_frame(bool game_frame) {
    shown_frames.fetch_add(1, std::memory_order_relaxed);
    if (game_frame) game_frames.fetch_add(1, std::memory_order_relaxed);
    if (opts.show_fps >= 2) {
        const int64_t now = prof_now();
        std::lock_guard lk(ft_mu);
        if (ft_last_ns > 0 && now > ft_last_ns) {
            const float ms = std::clamp((float)((now - ft_last_ns) / 1e6), 0.1f, 200.0f);
            ft_ring[ft_head] = ms;
            ft_head = (ft_head + 1) % kFtSamples;
            if (ft_count < kFtSamples) ft_count++;
        }
        ft_last_ns = now;
    }
}

// The Fullscreen row and F11 are one setting: the row asks the window's thread (host/display.c), F11 shows in the row
void fullscreen_tick() {
    static int applied = 0, settle = 0;
    const HWND main_window = app_window();
    if (!main_window) return;
    static const bool env = getenv("HOS_FULLSCREEN") != nullptr;   // (a test's own choice: the row is left alone)
    if (env) return;
    if (opts.fullscreen != applied) {
        applied = opts.fullscreen;
        settle = 60;
        PostMessageA(main_window, WM_APP + 7, (WPARAM)applied, 0);
        return;
    }
    if (settle > 0) { settle--; return; }
    const int now = !(GetWindowLongA(main_window, GWL_STYLE) & WS_CAPTION);
    if (now != applied) {
        opts.fullscreen = applied = now;
        menu_refresh = true;
        save();
    }
}

/// A rate over the half-second that just ended. The first call only starts that interval: the frames since launch,
/// divided by one second, were shown as the rate the moment the counter was turned on.
bool fps_take(int64_t now, int64_t shown, int64_t game, int64_t* next, int64_t* last_shown, int64_t* last_game,
              double* shown_rate, double* game_rate) {
    if (now < *next) return false;
    const bool have = *next != 0;
    if (have) {
        const double secs = (now - *next + 500000000) / 1e9;
        *shown_rate = (shown - *last_shown) / secs;
        *game_rate = (game - *last_game) / secs;
    }
    *last_shown = shown;
    *last_game = game;
    *next = now + 500000000;
    return have;
}

void menu_option_set(int id, int value) {
    if (id != 7) return;
    opts.mouse_sens = std::min(std::max(value, 10), 500);   // (the keys work while the menu is closed: it shows it when opened)
    save();
}

void menu_tick() {
    fullscreen_tick();
    enum { K_N = 13 };
    static bool held[K_N];
    static const int keys[K_N] = {VK_INSERT, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_ESCAPE, VK_F1,   // (F1: for keyboards without Insert)
                                  VK_RETURN, VK_DELETE, VK_BACK,
                                  'Q', 'E', VK_TAB};   // (the tabs: Q and E, Tab and Shift+Tab)
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    const bool mine = pid == GetCurrentProcessId();
    bool press[K_N] = {};
    for (int i = 0; i < K_N; i++) {
        const bool down = mine && (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
        press[i] = down && !held[i];
        held[i] = down;
        if (press[i]) pad_last = false;
    }
    static uint32_t pad_held = 0;
    const uint32_t raw = mine && host && host->pad_raw ? host->pad_raw() : 0, pad_press = raw & ~pad_held;
    const uint32_t chord = 1u << HOS_PAD_BACK | 1u << HOS_PAD_START;
    const bool chord_now = (raw & chord) == chord && (pad_held & chord) != chord;
    pad_held = raw;
    if (pad_press) pad_last = true;
    {
        static const char* force = getenv("RX_VK_UI_PROMPTS");
        if (force) pad_last = !strcmp(force, "pad");
    }
    auto pad = [&](int p) { return listening != 1 && (pad_press >> p & 1) != 0; };
    if (opts.menu_shortcut == 1) press[0] = false;   // (F1 only: ignore keyboard Insert)
    if (!open_now.load() && opts.menu_shortcut == 2 && press[5]) press[6] = true;   // (Insert/F1/Esc: Esc opens when closed)
    if (pad(HOS_PAD_TOUCHPAD) && !touchpad_bound()) press[6] = true;
    if (host && host->binding)
        menu_keys::confirm_back(host->binding(HOS_BIND_PAD, HOS_SW_A, 0), host->binding(HOS_BIND_PAD, HOS_SW_B, 0),
                                &pad_confirm, &pad_back);
    if ((opts.pad_menu && pad(HOS_PAD_GUIDE)) || chord_now) press[0] = true;
    {   // holding an arrow or the D-pad repeats it: after 0.35 s, 12/s, then 30/s after 1.5 s held
        static int64_t held_since[5], next_repeat[5];
        const int pads[5] = {0, HOS_PAD_UP, HOS_PAD_DOWN, HOS_PAD_LEFT, HOS_PAD_RIGHT};
        const int64_t t = prof_now();
        for (int i = 1; i <= 4; i++) {
            const bool down = held[i] || (open_now.load() && listening != 1 && (raw >> pads[i] & 1));
            if (!down) { held_since[i] = 0; continue; }
            if (!held_since[i]) { held_since[i] = t, next_repeat[i] = t + 350000000LL; continue; }
            if (t >= next_repeat[i] && open_now.load() && listening == 0) {
                press[i] = true;
                next_repeat[i] = t + (t - held_since[i] > 1500000000LL ? 33000000LL : 83000000LL);
            }
        }
    }
    if (open_now.load() && listening != 1) {
        press[1] |= pad(HOS_PAD_UP), press[2] |= pad(HOS_PAD_DOWN), press[3] |= pad(HOS_PAD_LEFT);
        press[4] |= pad(HOS_PAD_RIGHT), press[5] |= pad(pad_back), press[7] |= pad(pad_confirm);
    }
    {   // RX_VK_MENU_SCRIPT (test runs): "<seconds>:<INS|UP|DOWN|LEFT|RIGHT|ESC|ENTER|TABL|TABR|PADS|PADE|PADW|PADN>,..."
        static const char* next = getenv("RX_VK_MENU_SCRIPT");
        static const int64_t start = prof_now();
        double at;
        char key[8];
        int n = 0;
        if (next && sscanf(next, "%lf:%7[A-Z]%n", &at, key, &n) == 2 && (prof_now() - start) / 1e9 >= at) {
            static const char* const names[] = {"INS", "UP", "DOWN", "LEFT", "RIGHT", "ESC", "", "ENTER", "", "", "TABL", "TABR"};
            for (int i = 0; i < 12; i++) press[i] |= *names[i] && !strcmp(key, names[i]);
            static const char* const pads[] = {"PADS", "PADE", "PADW", "PADN"};
            for (int p = 0; p < 4; p++)
                if (!strcmp(key, pads[p]) && open_now.load() && listening != 1) {
                    press[5] |= p == pad_back, press[7] |= p == pad_confirm;
                    pad_last = true;
                }
            next = next[n] == ',' ? next + n + 1 : nullptr;
        }
    }
    bool tab_left = press[10] || pad(HOS_PAD_LSHOULDER) || pad(HOS_PAD_LTRIGGER);
    bool tab_right = press[11] || press[12] || pad(HOS_PAD_RSHOULDER) || pad(HOS_PAD_RTRIGGER);
    if (press[12] && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) tab_left = true, tab_right = false;
    bool changed = menu_refresh;
    menu_refresh = false;
    MkwVkPointer ptr{};
    int mx = -1, my = -1;
    bool mouse = false, moved = false;
    if (open_now.load() && host && host->version >= 5 && host->pointer) {
        mouse = host->pointer(&ptr) != 0;
        int ix = 0, iy = 0;
        menu_hit::to_image(ptr.x, ptr.y, ptr.width, ptr.height, (int)dev.swap_extent.width, (int)dev.swap_extent.height,
                           &ix, &iy);
        if (mouse) mx = ix - (int)menu_ox, my = iy;
        static int last_x = -1, last_y = -1;
        moved = mouse && mouse_read && (mx != last_x || my != last_y);
        last_x = mx, last_y = my, mouse_read = true;
        if (moved) pad_last = false;
        if (!mouse) ptr.pressed = 0, ptr.wheel = 0;
    }
    auto later = [&] {
        int now[mr::kMax];
        restart_values(now);
        restart_state.choose_later(now);
        open_now = false, page = 0, changed = true;
    };
    if ((press[0] || press[6]) && open_now.load() && page == 5) {
        later();
    } else if ((press[0] || press[6]) && open_now.load() && restart_ask()) {
        page = 5, ask_sel = 0, listening = 0, changed = true;
    } else if (press[0] || press[6]) {
        open_now = !open_now.load(), changed = true;
        page = 0, listening = 0;
        if (open_now.load()) {
            const int64_t t0 = prof_now();
            opts.pad_layout = detect_pad_layout();
            mic_sync_hi();
            const int64_t dt = prof_now() - t0;
            if (dt > 50000000LL) logf("menu: the opening took %.0f ms", (double)dt / 1e6);
        }
    }
    if (open_now.load() && page == 5) {
        if (mouse) {
            const menu_hit::Hit hit = hits.at((float)mx, (float)my);
            if (hit.part == menu_hit::kButton && (moved || ptr.pressed) && hit.index != ask_sel) ask_sel = hit.index, changed = true;
            if ((ptr.pressed & HOS_MOUSE_LEFT) && hit.part == menu_hit::kButton) press[7] = true;
            if (ptr.pressed & HOS_MOUSE_RIGHT) press[5] = true;
        }
        if (press[1] || press[2] || press[3] || press[4] || tab_left || tab_right) ask_sel ^= 1, changed = true;
        if (press[5] || (press[7] && ask_sel == 0)) {
            later();
        } else if (press[7]) {
            logf("menu: restart now, to apply %s", restart_list(", ").c_str());
            save();
            pipeline_cache_save(true);
            open_now = false, page = 0, changed = true;
            if (app_window()) PostMessageA(app_window(), WM_APP + 9, 0, 0);
        }
    } else if (open_now.load() && page == 1) {
        using enum menu_hit::CtlColumn;
        if (!can_bind()) page = 0;
        else if (listening) {
            if (press[5] || prof_now() > listen_end) listening = 0, changed = true;
            else if (listening == 1) {
                if (!listen_armed) listen_armed = (raw & ~(1u << HOS_PAD_GUIDE)) == 0;
                else if (const uint32_t p = pad_press & ~(1u << HOS_PAD_GUIDE)) {
                    int bit = 0;
                    while (!(p >> bit & 1)) bit++;
                    host->bind(HOS_BIND_PAD, controls[csel].button, 0, bit);
                    listening = 0, changed = true;
                    opts.pad_layout = detect_pad_layout();
                    save();
                }
            } else {
                int down = 0;
                for (int k : bindable_keys())
                    if (mine && (GetAsyncKeyState(k) & 0x8000)) {
                        down = k;
                        break;
                    }
                if (!listen_armed) listen_armed = !down;
                else if (down) {
                    const int slot = ccol == kCtlKeyboard ? 1 : 0;
                    host->bind(HOS_BIND_KEY, controls[csel].button, slot, down);
                    listening = 0, changed = true;
                    save();
                }
            }
        } else {
            if (mouse) {
                const menu_hit::Hit hit = hits.at((float)mx, (float)my);
                const bool on = hit.part == menu_hit::kCtlRow || hit.part == menu_hit::kCtlCell;
                if (on && (moved || ptr.pressed)) {
                    if (hit.index != csel) csel = hit.index, scrolled_to[kControlsSlot] = csel, changed = true;
                    if (hit.part == menu_hit::kCtlCell) {
                        const int next_col = keys_only(csel) && hit.side == kCtlPrimary ? (int)kCtlKeyboard : hit.side;
                        if (ccol != next_col) ccol = next_col, changed = true;
                    }
                }
                if (ptr.pressed & HOS_MOUSE_LEFT) {
                    if (hit.part == menu_hit::kCtlCell || (hit.part == menu_hit::kCtlRow && hit.index == kControls))
                        press[7] = true;
                }
                if (ptr.pressed & HOS_MOUSE_RIGHT) press[5] = true;
                if (ptr.wheel) wheel_rows[kControlsSlot] += ptr.wheel, changed = true;
            }
            const int n = kControls + 1;
            if (press[5]) page = 0, changed = true;
            if (press[1]) csel = (csel + kControls) % n, changed = true;
            if (press[2]) csel = (csel + 1) % n, changed = true;
            if ((press[3] || press[4]) && csel < kControls) {
                if (keys_only(csel)) ccol = ccol == kCtlAlternate ? kCtlKeyboard : kCtlAlternate;
                else ccol = (ccol + kCtlColumns + (press[3] ? -1 : 1)) % kCtlColumns;
                changed = true;
            }
            if (keys_only(csel) && ccol == kCtlPrimary) ccol = kCtlKeyboard;
            if (press[7]) {
                if (csel == kControls) {
                    host->bind(HOS_BIND_PAD, -1, 0, 0);
                    opts.pad_layout = 0;
                    save();
                } else {
                    listening = ccol == kCtlPrimary ? 1 : 2;
                    listen_armed = false;
                    listen_end = prof_now() + 6000000000LL;
                }
                changed = true;
            }
            if ((press[8] || press[9]) && csel < kControls) {
                if (ccol == kCtlPrimary) {
                    host->bind(HOS_BIND_PAD, controls[csel].button, 0, -1);
                    opts.pad_layout = detect_pad_layout();
                } else {
                    host->bind(HOS_BIND_KEY, controls[csel].button, ccol == kCtlKeyboard ? 1 : 0, 0);
                }
                changed = true;
                save();
            }
        }
    } else if (open_now.load()) {
        if (mouse) {
            const menu_hit::Hit hit = hits.at((float)mx, (float)my);
            const int ht = hit.part == menu_hit::kTab     ? hit.index
                         : hit.part == menu_hit::kTabPrev ? TAB_COUNT
                         : hit.part == menu_hit::kTabNext ? TAB_COUNT + 1
                                                          : -1;
            if (ht != hover_tab) hover_tab = ht, changed = true;
            int& sel = tab_selected[cur_tab];
            const bool on_row = hit.part == menu_hit::kRow || hit.part == menu_hit::kSlider;
            if (drag_row >= 0) {
                Row& d = rows[tab_row_to_global(cur_tab, drag_row)];
                if (!(ptr.held & HOS_MOUSE_LEFT) || !d.value) {
                    drag_row = -1;
                    save();
                } else {
                    const int permille = hits.drag((float)mx, drag_row);
                    const int v = permille < 0 ? *d.value : menu_hit::slider_value(permille, d.lo, d.hi, d.step);
                    if (v != *d.value) set_row(d, v, v > *d.value ? 1 : -1, false), changed = true;
                }
            } else if (on_row && (moved || ptr.pressed) && hit.index != sel) {
                sel = hit.index, scrolled_to[cur_tab] = sel, changed = true;
            }
            if ((ptr.pressed & HOS_MOUSE_LEFT) && drag_row < 0) {
                if (hit.part == menu_hit::kTabPrev) tab_left = true;
                else if (hit.part == menu_hit::kTabNext) tab_right = true;
                else if (hit.part == menu_hit::kTab && hit.index != cur_tab) cur_tab = hit.index, changed = true;
                else if (hit.part == menu_hit::kSlider) {
                    Row& d = rows[tab_row_to_global(cur_tab, hit.index)];
                    drag_row = hit.index;
                    const int v = menu_hit::slider_value(hit.permille, d.lo, d.hi, d.step);
                    if (v != *d.value) set_row(d, v, v > *d.value ? 1 : -1, false);
                    changed = true;
                } else if (hit.part == menu_hit::kRow) {
                    Row& d = rows[tab_row_to_global(cur_tab, hit.index)];
                    switch (menu_hit::row_click(d.value != nullptr, d.hi - d.lo == d.step, hit.side, d.value && *d.value == d.hi)) {
                    case menu_hit::kClickConfirm: press[7] = true; break;
                    case menu_hit::kClickRight: press[4] = true; break;
                    case menu_hit::kClickLeft: press[3] = true; break;
                    case menu_hit::kClickSelect: break;
                    }
                }
            }
            if (ptr.pressed & HOS_MOUSE_RIGHT) press[5] = true;
            if (ptr.wheel && drag_row < 0) wheel_rows[cur_tab] += ptr.wheel, changed = true;
        }
        if (press[5]) {
            if (restart_ask()) page = 5, ask_sel = 0, changed = true;
            else open_now = false;
        }
        if (tab_left) cur_tab = (cur_tab + TAB_COUNT - 1) % TAB_COUNT, changed = true;
        else if (tab_right) cur_tab = (cur_tab + 1) % TAB_COUNT, changed = true;
        clamp_tab_selection(cur_tab);
        const int tab_count = std::max(1, tab_row_count(cur_tab));
        int& selected = tab_selected[cur_tab];
        if (press[1]) selected = (selected + tab_count - 1) % tab_count, changed = true;
        if (press[2]) selected = (selected + 1) % tab_count, changed = true;
        Row& r = rows[tab_row_to_global(cur_tab, selected)];
        if (press[7] && !r.value && !strcmp(r.name, "Controls") && can_bind()) {
            page = 1, csel = 0, ccol = menu_hit::kCtlPrimary, changed = true;
        } else if (press[7] && !r.value && !strcmp(r.name, "Quit to Desktop")) {
            save();
            if (const HWND window = app_window()) PostMessageA(window, WM_CLOSE, 0, 0);
            else ExitProcess(0);
        } else if ((press[3] || press[4] || press[7]) && r.value) {
            if (press[7]) press[4] = true;
            set_row(r, *r.value + (press[4] ? r.step : -r.step), press[4] ? 1 : -1);
            changed = true;
        }
        const int64_t now = prof_now();
        if (changed) {
            marquee_pos = 0, marquee_next = now + kMarqueeRest;
        } else if (marquee_max > 0 && now >= marquee_next) {
            if (marquee_pos >= marquee_max) marquee_pos = 0, marquee_next = now + kMarqueeRest;
            else if (++marquee_pos >= marquee_max) marquee_next = now + kMarqueeRest;
            else marquee_next = now + kMarqueeStep;
            changed = true;
        }
    }
    {   // (hookshot/#946) free the mouse while open; restore capture on close
        static bool told = false;
        const bool now_open = open_now.load();
        if (!now_open) {
            if (drag_row >= 0) save();
            hover_tab = -1, drag_row = -1, mouse_read = false;
        }
        if (now_open != told && app_window()) {
            told = now_open;
            PostMessageA(app_window(), WM_APP + 8, (WPARAM)now_open, 0);
        }
    }
    if (opts.show_fps) {
        static int64_t next = 0, last_game = 0, last_shown = 0;
        const int64_t now = prof_now();
        double shown = 0, game = 0;
        if (fps_take(now, shown_frames.load(), game_frames.load(), &next, &last_shown, &last_game, &shown, &game)) {
            snprintf(fps_text, sizeof fps_text, "%.0f", shown);
            if (opts_fg_on()) {
                if (const char* who = fg_short_name(opts.fg_backend)) snprintf(fps_sub, sizeof fps_sub, "Base %.0f, %s", game, who);
                else snprintf(fps_sub, sizeof fps_sub, "Base %.0f", game);
            } else {
                fps_sub[0] = 0;
            }
            if (opts.show_fps >= 2) {
                float sorted[kFtSamples];
                int n = 0;
                {
                    std::lock_guard lk(ft_mu);
                    n = ft_count;
                    const int oldest = (ft_head - n + kFtSamples) % kFtSamples;
                    for (int i = 0; i < n; i++) {
                        const float v = ft_ring[(oldest + i) % kFtSamples];
                        ft_snap[i] = v;
                        sorted[i] = v;
                    }
                    ft_snap_n = n;
                }
                if (n > 0) {
                    double sum = 0;
                    for (int i = 0; i < n; i++) sum += sorted[i];
                    const double avg_ms = sum / n;
                    std::sort(sorted, sorted + n);
                    const int p99_i = std::min(n - 1, std::max(0, (int)(n * 0.99)));
                    const double low1 = sorted[p99_i] > 0.01f ? 1000.0 / sorted[p99_i] : shown;
                    snprintf(fps_detail, sizeof fps_detail, "%.1f ms  1%% %.0f", avg_ms, low1);
                } else {
                    fps_detail[0] = 0;
                }
            } else {
                fps_detail[0] = 0;
            }
            if (opts.show_fps >= 4) {
                std::string e1;
                uint64_t used = 0, budget = 0;
                if (vram_usage(&used, &budget) && budget > 0) {
                    char vb[48];
                    snprintf(vb, sizeof vb, "VRAM %.1f/%.1f GB", used / 1073741824.0, budget / 1073741824.0);
                    e1 = vb;
                }
                if (opts.upscale > 0) {
                    if (!e1.empty()) e1 += " · ";
                    e1 += tx_upscale(opts.upscale);
                }
                if (dev.hdr) {
                    if (!e1.empty()) e1 += " · ";
                    e1 += (opts.hdr == 2 ? "HDR Native" : "HDR");
                }
                if (opts.crt_filter > 0 && !crt_native_pq()) {
                    if (!e1.empty()) e1 += " · ";
                    e1 += "CRT";
                }
                snprintf(fps_extra1, sizeof fps_extra1, "%s", e1.c_str());
                snprintf(fps_extra2, sizeof fps_extra2, "%.44s", dev.props.deviceName);
            } else {
                fps_extra1[0] = 0;
                fps_extra2[0] = 0;
            }
        }
    }
}

void menu_record(VkCommandBuffer cmd, uint32_t index, uint32_t w, uint32_t h) {
    const bool start = startup_on.load(std::memory_order_acquire);
    const bool fps = opts.show_fps && !start;
    const bool open = open_now.load(std::memory_order_relaxed);
    if (!start && !fps && !open) {
        if (prebake_needed(w, h) && ui::begin(w, h)) {
            ui::Ctx c = ui_ctx(w, h);
            prebake(c);
            ui::record(cmd, index, w, h);
            return;
        }
        ui::idle();
        return;
    }
    if (!ui::begin(w, h)) return;
    {
        static int64_t last = 0;
        const int64_t now = prof_now();
        ease_step = last ? std::min(1.0f, (float)((now - last) / 1e9) * 15.0f) : 1.0f;
        last = now;
    }
    ui::Ctx c = ui_ctx(w, h);
    prebake(c);
    if (start) draw_startup(c);
    if (open) draw_menu(c);
    if (fps) draw_fps(c, open);
    ui::record(cmd, index, w, h);
}

} // namespace mkwvk
