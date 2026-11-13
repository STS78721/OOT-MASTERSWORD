# 🗡️ SteelTempSaint's MasterSword — 0.1.3a

**Resolution / upscaler release** — Output resolution + internal 3D scale behave like a flagship PC title.

> **No copyrighted game files are included.**

---

## ⚡ What's New in 0.1.3a

### Output + upscalers (PC-style)
* **Output resolution** still sets the final frame (2048 / **2560×1440** / **3840×2160**, restart)
* **FSR / XeSS / Nodix quality** now drives **internal 3D render scale as a % of that Output**
  * Example: Output **1440p** + FSR **Quality** → 3D at **~67% of 2560×1440** (then upscaled to 1440p)
  * FSR: Native AA 100% · Quality **67%** · Balanced 58% · Performance 50% · Ultra 33%
  * XeSS / Nodix: matching quality tiers
* **Resolution** row shows `Set by FSR (67% · WxH)` while a vendor upscaler owns the scale; otherwise it stays Auto / 100% / 75% / 67% / 50% of Output

### Pink / washed 3D fix
* **Window crop override**: with Output unlocked, crop matches the real framebuffer (no more `2048×1152` crop on a `2560×1440` buffer → pink wash with HUD still fine)

### Still from 0.1.3 / 0.1.2e
* MasterSword branding, full runtime DLL bundle, `hookshot_vk.dll` alias, renderer wait, launcher `image.bin`+`romfs`

---

## ✅ Expected log lines (1440p + FSR Quality)

```
oe: output resolution 2560x1440
boot: native output bridge 2560x1440 (output_res=1)
nvn: window crop 0,0 2560x1440
f_80ea2f30 AFTER: min=(2560,1440)
```

If you still see `crop 0,0 2048x1152` or `min=(2048,1152)`, you are not on 0.1.3a — reinstall the Update zip.

Optional cleanup after updating: delete `hookshot_programs.bin` and `mkw_vk_pipelines.bin` / `hookshot_pipelines.bin`, then relaunch.

---

## 📦 Downloads

| Package | Use this if… |
|---------|----------------|
| **`SteelTemple-MasterSword-0.1.3a-Update.zip`** | You have 0.1.3 / 0.1.2x — **overwrite** |
| `SteelTemple-MasterSword-0.1.3a-Portable-StandAlone.zip` | Fresh install |
| `MasterSwordLauncher.exe` | Launcher-only refresh |

---

**Use your own game data. Play fair.**
