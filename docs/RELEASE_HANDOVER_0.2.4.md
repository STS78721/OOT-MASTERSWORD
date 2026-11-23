# MasterSword 0.2.4 — release handover

*Branch `mastersword-fg`, prepared 2026-11-23.*

## What 0.2.4 includes

### Frame Pacing, Startup & Memory
- **Submit & Present Priority Over Background Driver Cleanup (`gpu/vk/vk_internal.h`, `gpu/vk/vk_vma.cpp`, `gpu/vk/vk_memory.cpp`, `gpu/vk/vk_replay.cpp`):**
  - Adds a lightweight active-turn guard (`DriverTurn` / `driver_turns`) around hot-path `vkQueueSubmit2` and `vkQueuePresentKHR` calls (`queue_flush_locked`, `submit_recording`, `submit_segment_recording`, `par_replay` segment submits, `present_image`, and `present_out`).
  - Both the background image/memory reaper thread (`mkw_vk reaper`) and capture-buffer free worker (`mkw_vk cap free`) yield briefly via `reap_wait_turn()` (capped at 3 ms; `RX_VK_REAP_TURNS=0` disables) so background driver frees do not stall live frame submits or presents behind the driver lock.
- **Render-Pass-Aligned Replay Chunk Splits (`gpu/vk/vk_replay.cpp`):**
  - Factors chunk splitting into `split_part()` and defers `maybe_split()` until the next natural pass boundary (`NOP_BARRIER` or `NOP_SET_RT`) rather than splitting mid-render-pass every 240 draws (`RX_VK_CHUNK`), avoiding redundant `vkCmdEndRendering` / `LOAD_OP_LOAD` render-target reloads while still splitting huge passes at `4 * RX_VK_CHUNK` (`RX_VK_SPLIT_AT_PASS=0` opts out).
- **Startup Pre-Build of Texture Decoder Pipelines (`gpu/vk/vk_texture.cpp`, `gpu/vk/vk_main.cpp`, `gpu/vk/vk_internal.h`):**
  - Pre-builds GPU ASTC decode and BC3/BC7 re-encode compute pipelines via `texture_pipelines_ahead()` on GPUs without native ASTC sampling, removing first-upload compilation hitches (`RX_VK_TEX_PIPES_LATE=1` opts out).
- **Release Redundant Compute Shader Modules After Precompile (`gpu/vk/vk_draw.cpp`):**
  - Destroys redundant `VkShaderModule` handles after compute pipeline creation (leaving TAAU stand-ins and failed builds intact; `RX_VK_KEEP_MODULES=1` opts out), reclaiming commit RAM on shader-module fallback drivers.
- **Bounded Automatic Retry for Transient DLSS VRAM OOM (`gpu/vk/dlss_checks.h`, `gpu/vk/vk_dlss.{h,cpp}`, `gpu/vk/vk_temporal.cpp`, `tests/test_dlss_checks.{cpp,py}`):**
  - Replaces permanent creation refusal on `NVSDK_NGX_Result_FAIL_OutOfGPUMemory` (`0xBAD0000D`) with `dlss_checks::retry` (up to 3 attempts spaced 600 frames apart, ~10 s at 60 fps), keyed by quality mode and reset immediately on feature creation, `release()`, or upscaler menu changes.

### New In-Game Menu Settings, Visual Features & OOT Amber Theme
- **New `OOT Mastersword (Amber)` Menu Theme (`gpu/vk/menu_theme.h`, `gpu/vk/vk_menu.cpp`):**
  - Added theme slot 4 (`kOotAmber`) styled after the native in-game Options palette: warm mahogany leather columns, terracotta-copper header banner, dusty-terracotta row label strips, vibrant amber-gold selection pills with crisp white text, smoky plum-espresso inactive toggle wells, and a luminous white focus border.
- **HDR10 Output & Grading (`gpu/vk/shaders/hdr_encode.comp`, `gpu/vk/vk_main.cpp`, `gpu/vk/vk_framegen.{h,cpp}`, `gpu/vk/vk_imgui.cpp`, `gpu/vk/vk_replay.cpp`):**
  - Live-toggleable **HDR** (`Off / Expanded / Native`) with `VK_EXT_swapchain_colorspace` ST.2084 10-bit swapchain negotiation (clean automatic fallback to SDR with status reason when Windows HDR is off), **HDR paper white** (`80–400 nits`), **HDR peak brightness** (`400–4000 nits`), **HDR contrast** (`50–200%` luminance contrast with smooth highlight shoulder), and PQ-encoded Dear ImGui menu/HUD rendering at paper white.
- **AMD Anti-Lag 2 (`gpu/vk/vk_antilag.{h,cpp}`, `gpu/vk/vk_main.cpp`, `gpu/vk/vk_framegen.cpp`, `gpu/vk/vk_replay.cpp`):**
  - Native `VK_AMD_anti_lag` Vulkan extension support (`AMD Anti-Lag 2: Off / On`) with per-frame input and present pacing; safe no-op on non-AMD GPUs.
- **Image Quality & Retro Display Controls (`gpu/vk/vk_draw.cpp`, `gpu/vk/grass_distance.h`, `gpu/vk/crt_policy.h`, `gpu/vk/shaders/dlss_out.comp`, `gpu/vk/shaders/crt_*.comp`, `gpu/vk/shaders/colour_vision.glsl`):**
  - **DLSS model** (`K Transformer / M Transformer 2 / L Transformer 2 / NVIDIA Default`) and **Upscaler sharpness** (`0–100%` contrast-adaptive under-HUD sharpening right after DLSS / FSR 3.1 / FSR 4).
  - **Anisotropic filtering** (`Game's own / 2x / 4x / 8x / 16x`, live sampler reload without restart).
  - **Grass distance** (`Default / 1.5x / 2x / 3x`, live GPU uniform-buffer reach scaling without touching guest RAM).
  - **CRT filter** (`Off / Lottes Aperture / Lottes TV / Hyllian / ZFast`) and **CRT lines** (`240p / 480p / 720p`), disabled automatically during Native HDR PQ output.
  - **Colour vision** (`Off / Protan / Deutan / Tritan`, Machado luminance-preserving Daltonization in both SDR and Rec.2020 HDR).
  - **Threaded rendering** (`Auto / On / Off`, restart-tracked).
- **Upgraded FPS & Performance Overlay (`gpu/vk/vk_menu.cpp`, `launcher/MasterSwordLauncher.py`):**
  - **FPS counter** expanded to 5 modes (`Off / FPS / Frame time / Graph / Detailed`) with **FPS corner** (`Top right / Top left / Bottom right / Bottom left`), 120-sample frame-time ring buffer, average ms + 1% low FPS, live color-coded sparkline graph, VRAM usage, active rendering badges, and GPU name — synced with `MasterSwordLauncher.exe`.
- **Audio, Controls, Input & Debug Tab (`gpu/vk/vk_menu.cpp`, `gpu/vk/vk_replay.cpp`, `gpu/vk/menu_restart.h`, `gpu/vk/vk_texture.cpp`):**
  - **Gameplay tab:** **Audio output** (`Auto / Stereo / 5.1 Surround`), **Microphone** (live host device selector for Ocarina mic), **Discord status** (`On / Off`), **Menu shortcut** (`Insert / F1`, `F1 only`, `Insert / F1 / Esc`), and **Controller menu button** (`Home / Select+Start` vs `Select+Start only`).
  - **Controls tab:** **Disable camera centering**, **Rumble strength** (`0–150%`, fixes silent rumble forwarding), **Stick dead zone** (`0–30%`), **Motion controls** + **Motion sensitivity X/Y**, **Invert motion X/Y**, **Motion deadzone** (fixes gyro forwarding), and **Button prompts** (`Auto / Switch / Xbox / PS4 / PS5 / Steam Deck`).
  - **Debug tab:** **ASTC as RGBA8**, **Present queue** (`Auto / On / Off`), **Intel shader compiles** (`Auto / Parallel / Serial`), plus session-only (never saved) **Heart protection**, **No-clip flight** (returns `-1` when Off so double-tap B never triggers flight), and **Free camera**.
  - Bumped `menu_restart::kMax` (`8 -> 24`) so every restart-requiring row tracks pending changes cleanly.

## Preserved invariants & verification
- **Program cache & ABI preserved:** Zero edits to `gpu/vk/vk_api.h`, `gpu/vk/vk_state.h`, `gpu/vk/vk_shader.*`, `gpu/vk/eden/**`, `gpu/vk/sirit/**`, `gpu/vk/compat/**`, `host/**`, or `nx2/**`; `HOOKSHOT_SHADER_STAMP == 0x4eae666d8f924912ull` unchanged (100% compatible with `0.2.2` / `0.2.3` `mastersword.exe` and existing shader/program/pipeline caches).
- **Tests & Binary Checks:**
  - `python -I -B tests/test_tls_objects.py` PASS (`19 TLS_OBJECTs; 54 other namespace-scope thread_locals, none with a constructor or destructor`).
  - `python -I -B tests/test_dlss_checks.py --cxx clang++` PASS (`dlss checks: ok`, `-std=c++20 -Wall -Wextra -Werror`).
  - `python -I -B tests/test_menu_restart.py --cxx clang++` PASS (`menu restart: all checks passed; PASS: the restart rows and Restart now`).
  - Standalone C++ unit checks (`menu_theme`, `crt_policy`, `grass_distance`, `-std=c++20 -Wall -Wextra -Werror`) PASS.
  - `mastersword_vk.dll`: built standalone (`RX_VK_ONLY=1`, `RX_RELEASE=1`, NVIDIA DLSS + AMD FSR 4 enabled, `0 failed`), `10,717,184` bytes, SHA256 `6C191373B16FB383DAC6103F7383968E532B6994D1DFC229423879499E785795`, exports `mkwvk_close_asked` and `mkwvk_init`, zero `__llvm_prf` / `.lprfd` / ReShade markers.
  - `MasterSwordLauncher.exe`: `23,428,045` bytes, SHA256 `75F55D26D9E1EAA1262FC3F06D7DE4E37E7AD6F0CE8E5A078D793C2DD86AE113`.
- **Golden Rule checks:** Safe `1920x1080` Windowed, Upscaling Off, Frame Generation Off, Reflex Off, Anti-Lag 2 Off, HDR Off, CRT Off, Colour vision Off, Anisotropy Game's own, Grass Default, Rumble 100%, Motion On, FOV 100%, V-Sync Mailbox defaults verified across all 26 default assertions; strict allowlist staging (`13` Update / `15` Portable-StandAlone); zero personal configs, saves, caches, logs, or NGX runtime binaries bundled.

## Release target
- **GitHub (`STS78721/OOT-MASTERSWORD`):** `SteelTemple-MasterSword-0.2.4-Update.zip`, `SteelTemple-MasterSword-0.2.4-Portable-StandAlone.zip`, and `MasterSwordLauncher.exe` staged in `C:\Games\GitHUB Builds\MasterSword\` (awaiting user OK before commit/tag/push/publish).
