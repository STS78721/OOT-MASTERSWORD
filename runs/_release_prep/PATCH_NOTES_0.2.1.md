[Updated on November 19, 2026]

MasterSword 0.2.1

A major CPU, Vulkan renderer, and GPU-hardware compatibility release pairing **Profile-Guided Optimization (PGO)** on `mastersword_vk.dll` with **Threaded Rendering**, **Selective Submit-Time Capture**, **Native Zstd & Name-Search Fast Paths**, an **Off-Thread Vulkan Resource Reaper**, and the **NVIDIA RTX 50 Series Water Hang & White-Link Fix**. Every optimization preserves full native visual fidelity.

■ Major Updates

- **Vulkan Renderer Profile-Guided Optimization (PGO):** Compiled `mastersword_vk.dll` with a MasterSword-native PGO profile captured across live Castle Town Market and Hyrule Field gameplay (`3,215` functions / `24,477` instrumented blocks / `76.08B` execution counts), while keeping `HOOKSHOT_SHADER_STAMP` (`0x4eae666d8f924912`) untouched so existing program caches remain valid. Also adds `RX_VK_PGO=gen` / `RX_VK_PGO=<profdata>` support directly to the build pipeline (`host/pgo_rt.c`, `nx2/build.py`).
- **Selective Submit-Time Capture (`RX_VK_CAPTURE=2` Default) & Shadow-Baseline Sync:** Narrows submit-time memory captures by default to 2D passes, compute dispatches' small buffers, and tracked readback pages (`RX_VK_CAPTURE_COMPUTE=1`), paired with write-back diff shadow-baseline sync (`RX_VK_SHADOW_BASELINE=1`) in `mirror_sync()`, a `shadow_written_collected()` race guard, and a sky light accumulator reset before compute dispatches so ambient lighting never doubles (`RX_VK_CAPTURE=1` restores full capture).
- **GPU-Adaptive Capture Wait Cap (`RX_VK_CAPTURE_WAIT_MAX_MS=20`):** Replaces unbounded late-page submit waits with a GPU-adaptive `p90` cap (`20 ms` baseline, scaling dynamically up to `250 ms` on slower GPUs via `RX_VK_CAPTURE_WAIT_ADAPT=1`), eliminating exposure flashes and stalls during heavy submission bursts.
- **Threaded Rendering (`Auto` on 12+ Logical CPUs):** Decouples Vulkan command recording and present pacing (`threaded_rendering.h`, `hos_nvn_event_value` present polling via `RX_VK_PRESENTS_AHEAD`) when enabled (`Auto` on systems with at least 12 logical CPUs).
- **Native Zstd Sequence Loop & Sorted-Container Name Searches:** Executes Ocarina of Time's main `ZSTD_decompressSequences_body` hot loop (`f_801102e0`, `host/zstd_native.c`) and sorted-container string searches (`f_805aeea0`, `host/names_native.c`) in native host code with exact register/flag parity, cooperative virtual-core polling, and a stuck-load watchdog during area streaming.
- **Off-Thread Vulkan Resource Reaper & Shared Pipeline Cache Locks:** Moves expensive driver destruction (`VkImageView`, `VkImage`, buffers, and sparse memory in strict FIFO dependency order) off the critical replay thread onto a dedicated reaper thread (`reap()`), pairs with a shared reader-lock pipeline cache lookup (`RX_VK_PIPE_SHARED=1`) and per-program pipeline ownership (`RX_VK_PIPE_BY_PROGRAM=1`), adds direct uniform buffer snapshotting into recording streams (`direct_ub_snapshot.h`), and adds a lockless `queue_flush()` fast path when no batched submits are held.
- **NVIDIA RTX 50 Series Water Hang & White-Link Volume-Box Fix:** Safely skips the two broken half-size HDR 3D-volume box draws (`2b37a33d189014a8`, `0948a49a3c3068a8`) that caused GPU hangs in water scenes and white-Link corruption on RTX 50 Series cards (`RX_VK_FOG_BOXES=1` to re-enable for testing).

■ New & Improvements

○ Performance & Memory

- **Retired-Mirror Clean-Page Inheritance:** New mirrors inherit only clean pages not written by the CPU since the retired mirror's last sync (`mirror_inherit.h`, opt-out via `RX_VK_INHERIT_ALL=1`).
- **Finalized GPU-Drawn Texture Retention (`RX_VK_KEEP_DRAWN=1`):** Preserves finalized GPU-drawn textures up to 4 MB while storage remains untouched and tracks eviction loss cleanly across re-creations.
- **Single-TLS Hot Draw Lookups & Redrawn Target Tracking:** Cuts redundant TLS calls (`dw()`/`pass()`) across hot draw/replay functions, tracks `targets_redrawn`, adds `guarded_compute` retry safety, and batches `mesh_conv_n` work cleanly.
- **512 MB Managed Pagefile Reserve & AMD 85% Import Give-Back:** Reserves 512 MB headroom for Windows pagefile growth alongside finalized texture commit budgets, enables automatic idle import reclamation at 85% live VRAM on both NVIDIA and AMD (`0x1002`), and adds `RX_VK_IMPORT_DIAG` refusal telemetry.
- **Above-Normal Process Priority (`HOS_PROCESS_PRIORITY=1`):** Sets Windows process priority to Above Normal at startup (restored automatically on exit) to eliminate 40–90 ms foreground scheduling hitches when background apps wake up (`0` = Normal, `2` = High).

○ Updater, Launcher & Portability

- Bumped MasterSword, `mastersword.exe`, `mastersword_vk.dll`, and `MasterSwordLauncher.exe` to **0.2.1**, wired cleanly to **GitHub Releases (`STS78721/OOT-MASTERSWORD`)** for in-game update notifications and 1-click launcher updates.
- Included `rx_atomic_load`, dynamic `SetThreadInformation`, Linux `__builtin_cpu_supports("avx2")` detection, POSIX header shims, and llvm-mingw DLSS linker rules so Windows MSVC/clang-cl, llvm-mingw, and Linux builds stay aligned.

■ Other

- Configurable switches for `hookshot_env.txt`:
  - `RX_VK_CAPTURE=1` restores full submit-time memory captures; `RX_VK_SHADOW_BASELINE=0` disables shadow-baseline sync.
  - `RX_VK_CAPTURE_WAIT_MAX_MS=<ms>` / `RX_VK_CAPTURE_WAIT_ADAPT=0` tune or disable the adaptive late-page capture wait cap.
  - `RX_VK_FOG_BOXES=1` re-enables the RTX 50 volume-box draws.
  - `HOS_PROCESS_PRIORITY=0|1|2` adjusts Windows process priority (`1` Above Normal by default).
  - `HOS_ZSTD_NATIVE=0` / `HOS_NAMES_NATIVE=0` fall back to lifted paths (`2` compares live for diagnostics).
  - `RX_VK_PIPE_SHARED=0` / `RX_VK_INHERIT_ALL=1` / `RX_VK_KEEP_DRAWN=0` / `RX_VK_FLUSH_LOCK=1` toggle individual renderer paths.
- A fresh install remains **1920×1080**, **Windowed**, **Upscaling Off**, **Frame Generation Off**, **Reflex Off**, **Field of view 100%**, and **V-Sync Mailbox**.
- Updating does not overwrite `mastersword_options.ini`, `hookshot_env.txt`, saves (`user\`), or your game dump.

■ Update

- Close the game, then run `MasterSwordLauncher.exe` to update automatically to **0.2.1**, or extract `SteelTemple-MasterSword-0.2.1-Update.zip` over your existing folder.
- For a fresh portable install, extract `SteelTemple-MasterSword-0.2.1-Portable-StandAlone.zip` and place your own `image.bin` and `romfs\` folder beside `mastersword.exe`.
