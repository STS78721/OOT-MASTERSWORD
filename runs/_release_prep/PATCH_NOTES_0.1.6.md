# MasterSword 0.1.6

**Release date:** 2026-11-11  
**Type:** Hardware DLSS Frame Generation, Upscaler Quality, CPU/FMA & Stability update (drop-in over 0.1.5e / 0.1.5d / 0.1.5c / 0.1.5b)

Major graphics, pacing, CPU, and stability release bringing **real hardware NVIDIA NGX DLSS Frame Generation (`nvngx_dlssg.dll`, 2x–4x)** with **NVIDIA Reflex active under Frame Generation**, **Transparency Reactive Masks** for DLSS & FSR 3.1, **AMD FSR 3.1 Frame Interpolation** motion-vector fixes, **AVX2/FMA single-precision fused math** across all lifted game code, and critical **`VK_ERROR_DEVICE_LOST` / GPU hang** fixes.

---

## Highlights

- **Real Hardware NVIDIA NGX DLSS Frame Generation (`nvngx_dlssg.dll`)** — selecting **FG engine = DLSS MFG** (`2x` / `3x` / `4x`) now drives hardware optical-flow / Tensor-core DLSS-G directly through NGX (including at **Native 1080p / 1440p / 4K with Upscaling = Off** as well as alongside DLSS SR / DLAA)
- **Present-Ahead Pacing at 3x & 4x + V-Sync Auto FIFO under DLSS-G** — submits multi-frame sets ahead of time with dedicated spare swapchain images and switches V-Sync Auto to FIFO when DLSS-G is active for smooth VRR delivery
- **NVIDIA Reflex Active with Frame Generation** — keeps `VK_NV_low_latency2` sleep, simulation, and render-submit markers running on real game frames while Frame Generation is enabled (~12–18 ms lower input latency at 3x/4x)
- **Transparency Reactive Mask (`vk_reactive.cpp`) & Negative Mip LOD Bias** — eliminates ghosting on alpha/particle effects (water splashes, dust, fire, magic) under DLSS & FSR 3.1, and sharpens textures at sub-native DLSS render scales
- **Hardware AVX2/FMA Fused Single-Precision Math (`suyu_rt_fixes.py`) & Per-Thread Shader Pool Reuse** — compiles `FMADD`, `FRECPS`, and `FRSQRTS` to single hardware `vfmadd` instructions across all 75 lifted game chunks (22–45% faster on FMA chains) and speeds up shader preparation by ~20–30%
- **Critical GPU Stability & >60 FPS World Fixes** — fixes the Hyrule Castle tree-shadow GPU hang, 3D BC slice render-target `VK_ERROR_DEVICE_LOST`, Kokiri Forest flashing mesh-shader triangles, VRAM compaction active-texture races, and frozen water ripples / rupee pick-up counters above 60 FPS

---

## Changed

### Upscalers, Frame Generation & Latency
- **Hardware NGX DLSS-G (`vk_dlss.cpp`, `vk_temporal.cpp`, `vk_main.cpp`, `vk_replay.cpp`):** evaluates `NVSDK_NGX_Feature_FrameGeneration` (`nvngx_dlssg.dll`) on the temporal depth, dilated + actor motion vectors (in render pixels), and scene camera projection/view-projection matrices, with automatic fallback if a frame is refused
- **Native Resolution + DLSS MFG (`vk_temporal.cpp`, `vk_draw.cpp`):** feeds full depth, motion vectors, and camera matrices to NGX even when **Upscaling = Off**, allowing pure Native 4K / 1440p / 1080p rendering paired with hardware **DLSS MFG 2x / 3x / 4x**
- **3x & 4x Present-Ahead + V-Sync Auto FIFO (`vk_main.cpp`):** extends present-ahead copy submission beyond 2x, allocates spare swapchain images for 3x/4x sets, and selects `VK_PRESENT_MODE_FIFO_KHR` under V-Sync Auto while DLSS-G is presenting sets so VRR displays pace frames evenly
- **NVIDIA Reflex + Frame Generation (`vk_reflex.cpp`, `vk_framegen.cpp`):** tags real game frames with monotonic Reflex present IDs while keeping generated frames untagged so low-latency pacing remains active alongside FG
- **Transparency Reactive Mask (`vk_reactive.cpp`, `reactive_policy.h`):** computes a pre-post-process reactive mask (`R8_UNORM`) and feeds it to both NVIDIA DLSS and AMD FSR 3.1
- **AMD FSR 3.1 & FSR 3.1 Frame Interpolation (`vk_fsr3.cpp`, `vk_fsr3fi.cpp`):** wires full temporal FSR 3.1 Super Resolution (`Upscaling = FSR 3.1`) and FSR 3.1 Frame Interpolation (`FG engine = FSR 3.1 FG`, `2x`) with fixed low-res motion-vector reconstruction shaders
- **DLSS Mip Bias & Size Resilience (`vk_draw.cpp`, `vk_dlss.cpp`):** applies negative texture LOD bias (`log2(render_width / output_width) - 0.5`, clamped) when DLSS/DLAA is active, tolerates dynamic render-size rounding, and suppresses retry churn on refused sizes

### CPU, Shader Compilation & Host Runtime
- **Fused Single-Precision FMA / FRECPS / FRSQRTS (`nx2/suyu_rt_fixes.py`, `a64_suyu.h`):** enables hardware `__builtin_fmaf` (`vfmadd`/`vfnmadd`) on `x86-64-v3` builds alongside `RECOMP_FPX_FMA64`
- **Per-Thread Shader Translation Pool Reuse (`vk_shader.cpp`):** recycles `ObjectPool<IR::Inst>`, `IR::Block`, and `MAXWELL::Flow::Block` across shader translations instead of heap-churning per shader
- **Stage-Library First Lookup & 64 MB `pcache` Threshold (`vk_draw.cpp`):** checks warm graphics pipeline stage libraries before monolithic cache creation during gameplay and raises the pipeline-cache disk rewrite threshold to 64 MB
- **Guest Thread Scheduling & Short Sleeps (`host/hos_thread.c`):** threads that escape a long virtual-core wait now re-acquire a virtual core on their next block (`vc_escaped`), `hos_thread_destroy` closes thread wake handles cleanly, and `<= 6`-thread CPUs use high-resolution waitable timers on 20 µs guest worker polls
- **High-FPS Water Ripples & Pick-Up Counter (`host/nn_vi.c`, `host/frame_whole.h`, `nx2/oot_patches.py`):** carries sub-frame remainders per game counter (`hos_frame_whole`) so water rings and Link's item/rupee pick-up indicator animate smoothly above 60 FPS

### Vulkan Stability & `VK_ERROR_DEVICE_LOST` Fixes
- **Hyrule Castle Tree-Shadow Hang Fix (`vk_draw.cpp`):** disables the broken tree-shadow mask blend override by default (`RX_VK_TREE_SHADOW_MASK=1` opt-in), resolving GPU hangs near Hyrule Castle
- **3D BC Slice Render Targets & Foliage LOD Dither (`vk_texture.cpp`, `vk_draw.cpp`, `vk_xfer.cpp`):** routes 3D block-compressed slice targets through safe uncompressed staging attachments (`BlockAttachment`) instead of invalid 3D views, enabling the `53509044401d78d2` foliage LOD noise pass safely
- **Kokiri Forest Mesh-Shader Index Map Fix (`spirv_emit_context.cpp`):** sizes the mesh-to-fragment shared index map in 32-primitive blocks (`4 + Corners * DivCeil(max_primitives, 32) * 32`), fixing flashing triangles in Kokiri Forest
- **Default-On VRAM Compaction with Active-Texture Guard (`vram_compact.h`, `vk_texture.cpp`, `vk_vma.cpp`):** enables VMA texture block compaction safely by skipping textures sampled in recent frames
- **Per-Queue `sync_settle()` & Discarded Resource Cleanup (`vk_replay.cpp`, `vk_draw.cpp`, `vk_shader.cpp`):** waits on the active queue fence before CPU readbacks and destroys discarded Vulkan buffers/views/shader modules on early error paths

### Launcher (`MasterSwordLauncher.exe`) & Safe Defaults
- **Dual NGX Runtime Installer (`MasterSwordLauncher.py`):** **SETTINGS → Download & Install DLSS SDK** now downloads and SHA-256-verifies **both** `ngx\nvngx_dlss.dll` (DLSS SR) and `ngx\nvngx_dlssg.dll` (DLSS Frame Generation) `v310.9.16` directly from NVIDIA's official GitHub
- **Safe 1080p Out-of-the-Box Baseline:** fresh installs default cleanly to **1920×1080 Windowed**, **Upscaling = Off**, **FG engine = Off**, and **Reflex = Off** so every GPU starts safely while preserving existing `mastersword_options.ini` configurations on update

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.6-Update.zip`** | You already have 0.1.5e / 0.1.5d / 0.1.5c / 0.1.5b / 0.1.5 / 0.1.4 — open **MasterSwordLauncher** for 1-click auto-update, or extract over your game folder |
| `SteelTemple-MasterSword-0.1.6-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game before updating.
2. Open **`MasterSwordLauncher.exe`** — it detects **`0.1.6`**, downloads `SteelTemple-MasterSword-0.1.6-Update.zip`, verifies `mastersword.exe`, and restarts into **`V 0.1.6`** automatically (or extract the Update zip directly over your game folder).
3. **NVIDIA RTX 40/50 users (DLSS MFG):** open **SETTINGS → Upscaling / Frame Gen** and click **Download & Install DLSS SDK** once so both `ngx\nvngx_dlss.dll` and `ngx\nvngx_dlssg.dll` are placed beside `mastersword.exe`.
4. Launch the game — shaders will re-prepare once for the updated mesh-shader & translator stamp, and both the startup warmer and **`Insert`** menu will show **`0.1.6`**.

Your dump (`image.bin` + `romfs\`), saves (`user\`), and personal `mastersword_options.ini` stay untouched on your PC.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone testing native-res performance, upscalers, and hardware frame generation
