# MasterSword 0.2.2 — release handover

*Branch `mastersword-fg`, prepared 2026-11-19.*

## What 0.2.2 includes
- **Fast Sync (`RX_VK_FAST_SYNC=1`) & Memory Sync Overhaul:** Write-back sequence tracking (`msync`), page diff captures, tile-based partial texture tracking (`gpu/vk/texture_tiles.h`), render-target overlap tracking (`gpu/vk/target_overlap.h`), GPU dump stats (`gpu/vk/vk_dump.{h,cpp}`), and host OS sync updates (`host/nn_os_sync.c`).
- **Swapchain Acquire Fence & Threaded Pacing:** CPU-side `acquire_fence` + deferred `HeldImage` drop (`gpu/vk/vk_replay.cpp`), `host_read_barrier()` before ending command buffers, threaded rendering wakeup tuning (`gpu/vk/threaded_rendering.h`, `host/hos_thread.c`, `host/nn_vi.c`), and fresh `0.2.2` PGO profile (`work/pgo/mastersword_vk.profdata`: `3,296` functions, `25,182` blocks, `82.66B` execution counts).
- **HUD-Less FSR 3.1 Frame Generation & Hidden-Mesh Stabilization:**
  - `gpu/vk/shaders/fg_hud.comp`, `fg_hud_comp_spv.h`, updated `fsr3_fi_interpolate_comp_spv.h` (`interpolation_t = 0.5f`), `gpu/vk/vk_fsr3fi.{h,cpp}`, `gpu/vk/vk_framegen.{h,cpp}` (`RX_VK_FSR_FG_HUD=0` opt-out, preserving MasterSword 2x–8x, Apex FG, Nodix SMFG, DLSS MFG), plus 100% `upscale_picture()` passthrough in `gpu/vk/vk_draw.cpp`.
  - `gpu/vk/hidden_mesh.h`, `gpu/vk/vk_temporal.{h,cpp}`, and `vk_draw.cpp` (`hidden_mu` + `hidden_active` fast path safe under Auto parallel replay).
- **VRAM, Texture & GPU Stability Fixes:**
  - 3/4 CPU-overwrite finalized texture eviction (`finalized_obsolete`), generalized upload retry recovery (`retry_frame` / `upload_never`), and RGB-to-RGBA alpha widening (`widen_alpha`) in `gpu/vk/vk_texture.{h,cpp}`.
  - 120-frame deferred mask free + pass-end transitions (`mask_retire`/`mask_reap`), 1-level sampler anisotropy skip, Intel `!blends` depth-bias guard, and vertex divisor clamping (`dev.max_vertex_divisor`) in `gpu/vk/{vk_internal.h,vk_main.cpp,vk_draw.cpp}`.
  - Per-pointer external host memory type check in `gpu/vk/vk_memory.cpp` (`import_pool()`).
  - Open save-write wait on window close in `host/nn_fs.c` and `host/hos_relaunch.c`.
- **Host & Lifted-Code Optimizations & >60 FPS Gameplay Fixes:**
  - OS-wake job barriers (`host/oot_barrier.{h,c}`, `HOS_OOT_BARRIER_WAIT` in `f_800998a0`, `f_800738e0`, `f_8007e200`), texture heap compaction throttle (`host/oot_tex_compact.{h,c}`, `HOS_TEX_COMPACT_MS=1000` in `f_80797240`), and pieced native host libc (`host/hos.h`, `host/nn_libc.c`, `nx2/entry_hooks.py`, `nx2/lift.py`, `nx2/make_game.py` in `f_8572f020`, `f_8572f040`, `f_8572f300`, `f_85747d40`, `f_8575d280`, `f_8575d380`).
  - `>60 fps` / `<60 fps` fixes in `host/{loop_step.h,frame_whole.h,nn_vi.c,oot_high_fps.h,oot_high_fps.c}` & `nx2/oot_patches.py`: Epona jump (`f_82c59eb0`), Shadow Temple / moving floor carry (`f_80159dc0`, `f_8015d490`, `f_838baa40`), frozen Zora pose/step (`f_802cfca0`, `f_80944aa0`), 3× water ripple substeps (`f_800672e0`), ocarina song cue (`f_832b2af0`, `f_832b2dc0`), and Havok cloth time (`f_81b3aa48`, optional `HOS_FPS_CLOTH_60=1` gate in `f_81b47080`).

## Preserved invariants & verification
- **Program cache & ABI preserved:** Zero edits to `gpu/vk/eden/**`, `gpu/vk/sirit/**`, `gpu/vk/compat/**`, `vk_shader.*`, `vk_state.h`, `vk_api.h`, or `vk_menu.cpp`; `HOOKSHOT_SHADER_STAMP == 0x4eae666d8f924912ull` unchanged.
- **Targeted lifted-C splice:** Only the 13 targeted `work/sa/gen/code_*.c` chunks (`code_000.c`, `code_001.c`, `code_002.c`, `code_006_1.c`, `code_008.c`, `code_022.c`, `code_036.c`, `code_041.c`, `code_046.c`, `code_hot_1.c`, `code_hot_5.c`, `code_hot_6.c`, `code_hot_12.c`) compiled cleanly (`0 failed`) in Checkpoint Build #3; `0` chunks recompiled on final `0.2.2` build (`2s`, `0 failed`).
- **Tests & Binary Checks:** `python -I -B tests/test_tls_objects.py` PASS (`18 TLS_OBJECTs; 53 other namespace-scope thread_locals`); `mastersword.exe --probe-dpi` PASS (`0.2.2 (source f0f257d8)`); `mastersword.exe` PE Subsystem == `2`; `mastersword_vk.dll` contains `0.2.2` and no PGO instrumentation symbols (`__llvm_prf` / `.lprfd`).
- **Golden Rule checks:** Safe `1920x1080` windowed, Upscaling Off, Frame Generation Off, Reflex Off, FOV 100%, V-Sync Mailbox defaults in `vk_menu.h` and `MasterSwordLauncher.py`; strict allowlist staging (`15` portable / `13` update); zero personal settings, caches, logs, saves, or NGX binaries bundled.

## Release target
- **GitHub (`STS78721/OOT-MASTERSWORD`):** `https://github.com/STS78721/OOT-MASTERSWORD/releases/tag/0.2.2` (live smoke playtest completed; published on GitHub).
