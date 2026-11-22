# MasterSword 0.2.3 — release handover

*Branch `mastersword-fg`, prepared 2026-11-22.*

## What 0.2.3 includes
- **LDS-Accelerated GPU ASTC Decoder & Multi-Layer BC3 Batching (`gpu/vk/host_shaders/astc_decoder_comp_spv.h`, `gpu/vk/vk_texture.cpp`, `gpu/vk/vk_vma.cpp`, `gpu/vk/vk_internal.h`):**
  - Replaces private shader scratch arrays with on-chip bank-conflict-free LDS (`s_buf[16][65]`), packed byte emission, and workgroup bounds (`blocks_x`, `blocks_y` in 40-byte `AstcPush`), speeding up GPU ASTC decoding on non-ASTC cards.
  - Adds `allocation_in_vram()` check and `1024x1024` multi-layer BC3 small scratch (`bc_small`, 8 layers/run by default on NVIDIA, RADV, and AMD Windows; `256x256` tiny scratch on sub-6 GB cards for single-layer BC3 mip-group batching `RX_VK_BC_LEVELS`), explicit block-aligned `vkCmdCopyBufferToImage` pitches, and per-layer copy region splitting (`RX_VK_BC_COPY_PER_LAYER`) on non-4-aligned multi-layer BC mip tails for AMD Windows drivers.
- **Batched Indirect Mesh Draw Conversion (`gpu/vk/mesh_conv.h`, `gpu/vk/vk_draw.{h,cpp}`, `gpu/vk/vk_replay.cpp`):**
  - Pre-fills the 4 MB indirect mesh conversion ring once at creation (`{count, 1, 1}` with 48-byte slot alignment) and scans forward after game barriers (`RX_VK_MESH_CONV=2`) using a compile-time `kOpTable` (256-entry O(1) lookup) to batch-convert indirect mesh draws without breaking render passes.
  - Isolates per-thread batch state behind `TLS_OBJECT(mesh_batch, mesh_conv::Batch)` and invalidates active batches on memory-writing draws, stop opcodes, or `dispatch()`.
- **Handle-Table Live Count Bounding (`gpu/vk/vk_draw.cpp`):**
  - Bounds `stage_handles_counted()` descriptor-array evaluation to the active record count declared in validated handle-table buffer headers (`word[0] <= word[2]`), leaving trailing stale slots unbound (`0`) to prevent multi-gigabyte texture re-upload storms after pause menus (`RX_VK_HANDLE_TABLE_COUNT=0` opt-out). Kept inside `vk_draw.cpp` so `vk_shader.*` and `HOOKSHOT_SHADER_STAMP` remain unchanged.
- **FSR 3.1 Direct Output & Narrowed Temporal Barriers (`gpu/vk/vk_fsr3.{h,cpp}`, `gpu/vk/vk_temporal.{h,cpp}`, `gpu/vk/vk_dlss.cpp`):**
  - Lets FSR 3.1 render directly to the destination image without an extra copy (`direct_out`) and uses stage-narrowed `VkMemoryBarrier2` barriers around temporal upscaler evaluation.
- **Standalone Frame Pacing, Upscaler Pre-Warming & Low-VRAM Memory Sync (`gpu/vk/vk_framegen.cpp`, `gpu/vk/vk_memory.cpp`, `gpu/vk/vk_dlss.{h,cpp}`, `gpu/vk/dlss_checks.h`, `gpu/vk/vk_menu.{h,cpp}`):**
  - Allows Frame Pacing without enabling Frame Generation (`multiplier() <= 1`), skipping optical-flow luma/HUD work.
  - Holds Fast Sync on `<=6 GB` VRAM GPUs (`RX_VK_FAST_SYNC_HOLD`) and prioritizes dynamically GPU-written pools during mirror eviction (`RX_VK_MIRROR_PRIO`).
  - Pre-warms DLSS/DLAA before first 3D frame arrival (`RX_VK_DLSS_PREWARM`) and logs slow menu opens (`>50 ms`).
- **Windows Vulkan Streamline Presentation Compatibility (`gpu/vk/vk_main.cpp`, `gpu/vk/vk_replay.cpp`):**
  - Explicitly retires the old swapchain before recreation on Windows NVIDIA (`retire_swapchain()`) and tracks acquire binary semaphores alongside CPU acquire fences (`acquire_semaphore`, `retired_acquires`, `f.acquired`, `f.retired_acquire`).
- **Version Bump (`VERSION`, `launcher/MasterSwordLauncher.py`):**
  - Updated version string to `0.2.3` across `mastersword_vk.dll` (`0.2.3 (source d2194f87)`), `MasterSwordLauncher.exe`, and release archives.

## Preserved invariants & verification
- **Program cache & ABI preserved:** Zero edits to `gpu/vk/vk_api.h`, `gpu/vk/vk_state.h`, `gpu/vk/vk_shader.*`, `gpu/vk/eden/**`, `gpu/vk/sirit/**`, `gpu/vk/compat/**`, `host/**`, or `nx2/**`; `HOOKSHOT_SHADER_STAMP == 0x4eae666d8f924912ull` unchanged (100% compatible with `0.2.2` `mastersword.exe` and existing shader/program/pipeline caches).
- **Tests & Binary Checks:**
  - `python -I -B tests/test_tls_objects.py` PASS (`19 TLS_OBJECTs; 54 other namespace-scope thread_locals, none with a constructor or destructor`).
  - `mastersword_vk.dll`: built standalone (`RX_VK_ONLY=1`, `RX_RELEASE=1`, NVIDIA DLSS + AMD FSR 4 enabled, `0 failed`), `10,613,248` bytes, SHA256 `8C962DC6596AA9404D400D867C3A8316D898E4AB95FBC9CC5144C77EDC4F47B4`, exports `mkwvk_close_asked` and `mkwvk_init`, zero `__llvm_prf` / `.lprfd` markers.
  - `MasterSwordLauncher.exe`: `23,428,107` bytes, SHA256 `445CD1826B7BE3AC04DA8A7CFB5F2AF83B87C3889C35BC74F1D17DE6A81ECC5C`.
- **Golden Rule checks:** Safe `1920x1080` Windowed, Upscaling Off, Frame Generation Off, Reflex Off, FOV 100%, V-Sync Mailbox defaults untouched in `gpu/vk/vk_menu.h` and `launcher/MasterSwordLauncher.py`; strict allowlist staging (`13` Update / `15` Portable-StandAlone); zero personal configs, saves, caches, logs, or NGX runtime binaries bundled.

## Release target
- **GitHub (`STS78721/OOT-MASTERSWORD`):** `SteelTemple-MasterSword-0.2.3-Update.zip` & `SteelTemple-MasterSword-0.2.3-Portable-StandAlone.zip` staged in `C:\Games\GitHUB Builds\MasterSword\` (awaiting user OK before commit/tag/push/publish).
