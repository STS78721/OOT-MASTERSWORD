[Updated on November 19, 2026]

MasterSword 0.2.2

A comprehensive Vulkan sync, frame-pacing, memory-management, HUD-less FSR 3.1 Frame Generation, and high-framerate gameplay release pairing **Fast Sync (`RX_VK_FAST_SYNC=1`)** and **CPU/GPU Memory Sync Overhaul** with **CPU-Side Swapchain Acquire Pacing**, **OS-Wake Job Barriers**, **Pieced Native Host Libc Fast Paths**, **HUD-Less FSR 3.1 Frame Generation**, **Hidden-Mesh Camera Stabilization**, and **>60 FPS Gameplay Timing Fixes**.

■ Major Updates

- **Fast Sync (`RX_VK_FAST_SYNC=1`) & Memory Sync Overhaul:**
  - Tracks fine-grained CPU/GPU write-back sequences (`msync`, write-back sequence numbers, and per-page diffs across `gpu/vk/vk_memory.cpp`, `vk_draw.cpp`, `vk_replay.cpp`, `vk_vma.cpp`, and `host/nn_os_sync.c`) to avoid redundant host/GPU synchronization stalls while keeping rendering bit-accurate.
  - Adds tile-based partial texture tracking (`texture_tiles.h`), render-target overlap tracking (`target_overlap.h`), GPU-written capture throttling, and tighter Vulkan barrier scheduling (`RX_VK_FAST_SYNC=0` restores legacy sync behavior).
  - Refreshes the Vulkan renderer **Profile-Guided Optimization (PGO)** build against a `0.2.2` live gameplay capture (`3,296` functions, `25,182` basic blocks, `82.66B` execution counts) so all new Fast Sync, swapchain acquire, HUD mask, and hidden-mesh paths are layout-optimized while keeping `HOOKSHOT_SHADER_STAMP` (`0x4eae666d8f924912`) untouched.
- **FSR 3.1 HUD-Less Frame Generation & Temporal Scale Fixes:**
  - Introduces a dedicated HUD compute mask (`fg_hud.comp`) that separates static overlay UI/HUD pixels from world motion vectors during FSR 3.1 Frame Generation (`vk_framegen.cpp`, `vk_fsr3fi.cpp`, `vk_draw.cpp`), keeping menus, hearts, minimaps, and text crisp during camera motion (`RX_VK_FSR_FG_HUD=0` opt-out; properly wired alongside MasterSword Frame Generation, Apex FG, Nodix SMFG, and DLSS MFG).
  - Enables native 100% scale passthrough cleanly when `upscale_picture()` renders at full resolution and fixes interpolation weights across generated frames.
- **Hidden-Mesh Temporal Depth Stabilization:**
  - Stabilizes camera-inside-Link hidden-mesh depth rendering (`hidden_mesh.h`, `vk_temporal.cpp`, `vk_draw.cpp`) under DLSS, DLAA, and FSR 3.1 temporal upscalers across both serial and parallel command replay, eliminating disappearing geometry/flicker when the camera clips near Link.

■ New & Improvements

○ CPU, Frame Pacing & Memory Optimization

- **OS-Wake Job Barriers (`HOS_OOT_BARRIER_WAIT=1`):** Replaces 20 µs busy-sleep polling loops in the game's three parallel-for job systems (`f_800998a0`, `f_800738e0`, `f_8007e200`) with direct address waits (`WaitOnAddress` / `WakeByAddressAll` in `host/oot_barrier.c`) woken immediately when a worker completes a job mask bit (`HOS_OOT_BARRIER_WAIT=0` restores sleeps).
- **Pieced Native Host Libc Entry Hooks (`host/nn_libc.c`, `nx2/entry_hooks.py`):** Routes the lifted SDK's `memcpy`, `memmove`, `memset` (`>= 8` bytes), `memcmp` (`>= 16` bytes), `strlen`, and `strcmp` through pieced 256 KB host routines with cooperative `A64_POLL` checkpoints, accurate guest bounds checking (`hos_guest_range`), `HOS_WRITTEN` page tracking, and proper `w0` return-register behavior (`HOS_GUEST_LIBC=1` restores lifted SDK loops).
- **CPU-Side Swapchain Acquire Fence & Pacing (`vk_replay.cpp`):** Replaces GPU-blocked swapchain acquire semaphores with a CPU-side acquire fence and deferred `HeldImage` lifecycle, paired with `host_read_barrier()` before ending recorded command buffers and threaded-rendering wakeup refinements (`threaded_rendering.h`, `hos_thread.c`, `nn_vi.c`).
- **Texture Heap Compaction Throttle (`HOS_TEX_COMPACT_MS=1000`):** Throttles per-frame texture-data heap compaction requests (`f_80797240`, `host/oot_tex_compact.c`) to at most once per 1,000 ms instead of every frame (`0` restores per-frame requests, `off` disables).
- **VRAM, Texture & Driver Fixes:**
  - Evicts finalized textures overwritten 3/4 or more on the CPU (`finalized_obsolete` in `vk_texture.cpp`).
  - Defers freeing stabilized mask resources for 120 frames with clean pass-end transitions (`mask_retire` / `mask_reap` in `vk_draw.cpp`).
  - Generalizes texture upload retry/recovery (`retry_frame`, `upload_never`), widens RGB-to-RGBA source alpha cleanly (`widen_alpha`), skips redundant anisotropy on single-level samplers, skips Intel depth-bias workarounds on non-blended draws, checks host memory types per imported pointer in `import_pool()`, and clamps vertex attribute instance divisors to `dev.max_vertex_divisor`.
- **Save-Exit Write Protection (`host/nn_fs.c`, `host/hos_relaunch.c`):** Tracks active open/write/resize file operations and waits cleanly up to 3 seconds on window close so in-flight save writes never get cut off.

○ >60 FPS & <60 FPS Gameplay Accuracy

- **Epona Fence Jump Blend (`f_82c59eb0`):** Fixes Epona failing to blend into JumpReady and sliding through fences above 60 fps (`hos_oot_horse_whole_step`).
- **Shadow Temple Barge & Moving Floor Carry (`f_80159dc0`, `f_8015d490`, `f_838baa40`):** Scales slow-time checks against real frame steps above 100 fps so Link and enemies stay planted on moving floors (`hos_oot_floor_carry_slow`).
- **Frozen Zora Poses Above 60 FPS (`f_802cfca0`, `f_80944aa0`):** Keeps 1-frame animation loops locked to whole-frame steps (`hos_loop_step` / `hos_loop_pose`) so frozen Zoras under the ice in Zora's Domain stand completely still above 60 fps.
- **Smooth Water Ripples (`f_800672e0`):** Accumulates fractional water ripple substeps per water surface (`hos_frame_whole_times`) so ripples spread smoothly every frame above 60 fps.
- **Ocarina Song Playback Cues (`f_832b2af0`, `f_832b2dc0`):** Prevents rapid double/triple note advances during ocarina song playbacks above 60 fps (`hos_oot_song_cue`).
- **Havok Cloth Timing (`HOS_FPS_CLOTH`, optional `HOS_FPS_CLOTH_60=1`):** Scales cloth step durations smoothly below 60 fps (`hos_oot_cloth_seconds`) and adds an optional 60 Hz cloth solve gate above 60 fps (`HOS_FPS_CLOTH_60=1` in `hookshot_env.txt`, off by default).

■ Other

- Configurable environment switches for `hookshot_env.txt`:
  - `RX_VK_FAST_SYNC=0` disables Fast Sync memory tracking.
  - `RX_VK_FSR_FG_HUD=0` disables HUD-less compositing in FSR 3.1 Frame Generation.
  - `HOS_OOT_BARRIER_WAIT=0` restores 20 µs barrier sleeps; `HOS_TEX_COMPACT_MS=<ms|0|off>` tunes texture-heap compaction cadence; `HOS_GUEST_LIBC=1` restores lifted SDK memory/string functions.
  - `HOS_FPS_CLOTH=0` disables below-60 cloth scaling; `HOS_FPS_CLOTH_60=1` caps cloth solves at 60 Hz above 60 fps.
- A fresh install remains **1920×1080**, **Windowed**, **Upscaling Off**, **Frame Generation Off**, **Reflex Off**, **Field of view 100%**, and **V-Sync Mailbox**.
- Updating does not overwrite `mastersword_options.ini`, `hookshot_env.txt`, saves (`user\`), or your game dump.

■ Update

- Close the game, then run `MasterSwordLauncher.exe` to update automatically to **0.2.2**, or extract `SteelTemple-MasterSword-0.2.2-Update.zip` over your existing folder.
- For a fresh portable install, extract `SteelTemple-MasterSword-0.2.2-Portable-StandAlone.zip` and place your own `image.bin` and `romfs\` folder beside `mastersword.exe`.
