# GitHub release packaging notes (post tip-port waves)

## Done in tree (smoke-deployed; golden `bak_4x50` untouched)

### Wave A
- **#1999** RADV batch idle flush (`batch_gpu_idle`)
- **#1916** `probe_upscalers_soon()` on startup screen
- **#2122** Uncapped (`frame_rate_cap=6` / 1000 fps) + Graphics row (needs exe)
- **#1971** Mesh shaders row + device create (`mesh_shaders=`)
- **#2041** V-Sync row + swapchain recreate (`menu_vsync`)

### Wave B
- **#2147** FSR quality 0..7: 100/92/83/75 then AMD modes (`options_version=8` migrates old 1..4 → 4..7)
- **#2000** adapted: under DLSS, Resolution uses `step_render`/`snap_render` (Native/DLAA + twelfths); MS Upscaling taxonomy kept

### Wave C (partial)
- **#1581** tree-shadow mask stab (`shadow_stab.comp` + `mask_stabilize` on program `3427a1dbefd5e02c`)

## Still required before calling Wave C “done”

Order matters (state-list stub blocks the rest):

1. **#2046 HKSTATE2** — replace `state_list_run() {}` stub in `gpu/vk/vk_draw.cpp` with real v2 prepare/run; keep v1 readable. ~800 LOC tip commit `667ba79b`.
2. **#1980** refresh states during `--prepare-shaders` — after real `state_list_prepare`.
3. **#2032** predict-all / `disk_taken` / PrepareBar — after C2 (prefer after C3); MS startup screen wiring is the riskier half.

## Shipping / GitHub release follow-on (not code)

- Convert/ship tour + contributed pipeline lists for HKSTATE2 (**packaging**, not this tree’s compile).
- Smoke path only: `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\`
- **Never overwrite** `mastersword_vk.dll.bak_4x50` (golden).
- Rebuild: `runs/_release_prep/build_dlss_vk/rebuild_ngx_temporal.bat`
- Deploy: `work/sa/build/mastersword_vk.dll` (+ `mastersword.exe` when host/`nn_vi` changes)
- ABI: host 10/11, DLL 11, load `mastersword_vk.dll`
