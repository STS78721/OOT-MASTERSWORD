# 🗡️ SteelTempSaint's MasterSword — 0.1.3b

**Pink wash at Output 1440p/4K is fixed.** Stock Resolution 100% now renders the 3D at the Output size (not the leftover 1536×864).

## ⚡ What's New in 0.1.3b

* **Native scene base**: fixed Resolution / FSR·XeSS·Nodix quality turns on mode‑1 base scale **1.0** (was stuck at the game's **0.8** → 0.75 of 2048 = **1536×864** even at 100% / 1440p)
* **Cached-scale refresh** at Resolution 100% so the fixed path doesn't keep the dynamic path's last 0.75
* Keeps **0.1.3a**: Output 1440p/4K, upscaler quality = % of Output, window crop override

## ✅ Expected in the log (Output 1440p, Resolution 100%)

* `boot: Resolution fixed: scene base scale 1.0`
* `native-scene: profile … mode 1 base scale 0.8 -> 1.0`
* `nvn: window crop 0,0 2560x1440`
* 3D clears / targets at **2560×1440** (or your Resolution %), **not** `1536x864`

## 📦 Install

| Zip | When |
|---|---|
| **`SteelTemple-MasterSword-0.1.3b-Update.zip`** | You have 0.1.3 / 0.1.3a — **overwrite** |
| `SteelTemple-MasterSword-0.1.3b-Portable-StandAlone.zip` | Fresh install |

Restart after swapping files. Output resolution still needs a restart to take effect.
