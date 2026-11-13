# MasterSword 0.1.7

**Release date:** 2026-11-12  
**Type:** Live Field of View, High-FPS World/Physics/Audio Fixes, Temporal FSR 3.1, Crash Fixes & 5K2K Ultrawide update (drop-in over 0.1.6 / 0.1.5e / 0.1.5d / 0.1.5c)

Feature, gameplay-accuracy, upscaler, and stability release adding a **live Gameplay Field of View slider (`50%–150%`)** synced between the in-game `Insert` menu and `MasterSwordLauncher.exe`, **>60 FPS sound, moving-platform physics, time-of-day, and movie-pacing fixes**, **FSR 3.1 on the shared temporal chain with character motion vectors**, **`OptimizeChangeBank` room-transition heap-full retry crash prevention**, **Native Mouse Camera refinements**, and **5120×2160 (5K2K) native ultrawide support**.

---

## Highlights

- **Live Gameplay Field of View (`50%–150%`)** — new **Field of view** slider under **`Insert` → Gameplay** (and synced with **SETTINGS → Display → Field of View** in `MasterSwordLauncher.exe`); widens or narrows gameplay vertical FOV in real time while automatically leaving cutscenes, dialogue framing, and title cameras untouched at `100%`
- **Complete >60 FPS World, Physics & Sound Fixes** — fixes moving-platform carry/drift above 60 FPS via physics solver sub-stepping, gates `SoundJob` execution to 60 Hz so footsteps/ambient triggers never double-fire, carries sub-frame remainders on the time-of-day clock (`f_803edf70`), and paces movie playback accurately across high refresh rates
- **AMD FSR 3.1 on the Shared Temporal Chain** — promotes **Upscaling = FSR 3.1** onto the unified temporal pipeline (`vk_temporal.cpp`) alongside DLSS & FSR 4, gaining dilated + per-character/actor motion vectors, reactive masks, and shared temporal jitter
- **Scene/Room Transition Crash Prevention & Cooperative-Matrix Shader Fix** — retries `OptimizeChangeBank` allocations cleanly when the game heap fills during room transitions instead of aborting, and adds a subgroup `MEMBAR` between shared-memory `STS` → `LDS` pairs to eliminate bright `16×2` bar artifacts on NVIDIA cooperative-matrix paths
- **Native Mouse Camera Polish & 5120×2160 (5K2K) Ultrawide** — refines bow/slingshot aim handoffs, ledge/climb camera resets, and smooth pitch/yaw tracking under **Native mouse camera**, and raises the native 21:9 ultrawide render-target ceiling to **5120×2160**

---

## Changed

### Gameplay, High-FPS Accuracy & Input
- **Live Gameplay FOV (`host/oot_fov.c`, `nx2/oot_fov.py`, `vk_menu.cpp`, `MasterSwordLauncher.py`):** hooks perspective setup (`0x804e52f8`) to scale vertical gameplay FOV from `50%` to `150%` (default `100%`) while preserving `100%` framing in cutscenes and menus; bidirectionally synced via `field_of_view` in `mastersword_options.ini`
- **>60 FPS Physics, Sound & Clock (`host/oot_high_fps.c`, `nx2/oot_patches.py`, `code_003.c`, `code_046.c`, `code_048.c`):**
  - Sub-steps physics solver velocity/position integration so moving platforms carry Link accurately above 60 FPS
  - Gates `SoundJob` updates to 60 Hz ticks so ambient and animation-driven audio triggers fire at native cadence
  - Accumulates sub-frame remainders on the Hyrule time-of-day counter (`f_803edf70`) and fixes FMV/movie frame pacing at high frame rates
- **Room-Transition Heap Retry (`host/oot_heap_retry.c`, `nx2/oot_patches.py`):** intercepts `OptimizeChangeBank` heap-exhaustion paths during scene/bank swaps so transient fragmentation retries cleanly instead of crashing
- **Native Mouse Camera (`host/mouse_native.c`, `host/nn_hid.c`):** improves first-person bow/slingshot/hookshot aim transitions, ledge-climb camera alignment, and raw mouse delta accumulation
- **Audio Renderer (`host/nn_audio.c`):** implements `RemoveAux` (`SubMixType` & `FinalMixType`), `SetAuxEnabled`, `SetAuxInputOutput`, and `SetSplitterBiquadFilterParameter`

### Upscalers, Ultrawide & Vulkan Back End
- **Unified Temporal Chain for FSR 3.1 (`vk_temporal.cpp`, `vk_fsr3.cpp`, `vk_replay.cpp`):** routes FSR 3.1 Super Resolution through the main temporal pass so actor/character motion vectors, reactive masks, and `temporal_jitter_size()` match DLSS and FSR 4 (`RX_VK_FSR3_CHAIN=0` retains legacy fallback)
- **DLSS Transient Failure Recovery (`vk_dlss.cpp`):** recovers gracefully from transient NGX evaluation failures across resolution/fullscreen transitions and verifies local `ngx\nvngx_dlssg.dll` availability cleanly
- **NVIDIA Cooperative-Matrix Shared-Memory Barrier (`lower_to_maxwell.h`):** emits a subgroup barrier before unpredicated `LDS` reads following `STS` shared-memory stores, fixing `16×2` bright bar artifacts on Ampere/Ada/Blackwell cooperative-matrix paths
- **5120×2160 Ultrawide Ceiling (`host/hos_wide_size.h`, `host/hos_main.c`):** supports native 5K2K (`5120×2160`) 21:9 ultrawide render targets without clamping down to 3440×1440
- **Vulkan Memory, Texture & Draw Refinements (`vk_memory.cpp`, `vk_texture.cpp`, `vk_draw.cpp`):** tighter staging/render-target reuse and reduced descriptor/barrier churn

### Safe Out-of-the-Box Defaults (Golden Rule)
- Fresh installs always default to a safe, vendor-neutral **1920×1080 Windowed** profile (**Upscaling = Off**, **FG engine = Off**, **Reflex = Off**, **Field of view = 100%**) while leaving existing `mastersword_options.ini` settings untouched when updating

---

## Install

| Asset | Use when |
|---|---|
| **`SteelTemple-MasterSword-0.1.7-Update.zip`** | You already have 0.1.6 / 0.1.5e / 0.1.5d / 0.1.5c / 0.1.5b / 0.1.5 / 0.1.4 — open **MasterSwordLauncher** for 1-click auto-update, or extract over your game folder |
| `SteelTemple-MasterSword-0.1.7-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

1. Close the game before updating.
2. Open **`MasterSwordLauncher.exe`** — it detects **`0.1.7`**, downloads `SteelTemple-MasterSword-0.1.7-Update.zip`, verifies `mastersword.exe`, and restarts into **`V 0.1.7`** automatically (or extract the Update zip directly over your game folder).
3. Launch the game — shaders will re-prepare once where needed for the shared-memory barrier update, and both the startup warmer and **`Insert`** menu will show **`0.1.7`**.

Your dump (`image.bin` + `romfs\`), saves (`user\`), and personal `mastersword_options.ini` stay untouched on your PC.

---

## Credits

SteelTempSaint community and testers · NotRexed and Hookshot contributors · Eden / Eglantine shader stack · AMD FidelityFX / FSR SDK · everyone testing high-FPS gameplay, ultrawide, upscalers, and hardware frame generation
