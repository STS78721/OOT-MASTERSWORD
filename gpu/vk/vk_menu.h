// The Insert menu (vk_menu.cpp): options that apply while the game runs, kept in hookshot_options.ini beside the executable.
#pragma once

#include "vk_internal.h"

namespace mkwvk {

// (out of the box every feature is off: what the game does on its own. The ini beside the executable keeps a player's choices)
struct Options {
    // Frame generation engine: 0 Off, 1 MasterSword (Hookshot native vk_framegen), 2 Nodix SMFG 1.3, 3 DLSS MFG,
    // 4 FSR 3.1 FG, 5 XeSS MFG, 6 Apex FG (Fast). MasterSword is the only path that is the Hookshot interpolator by name.
    // DLSS MFG / SMFG / FSR / XeSS / Apex currently share that interpolator as a stand-in until Streamline/vendor FG
    // evaluates (sl.log loads dlssg but Vulkan CmdBind* hooks are incomplete here). SMAI is under Upscaling.
    int fg_backend = 0;
    int framegen = 0;          // made frames: 0 off, else the steps opts_fg_mult_max() allows for this engine
    // NVIDIA DLSS Frame Generation models (menu labels): A original, B UI, C Performance
    int framegen_preset = 1;   // 0 A (original), 1 B (UI), 2 C (Performance)
    int fg_flow = 35;          // Nodix Flow scale 10–100 (motion strength for SMFG)
    int pace = 0;              // frames shown on an even line (vk_framegen.cpp)
    int ms_pacer = 0;          // Mastersword Pacer: off until the Insert row is turned on (host/nn_vi.c)
    int hybrid_cpu = 1;        // (0.1.9c) Intel Hybrid CPU Patch: the game's threads on P-cores, background work on E-cores (host/hos_thread.c, read at start). Nothing on a CPU of one kind
    int vsync = 0;             // V-Sync: 0 Mailbox, 1 On (FIFO), 2 Off (immediate). The row cycles On, Mailbox, Off. RX_VK_FIFO / RX_VK_IMMEDIATE over it
    int fps_cap = 0;           // Game frame rate cap: 0 60 .. 5 240, 6 Uncapped (host/nn_vi.c frame_rate_cap=)
    int resolution = 0;        // the 3D's: 0 Auto (the game's dynamic resolution), else 100%, 75%, 67%, 50% of its full scale (nx2/oot_patches.py)
    int output_res = 0;        // whole frame, docked: 0 1920x1080, 1 2560x1440, 2 3840x2160; read once at start (host/nn_applet.c)
    int match_monitor = 0;     // 1: the screen's size, each side rounded down to a multiple of 8. Off uses output_res.
    int upscale = 0;           // 0 Off, 1 Catmull-Rom, 2 Lanczos, 3 XeSS, 4 FSR 3.1, 5 FSR 1.0, 6 SMRS (Nodix), 7 SMAI, 8 DLSS SR, 9 FSR 4
    int xess_quality = 3;      // shown only when Upscaling=XeSS: 0 AA .. 6 Ultra Performance
    int fsr_quality = 2;       // FSR 3.1/1.0: 0 Native AA 100%, 1 92%, 2 83%, 3 75%, 4 Quality 67%, 5 Balanced 58%, 6 Perf 50%, 7 Ultra 33% (#2147)
    int nodix_quality = 1;     // shown only when Upscaling=SMRS/SMAI: 0 Quality, 1 Balanced, 2 Performance
    int dlss_quality = 1;      // DLSS SR or FSR 4: 0 Quality, 1 Balanced, 2 Performance, 3 Ultra Perf, 4 Native AA (DLAA / FSR4_AA)
    // Compat with origin FSR3 / NGX helpers still in-tree (menu Upscaling row uses `upscale`, not these).
    int dlss = 0;
    int dlss_model = 0;
    int sharpness = 0;         // 0 to 100
    int smaa = 0;              // present-path SMAA 1x (iryoku Medium): 0 Off, 1 On
    // Colour Profiles (STS78721 / TOTK / DKB vivid OLED): preset writes the four values; moving one → Custom
    int color_preset = 0;       // 0 Default, 1 Vivid (OLED), 2 Clarity, 3 Custom
    int color_brightness = 0;   // -30..30 (% added)
    int color_contrast = 100;   // 70..150 (%; 100 = unchanged)
    int color_saturation = 100; // 50..200 (%; 100 = unchanged)
    int color_gamma = 100;      // 80..120 as hundredths of the exponent (100 = 1.00)
    int realtime = 1;     // game time follows the clock (host/nn_vi.c); 0: per frame (slower frames, slower game)
    int handheld = 0;     // the game's handheld profile: ~4 fps, softer; read once, at the start (host/nn_applet.c)
    int ultrawide = 0;    // the camera at the monitor's shape when it is wider than 16:9; read once, at the start (host/hos_main.c)
    int show_fps = 0;
    int fullscreen = 0;   // borderless fullscreen + cursor hide: F11 is the same setting
    int gpu_device = 0;   // graphics card index: 0 Auto, 1..N device 0..N-1 (gpu/vk/vk_main.cpp; RX_VK_DEVICE)
    int pad_background = 0;   // the controllers count while another window has the focus (host/nn_hid.c; HOS_INPUT_BACKGROUND)
    int mouse_native = 0; // the mouse turns the camera's own angles (host/mouse_native.c); 0: as the right stick
    int mouse_sens = 100; // the mouse's sensitivity in percent: 0.1 degrees a count at 100 with native look (host/nn_hid.c)
    int mouse_invert = 0; // the mouse's up and down swapped
    int stick_l_sens = 100; // left analog stick speed percent (host/pad_sdl.c, host/nn_hid.c; 100 = stock)
    int stick_r_sens = 100; // right analog stick speed percent (camera look)
    int text_speed = 0;   // how fast dialogue prints: 0 the game's, 1 2x, 2 4x, 3 instant (host/nn_vi.c, nx2/oot_patches.py; HOS_TEXT_SPEED)
    int fov = 100;        // vertical gameplay field of view, 50..150% of the game's current angle (host/oot_fov.c)
    int invert_cam_x = 0; // right-stick camera horizontal inversion: 0 normal, 1 inverted
    int invert_cam_y = 0; // right-stick camera vertical inversion: 0 normal, 1 inverted
    int pad_layout = 0;   // 0 Nintendo (default), 1 Xbox / PC, 2 Swap Face Buttons, 3 Custom
    int robust = 0;       // bounds-checked buffer access: 0 Auto (on), 1 Off, 2 On (gpu/vk/vk_main.cpp; RX_VK_ROBUST)
    int mesh_mode = 0;    // task/mesh programs: 0 Automatic, 1 Native, 2 Compute (mesh_shaders=; RX_VK_FORCE_MESH_FALLBACK over it)
    int preset = 0;       // quality preset: 0 Custom, 1 Minimum (4GB), 2 Low (6GB), 3 Balanced (8GB), 4 High (12GB), 5 Ultra (16GB+)
    int vram_mb = 0;      // dynamic texture cache budget limit in MB
    int mirror_mb = 0;    // staging mirror limit in MB
    // NVIDIA Reflex (VK_NV_low_latency2): 0 Off, 1 On, 2 Boost. Hardware-gated; ignored on non-NVIDIA.
    int reflex = 0;
    // Insert menu look: 0 OOT Mastersword (default), 1 Light, 2 Dark.
    int menu_theme = 0;
};
extern Options opts;

/// True when a frame-generation engine is selected and the multiplier is at least 2x.
inline bool opts_fg_on() { return opts.fg_backend > 0 && opts.framegen >= 2; }
/// MasterSword engine only — Hookshot native interpolator (vk_framegen).
inline bool opts_fg_hookshot() { return opts.fg_backend == 1 && opts.framegen >= 2; }
/// Nodix / vendor FG row (DLSS MFG, SMFG, FSR, XeSS, Apex Fast): stand-in until real DLLs evaluate.
inline bool opts_fg_vendor() { return opts.fg_backend >= 2 && opts.framegen >= 2; }
/// Highest Frame generation step this engine ships. 7x is never a step. 5x is Nodix and Apex only.
/// MasterSword 8 (2/3/4/6/8). DLSS MFG 6 (2/3/4/6, no 5x or 8x). FSR 3.1 is 2x only.
/// Nodix SMFG and Apex FG are 2/3/4/5/6. XeSS MFG is 2/3/4.
/// Apex's published ANR FG table is 2X–6X. The step on the Frame generation row is the one that is synthesised.
inline int opts_fg_mult_max() {
    switch (opts.fg_backend) {
    case 1: return 8;
    case 2: return 6;   // NODIX-TECH MFG Super Motion: Mode 0..4 → multiplier Mode+2, clamped ×2..×6
    case 3: return 6;
    case 4: return 2;
    case 5: return 4;
    case 6: return 6;   // Apex ANR FG quality table: 2X, 3X, 4X, 5X, 6X. No 8x. No SR build yet.
    default: return 0;
    }
}

/// Window-scale filter mode for vk_framegen.cpp. DLSS is temporal/NGX in-draw (no Lanczos stand-in).
/// Vendor/Nodix rows that already wrote the picture in-draw still use Lanczos only as a window filter when Sharpness needs scale.
inline int opts_spatial_upscale() {
    switch (opts.upscale) {
    case 1: return 1;                                      // Catmull-Rom
    case 2: case 3: case 4: case 5: case 6: case 7: return 2; // Lanczos (+ stand-ins as window filter)
    case 8: return 0;                                      // DLSS: temporal/NGX owns upscale; Sharpness alone may still scale
    case 9: return 0;                                      // FSR 4: temporal/AMD owns upscale
    default: return 0;
    }
}

/// True when a quality preset owns internal 3D below Output (same methodology as DLSS Perf/Quality):
/// render at % of Output, upscale to Output. False for Off / Native AA / DLAA / 100% Resolution.
bool upscaler_wants_upscale();
/// Map MasterSword Upscaling row onto upstream opts.dlss (DLSS / Off). Called from temporal_mode / resolution helpers.
void sync_temporal_from_upscale();

/// Pre-TAAU internal 3D colour for the next present (shared by DLSS / FSR / XeSS / Nodix / spatial).
void upscale_stash_input(VkImage img, VkImageView view, VmaAllocation alloc, VkFormat fmt, uint32_t w, uint32_t h);
bool upscale_take_input(VkImage* img, VkImageView* view, VmaAllocation* alloc, VkFormat* fmt, uint32_t* w, uint32_t* h);

/// (vk_memory.cpp) true when the quality preset's memory budgets (vram_mb, mirror_mb) are to be used: the card has no more
/// video memory than the preset is for. A 4GB preset on an 8 GB card kept 256 MB of mirrors and 2.2 GB of textures: the
/// game's memory was read over PCIe and the textures made again every frame, 5 fps with the CPU idle (Discord, 11-08)
bool preset_caps_memory();

void menu_init();             // (the options from the file, once)
/// (#2187) No options file yet: pick Quality preset from the card's VRAM once the device is chosen.
void menu_first_start_for_gpu(const VkPhysicalDeviceProperties& props, const VkPhysicalDeviceMemoryProperties& mem);
/// V-Sync as this run applies it: RX_VK_FIFO=1 On, RX_VK_IMMEDIATE=1 Off over the row. 0 Mailbox (default), 1 On, 2 Off
int menu_vsync();
void menu_set_gpu_count(int count);
void menu_tick();             // (each frame shown: the keys, while the game's window has the focus)
bool menu_is_open();
bool resolution_auto();       // Resolution Auto → game dynamic resolution (false while FSR/XeSS/Nodix quality owns scale)
int resolution_scale();       // fixed path scale in 1/10000 (twelfths of Output; upscaler quality when selected)
int upscaler_render_percent(); // 0 = Resolution row; else FSR/XeSS/Nodix quality % of Output
/// Final docked output size in pixels (game 2048x1152, or the monitor the window is on when Output=Native).
void menu_output_size(uint32_t& w, uint32_t& h);
void menu_option_set(int id, int value);   // (from the game's side: 7 the mouse's sensitivity, the - and = keys)
void menu_note_frame(bool game_frame);   // (the frame rates the FPS counter shows)
/// Over the window's image (TRANSFER_DST_OPTIMAL), w x h: the menu and the FPS counter. slot: the frame in flight, 0 to 2.
void menu_record(VkCommandBuffer cmd, VkImage dst, int slot, uint32_t w, uint32_t h);
/// Start-up prewarm screen while shaders/pipelines prepare (vk_startup_paint.h). Null clears it.
struct StartupView;
void startup_screen(const StartupView* view);

} // namespace mkwvk
