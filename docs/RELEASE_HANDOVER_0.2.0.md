# MasterSword 0.2.0 — release handover

*Branch `mastersword-fg` in `C:\Games\Hookshot - NotRexed - Test e400f88`, prepared 2026-11-17. Staged locally in `runs/_release_prep/stage_0.2.0` and `runs/_release_prep/dist/`; not pushed to remotes or uploaded yet.*

## What 0.2.0 includes
- **25 upstream `MagicBeanDev/Longshot` ports** (`longshot/main` checked through `544229a2` on 2026-11-17) integrated with zero visual fidelity downgrades:
  - **Performance & Town/Field FPS:** `#231` flat per-draw handle/constant caches (`flat_map.h`), `#590` cached `getenv` locks (`vk_mvec.cpp` `env_on()`, `vk_draw.cpp` `watch_wanted()`) + 64 KB relaxed-atomic `wrote_regions` + partial `vkCmdBindVertexBuffers2`, `#994` + `#1255` dynamic render-target pool (`target_pool.h`, `256–768 MB`, 8-present idle trim) & per-present `dead_targets_sweep` (preserving MasterSword's `RX_VK_DEAD_MIN_UNUSED=3` safety margin and 96 MB pressure pool floor), `#1256` character shadow-map 384 px storage bucketing (`host/shadow_slot.h`, `HOS_SHADOW_BUCKET=384`), `#1222` GPU-copy reuse for finalized ASTC textures (`astc_reuse.h`), `#568` 2 s staging arena burst retention (`RX_VK_STAGING_BURST_S=2`), `#384` single-layer BC3 decode scratch (`20–36 MB` vs `160–288 MB`), `#442` LRU sampler retirement.
  - **AMD / Low-VRAM & Memory Safety:** `#1091` `vram_compact::past_allowance` (triggers give-back when AMD's Windows driver reports `allowed == used`), `#1083` `collect_mu` serializing page-collection vs submit captures, `#468` / `#469` / `#431` `import_reach` unbounded span scans + `bg_inflight`/`dead_imports` page decommit protection + `lower_bound(hi)`, `#440` `gpu_pool_piece` command-order inline writes/clears, `#553` / `#554` / `#1251` readback timeout retry & busy eviction skip, `#1248` sparse bind retry (`sparse_free`), `#1244` unsupported texel format dummy view, `#672` partial BC3 edge guard.
  - **Rendering, Upscaler, Sync & Host:** `#769` met-wait CPU capture (`met_wait_capture_on`, `hold_gpu.h`), `#311` DLSS optimal feature creation dims, `#788` GPU timeline completion checks for upscaler/headless ImGui frees, `#621` atomic compute pipeline handles, `#661` thread-safe `RX_VK_SHADOW_BIAS`, `#552` stream bounds guard, `#670` clearer `VK_ERROR_DEVICE_LOST` timeline diagnostics, `#594` virtual-core 1 s starvation priority boost (`HOS_VCORE_STARVE_US`), `#488` 240 fps movie pacing at the 60 fps cap (`HOS_MOVIE_PACE`), `#1167` `WAVE_FORMAT_EXTENSIBLE` 5.1 channel mask check.
- **0.2.0 version stamp & clean assets:** `VERSION` = `0.2.0`, `host/hos_update.c` `RELEASES_PAGE` set to GitLab `permalink/latest`, transparent `assets/mastersword.ico` / `mastersword.png` in staged zip & smoke folder, `MasterSwordLauncher.exe` updated to `0.2.0` + GitLab links.

## Preserved invariants
- **7.4 GB program cache & ABI (`10/11`) preserved:** zero edits to `vk_shader.*`, `vk_api.h`, `vk_state.h`, `vk_menu.*`, `eden/**`, `sirit/**`, `compat/**`, `nx2/oot_patches.py`, `nx2/lift.py`, `nx2/oot_wide.py`, `runtime/mkw_host.h`.
- **0 lifted chunks recompiled:** all `code_*.c` / `code_hot_*.c` chunks skipped (`0s`) in `work\build_full.bat`.
- **Zero personal settings bundled:** staged exclusively from clean build outputs and `runs/_release_prep/stage_0.1.9d/portable` seed assets via allowlist.

## Live playtest checklist (smoke folder)
Smoke folder updated with `*.bak_pre_020` backups (`mastersword_vk.dll.bak_4x50` untouched):
- Play at 4K + DLSS SR + MFG: Castle Town Market (check 1% lows, shadow bucketing stability, zero 1-frame RT flickers or dark Link/scene flashes), Hyrule Field, Kokiri Forest (ground mist), Deku Tree floor web, and Scarecrow's Song.
- Play a pre-rendered movie at 60 fps cap and check audio pan/center channels.
- Verify Field -> Town -> Field keeps base fps and `mastersword_options.ini` / `user/` saves behave normally.

## Publish steps (once playtest passes)
1. Push `mastersword-fg` (or fast-forward `main`) to `gitlab-sts` / `mastersword` as requested.
2. Upload `runs\_release_prep\dist\SteelTemple-MasterSword-0.2.0-Portable-StandAlone.zip`, `SteelTemple-MasterSword-0.2.0-Update.zip`, and `MasterSwordLauncher.exe` with `runs\_release_prep\PATCH_NOTES_0.2.0.md` to the GitLab `0.2.0` release.
