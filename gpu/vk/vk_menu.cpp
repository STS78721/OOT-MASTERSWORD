// The Insert menu. Drawn on the CPU (GDI text into a bitmap) and copied over the window's image at the last step of a present,
// so it costs nothing while closed and it never touches the game's picture, nor what frame interpolation sees. Keys: Insert
// opens and closes it, Up and Down choose, Left and Right change, Esc closes; the game gets no keyboard input while it is open.
// (hookshot) A gamepad's Home button, or Select and Start together, opens it too; the D-pad chooses and changes, the bottom
// face button changes too, the right one closes. The game gets no gamepad input while it is open either.
// Its Controls page binds the Switch's buttons to gamepad inputs and keys.
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "vk_paths.h"
#include "vk_dlss.h"
#include "vk_fsr3.h"
#include "vk_fsr4.h"
#include "vk_temporal.h"
#include "upscaler_rows.h"
#include "vk_menu.h"
#include "vk_menu_hit.h"
#include "menu_theme.h"
#include "vk_reflex.h"
#include "vk_startup_paint.h"
#include "vk_work.h"   // (after the others: counts the commands that do GPU work, for full_barrier)
#ifndef HOOKSHOT_VERSION   // (nx2/build.py gives it to this file: nx2/version.py)
#define HOOKSHOT_VERSION "unknown"
#endif

namespace mkwvk {
HWND app_window();

Options opts;

namespace {

enum MenuTab {
    TAB_GRAPHICS = 0,
    TAB_GAMEPLAY = 1,
    TAB_COLOUR = 2,
    TAB_CONTROLS = 3,
    TAB_COUNT = 4
};

struct Row {
    MenuTab tab;
    const char* name;
    int* value;
    int lo, hi, step;
    const char* (*text)(int v);
    const char* help;   // (shown under the list while the row is selected)
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
    static const char* t[] = {"OOT Mastersword", "Light", "Dark"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
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
    static const char* t[] = {"1920x1080 (restart)", "2560x1440 (restart)", "3840x2160 (restart)"};
    return t[std::min(std::max(v, 0), 2)];
}
const char* tx_speed(int v) { return v ? "Real time" : "Per frame"; }
const char* tx_text_speed(int v) {
    static const char* t[] = {"Normal", "2x", "4x", "Instant"};
    if ((unsigned)v >= sizeof t / sizeof t[0]) return t[0];
    return t[v];
}
const char* tx_fov(int v) { return v == 100 ? "100% (original)" : tx_percent(v); }
const char* tx_handheld(int v) { return v ? "On (restart)" : "Off (restart)"; }
const char* tx_ultrawide(int v) { return v ? "Monitor's shape (restart)" : "16:9 (restart)"; }
int gpu_device_at_start = 0;   // (the row's value the game started with: menu_set_gpu_count)
const char* tx_gpu(int v) {
    static char buf[300];
    const int idx = v - 1;
    if (v <= 0) snprintf(buf, sizeof buf, "Auto");
    else if (idx < (int)gpu_device_names.size()) snprintf(buf, sizeof buf, "%d: %s", idx, gpu_device_names[idx].c_str());
    else snprintf(buf, sizeof buf, "%d: Device %d", idx, idx);
    // (a GPU other than the one in use: it takes a restart, said where the choice is, as Handheld and Ultrawide say it.
    // The whole name shows: the selected row slides through it, see marquee_pos)
    if (v != gpu_device_at_start) strncat(buf, " (restart)", sizeof buf - strlen(buf) - 1);
    return buf;
}
const char* tx_pad_layout(int v);
const char* tx_robust(int v) {
    static const char* t[] = {"Auto (restart)", "Off (restart)", "On (restart)"};
    return (unsigned)v < 3 ? t[v] : t[0];
}
const char* tx_mesh_mode(int v) {
    static const char* t[] = {"Automatic (restart)", "Native (restart)", "Compute (restart)"};
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
    {TAB_GRAPHICS, "Menu theme", &opts.menu_theme, 0, 2, 1, tx_menu_theme,
     "OOT Mastersword: Lost Woods greens with Master Sword blue and gold. Light: Switch settings grey and cyan. "
     "Dark: black panels with red accents."},
    {TAB_GRAPHICS, "Quality preset", &opts.preset, 0, 5, 1, tx_preset,
     "Automatically configures resolution, upscaling, frame generation, and dynamic VRAM limits tailored to your GPU tier."},
    {TAB_GRAPHICS, "Fullscreen", &opts.fullscreen, 0, 1, 1, tx_onoff,
     "Borderless fullscreen at the monitor size; cursor auto-hides. Same as F11. Starts from this setting."},
    {TAB_GRAPHICS, "GPU device", &opts.gpu_device, 0, 8, 1, tx_gpu,
     "Selects which graphics card Vulkan uses. Auto picks the best dedicated GPU. Applies after restart."},
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
     "Shows each frame at an even interval instead of as soon as it is ready. Less stutter, at up to one frame of delay."},
    {TAB_GRAPHICS, "Mastersword Pacer (Experimental)", &opts.ms_pacer, 0, 1, 1, tx_onoff,
     "Experimental. Holds the game's own frames to an even beat. A frame more than 2 ms late starts a new beat, so the next one is not held for a full extra frame. Off uses the previous start-time cap."},
    {TAB_GRAPHICS, "Intel Hybrid CPU Patch", &opts.hybrid_cpu, 0, 1, 1, tx_hybrid,
     "Recommended for Intel hybrid CPUs: 12th, 13th and 14th gen Core and Core Ultra. Runs the game's threads on the "
     "performance cores (P-cores) and background work such as shader building on the efficiency cores (E-cores), like the "
     "Switch 2's six identical cores. Steadier frames and fewer glitches. No effect on CPUs with one kind of core (most AMD). "
     "Applies after a restart."},
    {TAB_GRAPHICS, "V-Sync", &opts.vsync, 0, 2, 1, tx_vsync,
     "On waits for the display refresh (FIFO). Mailbox shows the latest finished frame on the next refresh, without tearing. "
     "Off presents immediately and can tear. Changes at once. RX_VK_FIFO / RX_VK_IMMEDIATE still choose for a run."},
    {TAB_GRAPHICS, "NVIDIA Reflex", &opts.reflex, 0, 2, 1, tx_reflex,
     "Low-latency pacing on NVIDIA GPUs (VK_NV_low_latency2). Boost also raises GPU clocks. Hidden effect on AMD/Intel."},
    {TAB_GRAPHICS, "Resolution", &opts.resolution, 0, 4, 1, tx_resolution,
     "Internal 3D size as a percent of Output resolution (like a PC title). Auto lets the game lower it when the GPU is busy. "
     "With XeSS/FSR/Nodix selected, their quality row sets this live (e.g. FSR Quality = 67% of 1440p/4K)."},
    {TAB_GRAPHICS, "Output resolution", &opts.output_res, 0, 2, 1, tx_output_res,
     "The size the game draws at. 1080p, 1440p, or 4K. Applies after a restart. Match monitor overrides this."},
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
    {TAB_GRAPHICS, "FSR 4 quality", &opts.dlss_quality, 0, 4, 1, tx_fsr4_quality,
     "FSR 4 quality: same render-scale shares as DLSS (Quality 67% … Ultra Performance 33%), plus Native AA at full Output. "
     "Needs AMD's FSR 4 runtime and a GPU AMD's library lists an FSR 4 model for."},
    {TAB_GRAPHICS, "SMAA", &opts.smaa, 0, 1, 1, tx_onoff,
     "Subpixel Morphological Anti-Aliasing on the present (iryoku SMAA Medium). Softens shimmering edges after the game "
     "draws, before upscaling. The game's own title SMAA is separate."},
    {TAB_GRAPHICS, "Sharpness", &opts.sharpness, 0, 100, 10, tx_percent,
     "Sharpens the picture as it goes to the window, with or without an upscaling filter. Too much shows halos around edges."},
    {TAB_GRAPHICS, "Handheld profile", &opts.handheld, 0, 1, 1, tx_handheld,
     "Runs the game as if the console were undocked: lower resolutions, lighter on the GPU. Applies after a restart."},
    {TAB_GRAPHICS, "Ultrawide", &opts.ultrawide, 0, 1, 1, tx_ultrawide,
     "Monitor's shape widens the camera to fill a screen wider than 16:9. The HUD is still stretched. Applies after a restart."},
    {TAB_GRAPHICS, "Robust buffers", &opts.robust, 0, 2, 1, tx_robust,
     "Bounds-checked buffer reads: a read past a buffer's end is dropped instead of stopping the GPU. Auto is on, on every card; "
     "Off can be faster on some NVIDIA cards. Applies after restart."},
    {TAB_GRAPHICS, "Mesh shaders", &opts.mesh_mode, 0, 2, 1, tx_mesh_mode,
     "How the game's mesh programs (much of the scenery) run: on the GPU's own mesh shaders, or as compute shaders, which "
     "every GPU has. Automatic is the old behaviour. Applies after a restart."},
    {TAB_GRAPHICS, "Quit to Desktop", nullptr, 0, 0, 0, nullptr,
     "Saves options and closes MasterSword. Enter or the bottom face button quits."},

    // Gameplay
    {TAB_GAMEPLAY, "FPS counter", &opts.show_fps, 0, 1, 1, tx_onoff,
     "Shows frames per second in the top right corner. With frame generation on, it also shows the game's own rate."},
    {TAB_GAMEPLAY, "Game speed", &opts.realtime, 0, 1, 1, tx_speed,
     "Per frame: the game slows down whenever it runs below 60 fps. Real time: it keeps normal speed down to 15 fps by taking "
     "longer steps."},
    {TAB_GAMEPLAY, "Game frame rate cap", &opts.fps_cap, 0, 6, 1, tx_fps_cap,
     "Caps the game's own frames (not made ones). Uncapped: as fast as the PC can, still at real time; with V-Sync on, the display's "
     "refresh rate is the limit. Applies after a restart."},
    {TAB_GAMEPLAY, "Field of view", &opts.fov, 50, 150, 5, tx_fov,
     "How much of the world you see: 100% is the original view. Higher is wider, lower is closer. Changes apply at once "
     "during gameplay. Cutscenes keep their own view."},
    {TAB_GAMEPLAY, "Text speed", &opts.text_speed, 0, 3, 1, tx_text_speed,
     "How fast the letters of dialogue and other messages appear. Normal is the game's own pace, with its pauses. Instant shows "
     "each page at once, as pressing A does."},
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
    {TAB_CONTROLS, "Left stick speed", &opts.stick_l_sens, 10, 500, 10, tx_sens,
     "How far the left analog stick pushes movement for a given deflection. 100% is stock. Keyboard WASD is unchanged."},
    {TAB_CONTROLS, "Right stick speed", &opts.stick_r_sens, 10, 500, 10, tx_sens,
     "How far the right analog stick turns the camera for a given deflection. 100% is stock. Mouse look uses Mouse sensitivity."},
    {TAB_CONTROLS, "Invert mouse vertical", &opts.mouse_invert, 0, 1, 1, tx_onoff,
     "Swaps up and down for the mouse: moving it forward tilts the camera down."},
    {TAB_CONTROLS, "Invert camera X", &opts.invert_cam_x, 0, 1, 1, tx_onoff,
     "Inverts horizontal look (left/right) on the right analog stick."},
    {TAB_CONTROLS, "Invert camera Y", &opts.invert_cam_y, 0, 1, 1, tx_onoff,
     "Inverts vertical look (up/down) on the right analog stick."},
    {TAB_CONTROLS, "Layout preset", &opts.pad_layout, 0, 3, 1, tx_pad_layout,
     "Nintendo (default Switch order), Xbox / PC (A for action/talk), or Swap Face. Individual bindings switch this to Custom."},
    {TAB_CONTROLS, "Controls", nullptr, 0, 0, 0, nullptr,   // (hookshot: opens its page)
     "Which gamepad button and which keys press each of the Switch's buttons."},
};
constexpr int kRows = sizeof rows / sizeof rows[0];
constexpr int kMenuWidth = menu_hit::kWidth;   // (at a scale of 1: 720 lines; wider for FG engine / Nodix labels)

int cur_tab = TAB_GRAPHICS;
int tab_selected[TAB_COUNT] = {0, 0, 0, 0};

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

/// Sub-options only appear for the engine / upscaler they belong to (keeps Graphics readable).
bool row_visible(const Row& r) {
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
    return true;
}

int tab_row_count(int t) {
    int c = 0;
    for (int i = 0; i < kRows; i++)
        if (rows[i].tab == (MenuTab)t && row_visible(rows[i])) c++;
    return c;
}
/// The most rows a tab has: the menu keeps that height whichever tab is shown
int max_tab_rows() {
    int most = 0;
    for (int t = 0; t < TAB_COUNT; t++) most = std::max(most, tab_row_count(t));
    // Graphics can grow when FG/upscale sub-rows appear; keep room for the dense case.
    most = std::max(most, 16);
    return most;
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

std::atomic<bool> open_now{false};
bool menu_refresh = false;
// (hookshot/#946) mouse hit state while the Insert menu is open
int hover_tab = -1;
int row_value_l[kRows];
bool row_slider[kRows];
int drag_row = -1;
bool mouse_read = false;
bool is_slider(const Row& r) {
    return r.value == &opts.sharpness || r.value == &opts.mouse_sens || r.value == &opts.stick_l_sens ||
           r.value == &opts.stick_r_sens || r.value == &opts.fg_flow || r.value == &opts.fov ||
           (r.value && r.text == tx_percent && r.hi - r.lo >= 50 && r.step >= 5);
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
    static char k[32];
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
            if (r.value) fprintf(f, "%s=%d\n", ini_key(r), *r.value);
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
int page = 0;          // 0 the options, 1 the controls
int csel = 0, ccol = 0;   // the controls' row (kControls: reset) and column (0 the gamepad, 1 the keyboard)
int listening = 0;     // waiting for the new input: 1 a gamepad's, 2 a key
bool listen_armed = false;   // (once what started it was let go)
int64_t listen_end = 0;
std::string keys_text(int button) {
    const int a = host->binding(HOS_BIND_KEY, button, 0), b = host->binding(HOS_BIND_KEY, button, 1);
    std::string t = a ? key_name(a) : "";
    if (b) t += std::string(a ? ", " : "") + key_name(b);
    return t.empty() ? "None" : t;
}

// ---- what is drawn: bitmaps of the menu and the counter, in the window's byte order ----
struct Bitmap {
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> px;
    uint64_t version = 0;
    bool keyed = false;                   // (black is see-through: copied as the runs of pixels in between)
    std::vector<VkBufferImageCopy> runs;
};
Bitmap menu_bmp, fps_bmp;
std::mutex bmp_mu;
/// Menu panel height at scale s for `rows` option lines and `help_px` of wrapped help (matches redraw_menu).
int menu_panel_h(int s, int rows, int help_px) {
    using namespace menu_hit;
    // header + pad + rows + help gap + help text + footer
    return (kHeadH + kContentPad + rows * kRowH + 12) * s + help_px + (12 + kFootH) * s;
}
int measure_help(int s);   // (defined below: used when choosing a scale that still fits the swapchain)
/// Prefer h/720, then shrink until the densest tab + real help text fit. Windowed often fit by luck; fullscreen
/// 4K (scale 3) with Wave A/B's extra Graphics rows made the bitmap taller than the swapchain, and copy_over
/// skipped the draw (Insert open, blank).
int scale_of(uint32_t h) {
    int s = (int)std::max(1u, h / 720);
    const int rows = max_tab_rows();
    while (s > 1) {
        if (menu_panel_h(s, rows, measure_help(s)) <= (int)h) break;
        --s;
    }
    return s;
}

thread_local uint32_t* paint_bits = nullptr;   // (start-up screen blends its logo in: vk_startup_paint.h)
void gdi_draw(Bitmap& out, uint32_t w, uint32_t h, void (*paint)(HDC, uint32_t, uint32_t, int), int scale) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = (LONG)w;
    bi.bmiHeader.biHeight = -(LONG)h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(dc); return; }
    HGDIOBJ old = SelectObject(dc, bmp);
    paint_bits = (uint32_t*)bits;
    paint(dc, w, h, scale);
    paint_bits = nullptr;
    GdiFlush();
    const bool rgba = dev.swap_format == VK_FORMAT_R8G8B8A8_UNORM || dev.swap_format == VK_FORMAT_R8G8B8A8_SRGB;
    std::lock_guard lk(bmp_mu);
    out.w = w, out.h = h;
    out.px.assign((const uint8_t*)bits, (const uint8_t*)bits + (size_t)w * h * 4);
    if (out.keyed) {
        out.runs.clear();
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w;) {
                const uint8_t* p = out.px.data() + ((size_t)y * w + x) * 4;
                if (!(p[0] | p[1] | p[2])) { x++; continue; }
                uint32_t e = x + 1;
                for (; e < w; e++) {
                    const uint8_t* q = out.px.data() + ((size_t)y * w + e) * 4;
                    if (!(q[0] | q[1] | q[2])) break;
                }
                VkBufferImageCopy c{};
                c.bufferOffset = ((uint64_t)y * w + x) * 4;
                c.bufferRowLength = w;
                c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                c.imageOffset = {(int32_t)x, (int32_t)y, 0};
                c.imageExtent = {e - x, 1, 1};
                out.runs.push_back(c);
                x = e;
            }
        }
        // One strip per row was a copy of its own. At 4K that is a couple of thousand copies every present, and the
        // frame rate fell while the menu was open. A strip that continues the one above becomes a taller copy.
        if (out.runs.size() > 1) {
            std::vector<VkBufferImageCopy> stacked;
            stacked.reserve(out.runs.size());
            std::vector<uint32_t> above, row;
            int32_t row_y = out.runs[0].imageOffset.y - 1;
            for (const VkBufferImageCopy& c : out.runs) {
                if (c.imageOffset.y != row_y) {
                    above.swap(row);
                    row.clear();
                    row_y = c.imageOffset.y;
                }
                uint32_t onto = UINT32_MAX;
                for (uint32_t i : above) {
                    VkBufferImageCopy& p = stacked[i];
                    if (p.imageOffset.x != c.imageOffset.x || p.imageExtent.width != c.imageExtent.width) continue;
                    if (p.bufferRowLength != c.bufferRowLength) continue;
                    if (p.imageOffset.y + (int32_t)p.imageExtent.height != c.imageOffset.y) continue;
                    const uint64_t step = (uint64_t)p.bufferRowLength * 4ull * p.imageExtent.height;
                    if (p.bufferOffset + step != c.bufferOffset) continue;
                    onto = i;
                    break;
                }
                if (onto != UINT32_MAX) {
                    stacked[onto].imageExtent.height++;
                    row.push_back(onto);
                } else {
                    row.push_back((uint32_t)stacked.size());
                    stacked.push_back(c);
                }
            }
            out.runs.swap(stacked);
        }
    }
    for (size_t i = 0; i < out.px.size(); i += 4) {
        if (rgba) std::swap(out.px[i], out.px[i + 2]);
        out.px[i + 3] = 255;
    }
    out.version++;
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
}
HFONT font(int px, int weight) {
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, "Segoe UI");
}
int help_px = 0;   // (the help text's height: measure_help)
/// The help text word-wrapped to width, a TextOutA a line (the POSIX GDI stand-in has no DrawTextA), drawn at (x, y)
/// when draw: its height in pixels either way
int wrap_help(HDC dc, const char* text, int width, int x, int y, bool draw) {
    SIZE sz{};
    GetTextExtentPoint32A(dc, "Ag", 2, &sz);
    const int line_h = (int)sz.cy;
    int lines = 0;
    for (const char* p = text; *p;) {
        int n = 0;   // (the most whole words that fit; one word at least, however long)
        for (int end = 0;; end++) {
            while (p[end] && p[end] != ' ') end++;
            GetTextExtentPoint32A(dc, p, end, &sz);
            if (n && sz.cx > width) break;
            n = end;
            if (!p[end]) break;
        }
        if (draw) TextOutA(dc, x, y + lines * line_h, p, n);
        lines++;
        p += n;
        while (*p == ' ') p++;
    }
    return lines * line_h;
}
void fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r{x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}
void fill_round(HDC dc, int x, int y, int w, int h, int rad, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, x, y, x + w, y + h, rad * 2, rad * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}
void paint_menu(HDC dc, uint32_t w, uint32_t h, int s) {
    // Layout constants from vk_menu_hit.h (qualified so picture and hit-test stay locked).
    const int kHeadH = menu_hit::kHeadH, kSideW = menu_hit::kSideW, kFootH = menu_hit::kFootH;
    const int kTabH = menu_hit::kTabH, kRowH = menu_hit::kRowH, kTabY = menu_hit::kTabY;
    (void)kTabY;   // same edge as the sidebar body; kept for the hit-test lock
    (void)menu_hit::tab_slot_w(s, (int)w, TAB_COUNT);
    const menu_theme::Palette th = menu_theme::of(opts.menu_theme);
    // keyed black outside: rounded card (gdi_draw treats black as see-through when keyed)
    fill(dc, 0, 0, (int)w, (int)h, RGB(0, 0, 0));
    const int rad = 14 * s;
    fill_round(dc, 0, 0, (int)w, (int)h, rad, th.panel);
    SetBkMode(dc, TRANSPARENT);
    HFONT title = font(18 * s, FW_SEMIBOLD), body = font(15 * s, FW_NORMAL), small = font(12 * s, FW_NORMAL);
    HGDIOBJ old = SelectObject(dc, title);

    // ---- title bar ----
    fill(dc, 0, 0, (int)w, kHeadH * s, th.header);
    // re-round the top by covering the square header corners with panel colour… skipped: header fills full width
    fill(dc, 0, kHeadH * s - s, (int)w, s, th.divider);
    fill(dc, 16 * s, (int)(kHeadH * 0.28f) * s, 5 * s, (int)(kHeadH * 0.44f) * s, th.accent);   // accent bar
    SetTextColor(dc, th.text);
    TextOutA(dc, 28 * s, (kHeadH * s - 18 * s) / 2, "MasterSword", 11);
    {
        SIZE tsz{};
        GetTextExtentPoint32A(dc, "MasterSword", 11, &tsz);
        SelectObject(dc, small);
        SetTextColor(dc, th.text_soft);
        TextOutA(dc, 28 * s + tsz.cx + 12 * s, (kHeadH * s - 12 * s) / 2 + s, "Settings", 8);
        std::string ver = HOOKSHOT_VERSION;
        SIZE sz{};
        GetTextExtentPoint32A(dc, ver.c_str(), (int)ver.size(), &sz);
        const int room = (int)w - 40 * s - tsz.cx - 80 * s;
        if (sz.cx > room && ver.find(" (") != std::string::npos) {
            ver.resize(ver.find(" ("));
            GetTextExtentPoint32A(dc, ver.c_str(), (int)ver.size(), &sz);
        }
        SetTextColor(dc, th.text_dim);
        if (sz.cx <= (int)w - 32 * s) TextOutA(dc, (int)w - 16 * s - sz.cx, (kHeadH * s - 12 * s) / 2 + s, ver.c_str(), (int)ver.size());
        SelectObject(dc, title);
    }

    if (page == 1) {   // (hookshot) the Controls bindings page
        SelectObject(dc, body);
        SetTextColor(dc, th.text_soft);
        TextOutA(dc, 16 * s, 12 * s + 4 * s, "Controls", 8);
        const int rh = menu_hit::kCtlRowH * s, top = menu_hit::kCtlTop * s, xpad = menu_hit::kCtlPad * s,
                  xkey = menu_hit::kCtlKey * s;
        SetTextColor(dc, th.text_dim);
        TextOutA(dc, 16 * s, 44 * s, "Button", 6);
        TextOutA(dc, xpad, 44 * s, "Gamepad", 7);
        TextOutA(dc, xkey, 44 * s, "Keyboard", 8);
        for (int i = 0; i <= kControls; i++) {
            const int y = top + i * rh;
            const bool sel = i == csel;
            if (sel) fill_round(dc, 8 * s, y, (int)w - 16 * s, rh, 6 * s, th.row_sel);
            SetTextColor(dc, sel ? th.accent_text : th.text);
            if (i == kControls) {
                const char* t = "Reset to defaults";
                TextOutA(dc, 16 * s, y + 3 * s, t, (int)strlen(t));
                continue;
            }
            const Control& c = controls[i];
            TextOutA(dc, 16 * s, y + 3 * s, c.name, (int)strlen(c.name));
            for (int col = 0; col < 2; col++) {
                std::string v = col == 0 ? (keys_only(i) ? "-" : pad_name(host->binding(HOS_BIND_PAD, c.button, 0)))
                                         : keys_text(c.button);
                if (sel && col == ccol && listening) v = col == 0 ? "Press a button..." : "Press a key...";
                if (sel && col == ccol) {
                    SIZE sz{};
                    GetTextExtentPoint32A(dc, v.c_str(), (int)v.size(), &sz);
                    fill_round(dc, (col ? xkey : xpad) - 6 * s, y + 2 * s, (int)sz.cx + 12 * s, rh - 4 * s, 5 * s,
                               listening ? th.warn : th.accent);
                    SetTextColor(dc, th.header);
                } else {
                    SetTextColor(dc, sel ? th.accent_text : th.text);
                }
                TextOutA(dc, col ? xkey : xpad, y + 3 * s, v.c_str(), (int)v.size());
            }
        }
        SetTextColor(dc, th.text_dim);
        const char* hint = listening ? "Press the new one   Esc cancels"
                                     : "Enter binds   Delete clears   Esc goes back";
        const char* hint2 = "Gamepad: D-pad moves, bottom button binds, right one goes back";
        TextOutA(dc, 16 * s, top + (kControls + 1) * rh + 6 * s, hint, (int)strlen(hint));
        TextOutA(dc, 16 * s, top + (kControls + 1) * rh + 26 * s, hint2, (int)strlen(hint2));
        SelectObject(dc, old);
        DeleteObject(title);
        DeleteObject(body);
        DeleteObject(small);
        return;
    }

    // ---- sidebar categories (Switch settings) ----
    const int side = kSideW * s;
    const int body_y = kHeadH * s;
    const int foot_y = (int)h - kFootH * s;
    fill(dc, 0, body_y, side, foot_y - body_y, th.sidebar);
    fill(dc, side - s, body_y, s, foot_y - body_y, th.divider);
    const char* const tab_titles[TAB_COUNT] = {"Graphics", "Gameplay", "Colour", "Controls"};
    const int ih = kTabH * s, ix = 12 * s, iw = side - 24 * s, iy0 = body_y + 12 * s;
    HFONT tab_font = font(14 * s, FW_SEMIBOLD);
    SelectObject(dc, tab_font);
    for (int t = 0; t < TAB_COUNT; t++) {
        const int y = iy0 + t * (ih + 6 * s);
        const bool active = (t == cur_tab);
        const bool hov = (hover_tab == t);
        if (active) {
            fill_round(dc, ix, y, iw, ih, 8 * s, th.row_sel);
            fill(dc, ix + 6 * s, y + ih / 4, 4 * s, ih / 2, th.accent);
            SetTextColor(dc, th.accent_text);
        } else if (hov) {
            fill_round(dc, ix, y, iw, ih, 8 * s, th.row_hover);
            SetTextColor(dc, th.text);
        } else {
            SetTextColor(dc, th.text);
        }
        TextOutA(dc, ix + 20 * s, y + (ih - 14 * s) / 2, tab_titles[t], (int)strlen(tab_titles[t]));
    }
    // L / R pills (match hit-test at sidebar bottom)
    {
        SelectObject(dc, small);
        const int ph = (int)(kTabH * 0.7f) * s, py = foot_y - ph - 12 * s;
        auto pill = [&](int px, const char* letter) {
            SIZE ts{};
            GetTextExtentPoint32A(dc, letter, 1, &ts);
            const int pw = ts.cx + ph;
            fill_round(dc, px, py, pw, ph, ph / 2, th.text);
            SetTextColor(dc, th.header);
            TextOutA(dc, px + (pw - ts.cx) / 2, py + (ph - ts.cy) / 2, letter, 1);
            return pw;
        };
        int px = ix + 8 * s;
        px += pill(px, "L") + 8 * s;
        pill(px, "R");
    }
    DeleteObject(tab_font);

    // ---- option rows in the content pane ----
    SelectObject(dc, body);
    const int rh = kRowH * s, top = menu_hit::rows_top(s), cx = menu_hit::content_x(s);
    const int tab_count = tab_row_count(cur_tab);
    marquee_max = 0;
    for (int i = 0; i < tab_count; i++) {
        const Row& r = rows[tab_row_to_global(cur_tab, i)];
        const int y = top + i * rh;
        const bool sel = i == tab_selected[cur_tab];
        if (sel) {
            fill_round(dc, cx + 4 * s, y, (int)w - cx - 12 * s, rh, 8 * s, th.row_sel);
            // accent outline: thin bars on the selection card
            fill(dc, cx + 4 * s, y, (int)w - cx - 12 * s, s, th.accent);
            fill(dc, cx + 4 * s, y + rh - s, (int)w - cx - 12 * s, s, th.accent);
            fill(dc, cx + 4 * s, y, s, rh, th.accent);
            fill(dc, (int)w - 9 * s, y, s, rh, th.accent);
        }
        SetTextColor(dc, sel ? th.accent_text : th.text);
        TextOutA(dc, cx + 16 * s, y + (rh - 15 * s) / 2, r.name, (int)strlen(r.name));
        SIZE nsz{};
        GetTextExtentPoint32A(dc, r.name, (int)strlen(r.name), &nsz);
        const int name_right = cx + 16 * s + (int)nsz.cx + 12 * s;
        const int room = (int)w - menu_hit::kValueRight * s - name_right;
        std::string label;
        if (r.value) label = r.text(*r.value);
        else if (!strcmp(r.name, "Quit to Desktop")) label = sel ? "Enter" : "";
        else label = ">";
        const bool arrows = r.value && sel;
        auto width = [&](const std::string& t) {
            SIZE z{};
            GetTextExtentPoint32A(dc, t.c_str(), (int)t.size(), &z);
            return (int)z.cx;
        };
        auto framed = [&](const std::string& t) { return arrows ? "<  " + t + "  >" : t; };
        std::string v = framed(label);
        if (width(v) > room && arrows) {
            size_t start = label.size();
            while (start > 0 && width(framed(label.substr(start - 1))) <= room) start--;
            marquee_max = (int)start;
            const size_t from = (size_t)std::min(marquee_pos, marquee_max);
            size_t n = label.size() - from;
            while (n > 1 && width(framed(label.substr(from, n))) > room) n--;
            v = framed(label.substr(from, n));
        } else if (width(v) > room) {
            std::string t = label;
            while (t.size() > 1 && width(v) > room) {
                t.pop_back();
                while (!t.empty() && t.back() == ' ') t.pop_back();
                v = t + "...";
            }
        } else if (arrows) {
            marquee_max = 0;
        }
        SetTextColor(dc, sel ? th.accent_text : th.text_soft);
        TextOutA(dc, (int)w - menu_hit::kValueRight * s - width(v), y + (rh - 15 * s) / 2, v.c_str(), (int)v.size());
        row_value_l[i] =
            (int)w - menu_hit::kValueRight * s - std::min(r.value ? width(std::string("<  ") + label + "  >") : width(v), room);
        {
            const int tl = menu_hit::kSliderL * s, tr = menu_hit::kSliderR * s;
            row_slider[i] = is_slider(r) && name_right <= tl && row_value_l[i] >= tr + 8 * s;
            if (row_slider[i]) {
                const int ty = y + rh / 2 - s;
                const int knob = tl + (tr - tl) * menu_hit::slider_permille(*r.value, r.lo, r.hi) / 1000;
                fill(dc, tl, ty, tr - tl, 2 * s, th.track);
                fill(dc, tl, ty, std::max(0, knob - tl), 2 * s, th.track_fill);
                fill_round(dc, knob - 4 * s, y + rh / 2 - 6 * s, 8 * s, 12 * s, 4 * s, sel ? th.accent : th.text_soft);
            }
        }
    }

    // ---- help box + footer ----
    const int help_top = top + max_tab_rows() * rh + 8 * s;
    const int help_x = cx + 12 * s, help_w = (int)w - cx - 24 * s;
    fill_round(dc, help_x, help_top, help_w, help_px + 8 * s, 8 * s, th.help_bg);
    HFONT help = font(13 * s, FW_NORMAL);
    SelectObject(dc, help);
    SetTextColor(dc, th.text_soft);
    wrap_help(dc, rows[tab_row_to_global(cur_tab, tab_selected[cur_tab])].help, help_w - 16 * s, help_x + 8 * s,
              help_top + 4 * s, true);
    fill(dc, 0, foot_y, (int)w, (int)h - foot_y, th.header);
    fill(dc, 0, foot_y, (int)w, s, th.divider);
    SelectObject(dc, small);
    SetTextColor(dc, th.text_dim);
    const char* hint = "Arrows change    Q/E or L/R: categories    Insert / F1 / Home closes";
    TextOutA(dc, 16 * s, foot_y + (kFootH * s - 12 * s) / 2, hint, (int)strlen(hint));
    SelectObject(dc, old);
    DeleteObject(title);
    DeleteObject(body);
    DeleteObject(small);
    DeleteObject(help);
}
char fps_text[48] = "";
char fps_sub[32] = "";
/// The usual overlay: a bold green number with a dark edge, nothing behind it. Hard-edged letters: see-through pixels are copied or not.
void outlined(HDC dc, int x, int y, const char* text, COLORREF colour, int edge) {
    SetTextColor(dc, RGB(6, 8, 6));
    const int n = (int)strlen(text);
    for (int dy = -edge; dy <= edge; dy++)
        for (int dx = -edge; dx <= edge; dx++)
            if (dx || dy) TextOutA(dc, x + dx, y + dy, text, n);
    SetTextColor(dc, colour);
    TextOutA(dc, x, y, text, n);
}
HFONT hard_font(int px, int weight) {
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH, "Segoe UI");
}
void paint_fps(HDC dc, uint32_t w, uint32_t h, int s) {
    fill(dc, 0, 0, (int)w, (int)h, RGB(0, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_RIGHT | TA_TOP);
    const COLORREF green = RGB(72, 255, 110);
    const int right = (int)w - 2 * s;
    HFONT label = hard_font(13 * s, FW_BOLD), big = hard_font(28 * s, FW_BOLD);
    HGDIOBJ old = SelectObject(dc, label);
    outlined(dc, right, 14 * s, "FPS", green, s);
    SelectObject(dc, big);
    outlined(dc, right - 30 * s, 0, fps_text, green, s);
    if (fps_sub[0]) {
        SelectObject(dc, label);
        outlined(dc, right, 32 * s, fps_sub, RGB(226, 232, 226), s);
    }
    SelectObject(dc, old);
    DeleteObject(label);
    DeleteObject(big);
}
/// The tallest help text at scale s, in pixels: the menu keeps that height whichever row is selected
int measure_help(int s) {
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT f = font(13 * s, FW_NORMAL);
    HGDIOBJ old = SelectObject(dc, f);
    int most = 0;
    const int help_w = (kMenuWidth - menu_hit::kSideW - 40) * s;
    for (const Row& r : rows) most = std::max(most, wrap_help(dc, r.help, help_w, 0, 0, false));
    SelectObject(dc, old);
    DeleteObject(f);
    DeleteDC(dc);
    return most;
}
void redraw_menu(int s) {
    menu_bmp.keyed = true;   // rounded card: black outside is see-through
    if (page == 1) {
        uint32_t ch = (uint32_t)((64 + (kControls + 1) * 24 + 52) * s);
        if (dev.swap_extent.height && ch > dev.swap_extent.height) ch = dev.swap_extent.height;
        gdi_draw(menu_bmp, (uint32_t)(menu_hit::kCtlWidth * s), ch, paint_menu, s);
        return;
    }
    // Fit the panel inside the current swapchain. measure_help can exceed scale_of's earlier guess after a
    // fullscreen resize (windowed → 4K), which used to leave menu_bmp taller than the image and invisible.
    const uint32_t max_h = dev.swap_extent.height ? dev.swap_extent.height : 2160u;
    const int rows = max_tab_rows();
    while (s > 1) {
        help_px = measure_help(s);
        if ((uint32_t)menu_panel_h(s, rows, help_px) <= max_h) break;
        --s;
    }
    help_px = measure_help(s);
    uint32_t mh = (uint32_t)menu_panel_h(s, rows, help_px);
    if (mh > max_h) {
        help_px = std::max(0, (int)max_h - menu_panel_h(s, rows, 0));
        mh = max_h;
    }
    gdi_draw(menu_bmp, (uint32_t)(kMenuWidth * s), mh, paint_menu, s);
}

std::atomic<int64_t> game_frames{0}, shown_frames{0};
int cur_scale = 1;

struct Upload {
    VkBuffer buf = VK_NULL_HANDLE;
    VmaAllocation alloc = nullptr;
    void* map = nullptr;
    uint64_t size = 0, version = 0;
};
Upload ups[3][3];   // (menu, FPS counter, start-up screen; a frame in flight's each)

void copy_over(VkCommandBuffer cmd, VkImage dst, Bitmap& b, Upload& u, int x, int y, uint32_t w, uint32_t h) {
    std::lock_guard lk(bmp_mu);
    // Negative origin (panel taller/wider than the swapchain) used to cast to a huge uint and skip the draw.
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (!b.w || !b.h || (uint32_t)x >= w || (uint32_t)y >= h) return;
    // Clip instead of refusing: a fullscreen resize can leave one frame's bitmap slightly oversized.
    const uint32_t cw = std::min(b.w, w - (uint32_t)x);
    const uint32_t ch = std::min(b.h, h - (uint32_t)y);
    const uint64_t size = (uint64_t)b.w * b.h * 4;
    if (u.size < size) {
        if (!create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, u.buf, u.alloc, &u.map)) return;
        u.size = size, u.version = 0;
    }
    if (u.version != b.version) std::memcpy(u.map, b.px.data(), size), u.version = b.version;
    if (b.keyed) {
        static thread_local std::vector<VkBufferImageCopy> at;
        at.clear();
        for (auto r : b.runs) {
            r.imageOffset.x += x;
            r.imageOffset.y += y;
            if (r.imageOffset.x < 0 || r.imageOffset.y < 0) continue;
            if ((uint32_t)r.imageOffset.x >= w || (uint32_t)r.imageOffset.y >= h) continue;
            const uint32_t rw = std::min(r.imageExtent.width, w - (uint32_t)r.imageOffset.x);
            const uint32_t rh = std::min(r.imageExtent.height, h - (uint32_t)r.imageOffset.y);
            if (!rw || !rh) continue;
            r.imageExtent.width = rw;
            r.imageExtent.height = rh;
            at.push_back(r);
        }
        if (!at.empty()) vkCmdCopyBufferToImage(cmd, u.buf, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)at.size(), at.data());
        return;
    }
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageOffset = {x, y, 0};
    c.imageExtent = {cw, ch, 1};
    vkCmdCopyBufferToImage(cmd, u.buf, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
}

// ---- start-up prewarm screen (vk_startup_paint.h): centred on black ----
StartupView startup;
Bitmap startup_bmp;
std::atomic<bool> startup_on{false};
void paint_startup(HDC dc, uint32_t w, uint32_t h, int pct) { paint_startup_view(dc, w, h, pct, startup, paint_bits); }

} // namespace

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

void startup_screen(const StartupView* view) {
    if (!view) {
        startup_on.store(false, std::memory_order_release);
        return;
    }
    static int drawn_pct = 0;
    const int pct = startup_scale(dev.swap_extent.width, dev.swap_extent.height);
    StartupView v = *view;
    if (v.version.empty()) v.version = version_text(HOOKSHOT_VERSION);
    const double span = (kStartupW - 120) * pct / 100.0;
    if (v.fraction >= 0) v.fraction = std::floor(std::min(v.fraction, 1.0) * span) / span;
    if (pct != drawn_pct || !(v == startup)) {
        startup = v;
        drawn_pct = pct;
        gdi_draw(startup_bmp, (uint32_t)(kStartupW * pct / 100), (uint32_t)(kStartupH * pct / 100), paint_startup, pct);
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
        char line[80];
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
                if (r.value && !strcmp(line, ini_key(r))) {
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
        // (v9) Menu theme: 0 OOT Mastersword (default), 1 Light, 2 Dark. First load of v9 takes OOT;
        // after that the player's own Menu theme row is kept (saved as options_version 9).
        if (options_ver < 9) opts.menu_theme = 0;
        opts.menu_theme = std::min(std::max(opts.menu_theme, 0), 2);
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
    for (Row& r : rows) {
        if (r.value == &opts.gpu_device) {
            r.hi = count;
            if (opts.gpu_device > count) opts.gpu_device = 0;
        }
    }
    gpu_device_at_start = opts.gpu_device;
}


/// A row's new value v (dir: 1 up, -1 down); keep=false defers save (slider drag)
void set_row(Row& r, int v, int dir, bool keep = true) {
    if (!r.value) return;
    if (r.value == &opts.framegen) v = snap_framegen(v, dir);
    if (r.value == &opts.vsync) v = vsync_cycle(*r.value, dir);
    if (r.value == &opts.mouse_sens || r.value == &opts.stick_l_sens || r.value == &opts.stick_r_sens ||
        r.value == &opts.fov)
        v = std::min(std::max(v, r.lo), r.hi);
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
        clamp_tab_selection(cur_tab);
        if (keep) save();
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
        clamp_tab_selection(cur_tab);
        if (keep) save();
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
        if (r.value == &opts.color_brightness || r.value == &opts.color_contrast ||
            r.value == &opts.color_saturation || r.value == &opts.color_gamma)
            opts.color_preset = 3;
        if (r.value != &opts.fullscreen && r.value != &opts.show_fps && r.value != &opts.gpu_device &&
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
    if (r.value == &opts.upscale || r.value == &opts.dlss || r.value == &opts.resolution ||
        r.value == &opts.output_res || r.value == &opts.dlss_quality || r.value == &opts.dlss_model ||
        r.value == &opts.fsr_quality)
        temporal_reset_failure();
    clamp_tab_selection(cur_tab);
    if (keep) save();
}

bool menu_is_open() { return open_now.load(std::memory_order_relaxed); }

void menu_note_frame(bool game_frame) {
    shown_frames.fetch_add(1, std::memory_order_relaxed);
    if (game_frame) game_frames.fetch_add(1, std::memory_order_relaxed);
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
    }
    // (hookshot) the gamepads, by position: Home, or Select and Start together, as Insert; the D-pad as the arrows,
    // the bottom face button as Enter, the right one as Esc (not while a binding waits for its button)
    static uint32_t pad_held = 0;
    const uint32_t raw = mine && host && host->pad_raw ? host->pad_raw() : 0, pad_press = raw & ~pad_held;
    const uint32_t chord = 1u << HOS_PAD_BACK | 1u << HOS_PAD_START;
    const bool chord_now = (raw & chord) == chord && (pad_held & chord) != chord;
    pad_held = raw;
    auto pad = [&](int p) { return listening != 1 && (pad_press >> p & 1) != 0; };
    if (pad(HOS_PAD_GUIDE) || chord_now) press[0] = true;
    if (pad(HOS_PAD_TOUCHPAD) && !touchpad_bound()) press[6] = true;
    if (open_now.load() && listening != 1) {
        press[1] |= pad(HOS_PAD_UP), press[2] |= pad(HOS_PAD_DOWN), press[3] |= pad(HOS_PAD_LEFT);
        press[4] |= pad(HOS_PAD_RIGHT), press[5] |= pad(HOS_PAD_EAST), press[7] |= pad(HOS_PAD_SOUTH);
    }
    bool tab_left = press[10] || pad(HOS_PAD_LSHOULDER) || pad(HOS_PAD_LTRIGGER);
    bool tab_right = press[11] || press[12] || pad(HOS_PAD_RSHOULDER) || pad(HOS_PAD_RTRIGGER);
    if (press[12] && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) tab_left = true, tab_right = false;
    const int scale = scale_of(dev.swap_extent.height);
    // Fullscreen / resize: swapchain size can change without the preferred scale changing; still remake so the
    // panel is clamped to the new image (otherwise Insert stays blank after Fullscreen On at 4K).
    static uint32_t last_swap_w = 0, last_swap_h = 0;
    const bool swap_changed = last_swap_w != dev.swap_extent.width || last_swap_h != dev.swap_extent.height;
    last_swap_w = dev.swap_extent.width, last_swap_h = dev.swap_extent.height;
    bool changed = scale != cur_scale || menu_refresh || (open_now.load() && swap_changed);
    menu_refresh = false;
    cur_scale = scale;
    // (hookshot/#946) free mouse while the Insert menu is open
    MkwVkPointer ptr{};
    int mx = -1, my = -1, mw = 0, mh = 0;
    bool mouse = false, moved = false;
    if (open_now.load() && host && host->version >= 5 && host->pointer) {
        mouse = host->pointer(&ptr) != 0;
        {
            std::lock_guard lk(bmp_mu);
            mw = (int)menu_bmp.w, mh = (int)menu_bmp.h;
        }
        int ix = 0, iy = 0, x0 = 0, y0 = 0;
        menu_hit::to_image(ptr.x, ptr.y, ptr.width, ptr.height, (int)dev.swap_extent.width, (int)dev.swap_extent.height,
                           &ix, &iy);
        menu_hit::origin((int)dev.swap_extent.width, (int)dev.swap_extent.height, mw, mh, &x0, &y0);
        if (mouse) mx = ix - x0, my = iy - y0;
        static int last_x = -1, last_y = -1;
        moved = mouse && mouse_read && (mx != last_x || my != last_y);
        last_x = mx, last_y = my, mouse_read = true;
        if (!mouse) ptr.pressed = 0, ptr.wheel = 0;
    }
    if (press[0] || press[6]) {
        open_now = !open_now.load(), changed = true;
        page = 0, listening = 0;
        // (the layout row shows the controls as they are: host/nn_hid.c keeps them, the ini's pad_ lines, and a saved
        // layout is not applied over them at start)
        if (open_now.load()) opts.pad_layout = detect_pad_layout();
    }
    if (open_now.load() && page == 1) {   // (hookshot) the Controls page
        if (!can_bind()) page = 0;
        else if (listening) {
            if (press[5] || prof_now() > listen_end) listening = 0, changed = true;   // (Esc, or nothing for a while)
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
                    host->bind(HOS_BIND_KEY, controls[csel].button, 0, down);
                    host->bind(HOS_BIND_KEY, controls[csel].button, 1, 0);
                    listening = 0, changed = true;
                    save();
                }
            }
        } else {
            if (mouse) {
                const menu_hit::Hit hit = menu_hit::controls(mx, my, scale, mw, mh, kControls);
                const bool on = hit.part == menu_hit::kCtlRow || hit.part == menu_hit::kCtlCell;
                if (on && (moved || ptr.pressed)) {
                    if (hit.index != csel) csel = hit.index, changed = true;
                    if (hit.part == menu_hit::kCtlCell && !keys_only(csel) && ccol != hit.side) ccol = hit.side, changed = true;
                }
                if (ptr.pressed & HOS_MOUSE_LEFT) {
                    if (hit.part == menu_hit::kCtlCell || (hit.part == menu_hit::kCtlRow && hit.index == kControls))
                        press[7] = true;
                }
                if (ptr.pressed & HOS_MOUSE_RIGHT) press[5] = true;
                if (ptr.wheel) csel = ((csel - ptr.wheel) % (kControls + 1) + kControls + 1) % (kControls + 1), changed = true;
            }
            if (press[5]) page = 0, changed = true;
            if (press[1]) csel = (csel + kControls) % (kControls + 1), changed = true;
            if (press[2]) csel = (csel + 1) % (kControls + 1), changed = true;
            if ((press[3] || press[4]) && csel < kControls && !keys_only(csel)) ccol ^= 1, changed = true;
            if (keys_only(csel)) ccol = 1;   // (the stick's and the camera's directions: keys only)
            if (press[7]) {
                if (csel == kControls) {
                    host->bind(HOS_BIND_PAD, -1, 0, 0);
                    opts.pad_layout = 0;
                    save();
                }
                else listening = ccol ? 2 : 1, listen_armed = false, listen_end = prof_now() + 6000000000LL;
                changed = true;
            }
            if ((press[8] || press[9]) && csel < kControls) {
                if (ccol == 0) {
                    host->bind(HOS_BIND_PAD, controls[csel].button, 0, -1);
                    opts.pad_layout = detect_pad_layout();
                }
                else for (int k = 0; k < 2; k++) host->bind(HOS_BIND_KEY, controls[csel].button, k, 0);
                changed = true;
                save();
            }
        }
        if (changed) redraw_menu(scale);
    } else if (open_now.load()) {
        if (mouse) {
            const int count = tab_row_count(cur_tab);
            const menu_hit::Hit hit = menu_hit::options(mx, my, scale, mw, mh, TAB_COUNT, count, row_value_l, row_slider);
            const int ht = hit.part == menu_hit::kTab ? hit.index : -1;
            if (ht != hover_tab) hover_tab = ht, changed = true;
            int& sel = tab_selected[cur_tab];
            const bool on_row = hit.part == menu_hit::kRow || hit.part == menu_hit::kSlider;
            if (drag_row >= 0) {
                Row& d = rows[tab_row_to_global(cur_tab, drag_row)];
                if (!(ptr.held & HOS_MOUSE_LEFT) || !d.value) {
                    drag_row = -1;
                    save();
                } else {
                    const int v = menu_hit::slider_value(menu_hit::slider_drag(mx, scale), d.lo, d.hi, d.step);
                    if (v != *d.value) set_row(d, v, v > *d.value ? 1 : -1, false), changed = true;
                }
            } else if (on_row && (moved || ptr.pressed) && hit.index != sel) {
                sel = hit.index, changed = true;
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
                    if (!d.value) press[7] = true;
                    else if (d.hi - d.lo == d.step || hit.side >= 0) press[4] = true;
                    else press[3] = true;
                }
            }
            if (ptr.pressed & HOS_MOUSE_RIGHT) press[5] = true;
            if (ptr.wheel && drag_row < 0) {
                Row& d = rows[tab_row_to_global(cur_tab, sel)];
                const bool on_value = on_row && hit.index == sel && d.value &&
                                      (hit.part == menu_hit::kSlider || hit.side != 0);
                if (hit.part == menu_hit::kTab || hit.part == menu_hit::kTabPrev || hit.part == menu_hit::kTabNext) {
                    if (ptr.wheel > 0) tab_left = true;
                    else tab_right = true;
                } else if (on_value) {
                    const int dir = ptr.wheel > 0 ? 1 : -1;
                    for (int n = ptr.wheel > 0 ? ptr.wheel : -ptr.wheel; n > 0; n--) {
                        const int v = *d.value + dir * d.step;
                        if (is_slider(d) && (v < d.lo || v > d.hi)) break;
                        set_row(d, v, dir);
                    }
                    changed = true;
                } else if (on_row || hit.part == menu_hit::kNone) {
                    sel = ((sel - ptr.wheel) % count + count) % count, changed = true;
                }
            }
        }
        if (press[5]) open_now = false;
        // (the tabs: LB and RB, Q and E, or Tab)
        if (tab_left) cur_tab = (cur_tab + TAB_COUNT - 1) % TAB_COUNT, changed = true;
        else if (tab_right) cur_tab = (cur_tab + 1) % TAB_COUNT, changed = true;
        clamp_tab_selection(cur_tab);
        const int tab_count = std::max(1, tab_row_count(cur_tab));
        int& selected = tab_selected[cur_tab];
        if (press[1]) selected = (selected + tab_count - 1) % tab_count, changed = true;
        if (press[2]) selected = (selected + 1) % tab_count, changed = true;
        Row& r = rows[tab_row_to_global(cur_tab, selected)];
        if (press[7] && !r.value && !strcmp(r.name, "Controls") && can_bind())
            page = 1, csel = 0, ccol = 0, changed = true;
        if ((press[3] || press[4] || press[7]) && !r.value && !strcmp(r.name, "Quit to Desktop")) {
            save();
            ExitProcess(0);
        }
        if ((press[3] || press[4] || press[7]) && r.value) {   // (Enter, or the bottom face button, as Right)
            if (press[7]) press[4] = true;
            if (r.value == &opts.resolution && temporal_sr_upscale()) {
                set_row(r, *r.value, press[4] ? 1 : -1);
                changed = true;
            } else if (r.value == &opts.dlss_quality && temporal_sr_upscale()) {
                int v = *r.value + (press[4] ? r.step : -r.step);
                v = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
                set_row(r, v, press[4] ? 1 : -1);
                changed = true;
            } else {
            int v = *r.value + (press[4] ? r.step : -r.step);
            if (r.value == &opts.vsync) v = vsync_cycle(*r.value, press[4] ? 1 : -1);
            if (r.value == &opts.framegen) v = snap_framegen(v, press[4] ? 1 : -1);
            if (r.value == &opts.mouse_sens || r.value == &opts.stick_l_sens || r.value == &opts.stick_r_sens)
                v = std::min(std::max(v, r.lo), r.hi);   // (a scale: it stops at its ends)
            if (r.value == &opts.pad_layout) {
                if (v > 2) v = press[4] ? 0 : 2;
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
                if (*r.value >= 2 && opts.fg_backend == 0) opts.fg_backend = 1;   // mult with engine Off → Hookshot
                opts.preset = 0;
                sync_preset_budgets(0);
            } else {
                *r.value = v > r.hi ? r.lo : v < r.lo ? r.hi : v;
                if (r.value == &opts.fg_backend) {
                    if (*r.value > 0 && opts.framegen < 2) opts.framegen = 2;
                    if (*r.value == 0) opts.framegen = 0;
                    clamp_framegen_steps();   // engine change: keep only that engine's steps
                }
                if (r.value == &opts.color_brightness || r.value == &opts.color_contrast ||
                    r.value == &opts.color_saturation || r.value == &opts.color_gamma)
                    opts.color_preset = 3;   // Custom
                if (r.value != &opts.fullscreen && r.value != &opts.show_fps && r.value != &opts.gpu_device &&
                    r.value != &opts.invert_cam_x && r.value != &opts.invert_cam_y && r.value != &opts.mouse_native &&
                    r.value != &opts.mouse_sens && r.value != &opts.stick_l_sens && r.value != &opts.stick_r_sens &&
                    r.value != &opts.mouse_invert &&
                    r.value != &opts.ms_pacer &&
                    r.value != &opts.hybrid_cpu &&
                    r.value != &opts.color_preset && r.value != &opts.color_brightness &&
                    r.value != &opts.color_contrast && r.value != &opts.color_saturation &&
                    r.value != &opts.color_gamma) {   // (the controls / colour are not the quality preset's)
                    opts.preset = 0; // Custom
                    sync_preset_budgets(0);
                }
            }
            clamp_tab_selection(cur_tab);   // (FG / upscale sub-rows appear or hide)
            changed = true;
            save();
            }  // else not DLSS Resolution / quality via set_row
        }
        const int64_t now = prof_now();
        if (changed) {   // (a new row or value: its name from the start, after a rest)
            marquee_pos = 0, marquee_next = now + kMarqueeRest;
        } else if (marquee_max > 0 && now >= marquee_next) {
            if (marquee_pos >= marquee_max) marquee_pos = 0, marquee_next = now + kMarqueeRest;   // (back to the start)
            else if (++marquee_pos >= marquee_max) marquee_next = now + kMarqueeRest;            // (rest at the end)
            else marquee_next = now + kMarqueeStep;
            changed = true;
        }
        if (changed) redraw_menu(scale);
    }
    if (opts.show_fps) {
        static int64_t next = 0, last_game = 0, last_shown = 0;
        const int64_t now = prof_now();
        double shown = 0, game = 0;
        if (fps_take(now, shown_frames.load(), game_frames.load(), &next, &last_shown, &last_game, &shown, &game)) {
            snprintf(fps_text, sizeof fps_text, "%.0f", shown);
            if (opts_fg_on()) snprintf(fps_sub, sizeof fps_sub, "game %.0f", game);
            else fps_sub[0] = 0;
            fps_bmp.keyed = true;
            gdi_draw(fps_bmp, 120u * scale, 54u * scale, paint_fps, scale);
        }
    }
}

void menu_record(VkCommandBuffer cmd, VkImage dst, int slot, uint32_t w, uint32_t h) {
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
    if (startup_on.load(std::memory_order_acquire)) {
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        mb.srcAccessMask = mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1, di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &di);
        std::unique_lock lk(bmp_mu);
        const uint32_t sw = startup_bmp.w, sh = startup_bmp.h;
        lk.unlock();
        if (sw) copy_over(cmd, dst, startup_bmp, ups[2][slot], (int)(w - sw) / 2, (int)(h - sh) / 2, w, h);
    }
    if (opts.show_fps && !startup_on.load(std::memory_order_relaxed)) {
        // The game image was just blitted (or cleared) into dst. Its write must
        // finish before the FPS pixels overwrite it; command order alone does
        // not order these transfer writes. The menu below has its own barrier.
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        mb.srcAccessMask = mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1, di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &di);
        std::unique_lock lk(bmp_mu);
        const uint32_t fw = fps_bmp.w;
        lk.unlock();
        if (fw) copy_over(cmd, dst, fps_bmp, ups[1][slot], (int)w - (int)fw - (int)(12 * cur_scale), (int)(10 * cur_scale), w, h);
    }
    if (open_now.load(std::memory_order_relaxed)) {
        std::unique_lock lk(bmp_mu);
        const uint32_t mw = menu_bmp.w, mh = menu_bmp.h;
        lk.unlock();
        // (one copy finished before the next: on a Radeon the menu came out with its top torn away beside the FPS counter)
        VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        mb.srcStageMask = mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        mb.srcAccessMask = mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        di.memoryBarrierCount = 1, di.pMemoryBarriers = &mb;
        vkCmdPipelineBarrier2(cmd, &di);
        if (mw) {
            int mx = (int)(w - mw) / 2, my = (int)(h - mh) / 2;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            copy_over(cmd, dst, menu_bmp, ups[0][slot], mx, my, w, h);
        }
    }
}

} // namespace mkwvk
