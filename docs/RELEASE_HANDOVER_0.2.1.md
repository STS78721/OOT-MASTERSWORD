# MasterSword 0.2.1 — release handover

*Branch `mastersword-fg`, prepared 2026-11-19.*

## What 0.2.1 includes
- **Vulkan Renderer Profile-Guided Optimization (PGO):** `host/pgo_rt.c` + `nx2/build.py` `RX_VK_PGO` pipeline support (excluding `eden/`, `sirit/`, and `vk_shader.cpp` so the 7.4 GB program-cache stamp remains unchanged); `mastersword_vk.dll` built with MasterSword's Castle Town + Hyrule Field gameplay profile (`work/pgo/mastersword_vk.profdata`: `3,215` functions, `24,477` blocks, `76.08B` execution counts).
- **Selective Submit-Time Capture & Sync (`RX_VK_CAPTURE=2` default):** Restricts default submit-time capture to 2D passes, small compute buffers (`RX_VK_CAPTURE_COMPUTE=1`), and tracked readback pages; pairs with `RX_VK_SHADOW_BASELINE=1` in `mirror_sync()`, `shadow_written_collected()` race guard, pre-dispatch sky light accumulator resets (`kSkySumsProgram` / `kSkyRgblProgram`, `RX_VK_NO_SKY_ZERO=1` opt-out), retired-mirror clean-page inheritance (`gpu/vk/mirror_inherit.h`, `RX_VK_INHERIT_ALL=1` opt-out), and GPU-adaptive `p90` late-page wait cap (`RX_VK_CAPTURE_WAIT_MAX_MS=20`, `RX_VK_CAPTURE_WAIT_ADAPT=1`).
- **Threaded Rendering (`Auto` on 12+ logical CPUs):** `gpu/vk/threaded_rendering.h` + `gpu/vk/vk_replay.cpp` + `host/nvn_native.c` (`rx_option 0x4D5301`).
- **Native Zstd & Sorted-Container Name Searches:** `host/zstd_native.c`, `host/names_native.c`, and `nx2/oot_patches.py` (cooperative `A64_POLL` virtual-core polling + stuck-load watchdog).
- **Renderer Concurrency, Memory & Stability:**
  - Off-thread FIFO resource reaper (`gpu/vk/vk_vma.cpp` `reap()`, `gpu/vk/vk_texture.cpp` view/image/pool/sparse cleanup).
  - Finalized GPU-drawn texture retention (`RX_VK_KEEP_DRAWN=1`) & eviction loss tracking (`lost_made`).
  - Direct uniform buffer snapshotting (`gpu/vk/direct_ub_snapshot.h` + unit test `tests/direct_ub_snapshot_test.cpp`).
  - Shared reader-lock pipeline cache lookup (`RX_VK_PIPE_SHARED=1`), per-program pipeline ownership (`RX_VK_PIPE_BY_PROGRAM=1`), single TLS `dw()`/`pass()` lookups, `targets_redrawn`, `guarded_compute` retry, `mesh_conv_n`, lockless `queue_flush()` fast path (`pend_held`, `RX_VK_FLUSH_LOCK=1` opt-out).
  - RTX 50 Series water hang & white-Link volume-box skip (`2b37a33d189014a8`, `0948a49a3c3068a8`; `RX_VK_FOG_BOXES=1` opt-out).
  - 512 MB Windows managed-pagefile reserve, AMD (`0x1002`) 85% idle import reclamation + `RX_VK_IMPORT_DIAG`, Windows `ABOVE_NORMAL_PRIORITY_CLASS` default (`HOS_PROCESS_PRIORITY=1`).
- **Release Infrastructure & Portability Alignment (`3cbcca3f..6277b730`):** GitHub `STS78721/OOT-MASTERSWORD` updater & launcher alignment (`host/hos_update.c`, `launcher/MasterSwordLauncher.py`) plus llvm-mingw / CI portability (`rx_atomic_load`, dynamic `SetThreadInformation`, `__builtin_cpu_supports`, POSIX shims, `vk_ngx_msvc_rt`).

## Preserved invariants & verification
- **Program cache & ABI (`10/11`) preserved:** zero edits to `gpu/vk/eden/**`, `gpu/vk/sirit/**`, `gpu/vk/compat/**`, `vk_shader.*`, `vk_state.h`, `vk_api.h`, or `vk_menu.*`; computed `HOOKSHOT_SHADER_STAMP == 0x4eae666d8f924912ull` unchanged.
- **0 lifted chunks recompiled:** all 91 lifted `code_*.c` / `code_hot_*.c` chunks skipped (`0s`) in `work\build_full.bat` (`RX_RELEASE=1`, PE subsystem `2`, `0` failed).
- **Tests:** `python -I -B tests/test_tls_objects.py` PASS (`18 TLS_OBJECTs; 51 other namespace-scope thread_locals`); `tests/direct_ub_snapshot_test.cpp` compiled & PASS (`exit 0`); `mastersword.exe --probe-dpi` PASS (`0.2.1`).
- **Golden Rule checks:** `gpu/vk/vk_menu.h` and `launcher/MasterSwordLauncher.py` keep `1920x1080`, windowed, Upscaling Off, Frame Generation Off, Reflex Off, FOV 100%, V-Sync Mailbox; strict allowlist staging (`15` portable files / `13` update files); zero personal settings, logs, caches, saves, or NGX binaries bundled.

## Release targets
- **GitHub (`STS78721/OOT-MASTERSWORD`):** `https://github.com/STS78721/OOT-MASTERSWORD/releases/tag/0.2.1`
