# SteelTempSaint's MasterSword — 0.1.3c

**NVIDIA Reflex is live**, and launcher settings finally stick across exit / Insert-menu saves.

## What's New in 0.1.3c

* **NVIDIA Reflex** (`VK_NV_low_latency2`) — hardware-gated to NVIDIA GPUs with driver support
  * Insert menu: **NVIDIA Reflex** Off / On / Boost
  * Launcher Low-Latency Mode (`boost` / `on` / `off`) maps to the same setting
  * Sleep before frame acquire + Simulation / RenderSubmit / Present markers (out-of-band when present queue is separate)
* **Launcher settings persist**
  * Durable backup in `mastersword_launcher.json`
  * Reloads game folder + settings on every launcher start, Settings open, and Play
  * Insert menu no longer wipes launcher keys when it saves
  * `output_res` accepts both `0/1/2` and `2560x1440`-style values (no more silent fall-back to 2048)
* Keeps **0.1.3b** pink / native 1440p–4K scene fix

## Expected in the log (NVIDIA)

* `vk: NVIDIA Reflex boost (VK_NV_low_latency2)` (or `on` / `off`)
* With Output 1440p/4K still: `oe: output resolution 2560x1440` / `3840x2160` and native crop

## Install

| Zip | When |
|---|---|
| **`SteelTemple-MasterSword-0.1.3c-Update.zip`** | You have 0.1.3 / 0.1.3a / 0.1.3b — **overwrite** |
| `SteelTemple-MasterSword-0.1.3c-Portable-StandAlone.zip` | Fresh install |

Restart after swapping files. Output resolution still needs a restart to take effect.
