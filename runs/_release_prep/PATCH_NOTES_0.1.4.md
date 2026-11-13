# SteelTempSaint's MasterSword — 0.1.4

## DLSS Update — the one you've been waiting for

**NVIDIA DLSS Super Resolution is live** on MasterSword's Vulkan path — transformer presets **K / M / L**, real Performance (M) ~1080→4K, Quality/Balanced/DLAA on K, Ultra Perf on L. Pair it with Nodix **DLSS MFG** stand-in frame gen, **NVIDIA Reflex**, and a launcher that finally speaks the Insert menu's language.

This is the DLSS drop. Bag it. Tag it. Play it loud.

## What's New in 0.1.4

* **DLSS Super Resolution (Streamline)**
  * Upscaling → **DLSS SR** in the Insert menu
  * Quality / Balanced / DLAA → preset **K**; Performance → **M**; Ultra Performance → **L**
  * Internal 3D matches NGX optimal sizes (e.g. Performance ~1080p → 4K Output)
  * Needs Streamline + `nvngx_dlss.dll` beside `mastersword.exe` (launcher **Download & Install DLSS SDK**)
* **Launcher ↔ Insert menu sync (fixed for real)**
  * SETTINGS now writes the **same flat keys** the game loads: `upscaling`, `fg_engine`, `frame_generation`, `dlss_quality`, `fg_model`, …
  * Change DLSS / FG / Reflex / Output in the launcher — open Insert in-game and **they're there**
  * Change them in-game — reopen SETTINGS and **they stick**
  * Warns if no game folder is selected (so you don't save into the void)
* **DLSS / Nodix FG / Reflex in the launcher UI**
  * Super Resolution list includes **DLSS**, SMRS, SMAI, FSR, XeSS…
  * Frame Generation Mode includes **DLSS MFG** + multiplier + FG model A/B/C
* Keeps **0.1.3c** Reflex + settings persistence, **0.1.3b** native 1440p/4K pink fix

## Expected in the log (NVIDIA + DLSS)

* `vk: DLSS: slInit ok`
* `vk: DLSS SR ready (Streamline; presets K/M/L…)`
* `vk: DLSS 4: Evaluate ok Performance(M) (1920x1080 → 3840x2160, …)` (or Quality/Balanced/etc.)
* `vk: NVIDIA Reflex boost` (or on / off)

## Install

| Zip | When |
|---|---|
| **`SteelTemple-MasterSword-0.1.4-Update.zip`** | You have 0.1.3 / 0.1.3a–c — **overwrite** |
| `SteelTemple-MasterSword-0.1.4-Portable-StandAlone.zip` | Fresh install |

1. Close the game + launcher  
2. Overwrite with the Update zip (or unpack Portable)  
3. Open launcher → SETTINGS → **Download & Install DLSS SDK** if DLLs are missing  
4. Set Upscaling to **DLSS**, Output / quality as you like → **SAVE & APPLY**  
5. PLAY — confirm Insert menu matches  

Restart after changing Output resolution. Sell it like it's hot.
