# Handover → Gemini Carbon (MasterSword / Hookshot tip-port)

**From:** Composer (Cursor agent) session ending 2026-11-11 ~17:07 UTC+2  
**To:** Gemini Carbon  
**Workspace:** `C:\Games\Hookshot - NotRexed - Test e400f88`  
**User role:** Software engineer; wants surgical code work, concise answers, no surprise commits/PRs.

Read this entire file before changing anything. Also read always-apply rules:

- `.cursor/rules/mastersword-smoke-discipline.mdc`
- `.cursor/rules/upstream-perf-watch.mdc`
- `AGENTS.md` + `CONTRIBUTING.md` (no game files leave the PC; tests read source text)
- `runs/_release_prep/STICKY.md`

---

## 1. What this project is

- **Upstream:** [NotRexed/Hookshot](https://github.com/NotRexed/Hookshot) (`git remote origin`)
- **MasterSword release remote:** [STS78721/OOT-MASTERSWORD](https://github.com/STS78721/OOT-MASTERSWORD) (`git remote mastersword`)
- Local tree is a **heavily modified MasterSword fork** of Hookshot: branding, NVIDIA DLSS SR / FG stand-in / Reflex, FSR 4 path, Insert menu, launcher sync, smoke ABI quirks.
- Branch: **`mastersword-fg`** (dirty: ~188 modified/added paths — do **not** wholesale reset).
- Last local tip note: `0a91cb74 Merge remote-tracking branch 'origin/main' into yoink/upstream-perf-wb` — working tree has much more uncommitted work on top.

---

## 2. Smoke / golden / ABI (non-negotiable)

| Item | Value |
|---|---|
| Smoke folder | `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\` |
| Desktop bat | `MasterSword DLSS Test.bat` → that folder, `RX_BACKEND=vulkan`, `mastersword.exe` |
| Load DLL | **`mastersword_vk.dll`** (also copy as `hookshot_vk.dll` for fallbacks) |
| ABI | Host accept **10 / 11 / 4**; DLL built as **11**. Log: `vk: host ABI … (DLL 11)` |
| Golden | `mastersword_vk.dll.bak_4x50` (~6.3 MB) — **emergency play copy only**. Never overwrite. Never treat as edit source of truth. |
| Build out | `work\sa\build\mastersword.exe` + `mastersword_vk.dll` |
| FSR rebuild bat | `runs\_release_prep\build_dlss_vk\rebuild_fsr4.bat` (`HOOKSHOT_FSR=1`, NGX SDK, standalone) |
| Alternate bat | `rebuild_ngx_temporal.bat` / `rebuild_0.1.5_ports.bat` (older) |

**Hard rules (user + sticky):**

1. **Surgical diffs only** — no whole-file `git checkout` / stash restore / bak deploy unless user explicitly says so.
2. Do **not** touch `gpu/vk/vk_menu.cpp` unless the task is explicitly the menu (today’s ports did touch it when the user asked for menu-related upstream).
3. FG: never wait on busy OutSet; `synth_multiplier` soft-caps stand-in; DLSS MFG menu max **4x**.
4. Plan before restore / wholesale rebuild / ABI·menu·DLSS integration strategy.
5. Prefer fixing missing symbols surgically over swapping HEAD/stash files.

---

## 3. Shipped releases (GitHub)

| Tag | What | URL |
|---|---|---|
| **0.1.5c** | FSR 4 + tip ports (menu fit, pointer mailbox, Waves A/B bits, etc.) | https://github.com/STS78721/OOT-MASTERSWORD/releases/tag/0.1.5c |
| **0.1.5d** | Hotfix: host reads **`mastersword_options.ini`** (was stuck on legacy `hookshot_options.ini`) | https://github.com/STS78721/OOT-MASTERSWORD/releases/tag/0.1.5d |

**Packaging pattern (repeat this):**

- Notes: `runs/_release_prep/PATCH_NOTES_0.1.5x.md` (same structure as 0.1.5b/c/d)
- README: `runs/_release_prep/README_MASTERSWORD.md`
- Script: `runs/_release_prep/stage_and_upload_0.1.5x.py` (copy previous; PREV=`stage_0.1.5(prev)`; include FFX DLLs from `work/ffx-sdk/`)
- Stage from prior portable/update; stamp exe/vk/launcher; zip; `--upload` publishes release + README via git credential fill
- Assets: Portable zip, Update zip, `MasterSwordLauncher.exe`
- Version stamps: root `VERSION`, launcher `CURRENT_VERSION`, `HOOKSHOT_VERSION` via `nx2/version.py`, warmer/Insert must show the tag
- FFX ship files: `amd_fidelityfx_loader_vk.dll`, `amd_fidelityfx_upscaler_vk.dll`, `LICENSE-AMD-FSR-SDK.md`

**Critical:** Afternoon **perf ports are in the local smoke build (15:37) but were NOT shipped** as 0.1.5e. Online users on 0.1.5d do **not** have #2175/#2206/#2187/#2127/#2186 yet.

`gh` CLI is **not installed** on this machine; packaging used the Python urllib + `git credential fill` path.

---

## 4. Bug fixed this session: FPS stuck at 60 (0.1.5d)

**Symptom:** User set Game frame rate cap above 60; after restart still ~60. Feared “menu broken” + online users hit.

**Root cause:** Insert menu / launcher write **`mastersword_options.ini`**. Host (`hos_ini_option` in `host/nn_applet.c`, plus hid/audio/prompts/discord/ultrawide in `hos_main.c`) only opened **`hookshot_options.ini`**. No file → default `frame_rate_cap=0` → **60**. Same for `output_res` host path (log could show 2048x1152 while menu said 4K).

**Fix:** `hos_open_options_r()` in `host/hos_paths.h` — prefer `mastersword_options.ini`, fall back to `hookshot_options.ini`. Wired into applet/hid/audio/prompts/discord/hos_main.

**Also remember:**

- Cap applies **after restart** (`fps_max()` caches once in `host/nn_vi.c`).
- Menu help: Uncapped / higher caps need restart; V-Sync On + 60 Hz panel still locks present.
- With FG 4x, OSD may show **game** ~60 and **shown** frames higher — log lines distinguish `vi:` vs `frame interpolation:`.
- Smoke `mastersword_options.ini` currently has `frame_rate_cap=0` again (was briefly set to `2`=120 for test); `vsync=2` (Off), `fullscreen=1`, `output_res=2` (4K), `fg_engine=3`, `frame_generation=4`, `upscaling=0` (as of this handover snapshot).

---

## 5. Menu “broken again”

User reported Insert menu broken after 0.1.5c deploy (fullscreen 4K historically blank when panel taller than swapchain). Fit/clamp code is in `vk_menu.cpp` (`menu_panel_h` / `scale_of` / `redraw_menu` / `copy_over` clip). That was in 0.1.5c.

After that, some “broken” reports may have been **settings not sticking** (ini bug above), not a blank panel. If Insert is literally blank at 4K again, re-check swap extent + panel height path — do not restore golden unless user asks.

Pointer mailbox (#1428) is in tree: `host/display_pointer.h`, `host/display.c`, SDL path; menu uses `host->pointer`.

---

## 6. FSR 4 (0.1.5c feature)

- Upscaling row **9** = FSR 4; quality via shared `opts.dlss_quality` with visibility gate.
- Build: `HOOKSHOT_FSR=1`; headers `third_party/fidelityfx`; fetch via `scripts/ffx_sdk.py`.
- Runtime DLLs beside exe (smoke + release zips).
- On user’s **RTX 5070 Ti**: log `AMD FSR 4: no FSR 4 for this GPU` (library lists 3.1.6 / 2.3.4) — **expected**; use **DLSS SR**.
- DLSS path unchanged (presets K/M/L). FG stand-in still “DLSS MFG” label.

---

## 7. Upstream tip ports already in this tree (earlier waves)

See `runs/_release_prep/RELEASE_PACKAGING_NOTE.md` and `FSR4_PORTS_2026-11-11.md`.

**Wave A (done):** #1999 batch idle, #1916 upscaler probe, #2122 Uncapped, #1971 mesh shaders row, #2041 V-Sync  
**Wave B (done):** #2147 FSR quality steps, #2000 DLSS Resolution render scales (MS taxonomy kept)  
**Wave C partial:** #1581 tree-shadow stab  

**Also ported earlier this day:** #1754 sampler admission (mostly already present + docs), #1428 pointer mailbox.

**Wave C still blocked:**

1. **#2046 HKSTATE2** — replace `state_list_run() {}` stub in `gpu/vk/vk_draw.cpp` (~800 LOC tip `667ba79b`)
2. **#1980** prepare refresh — needs real `state_list_prepare` (skipped today for that reason)
3. **#2032** predict-all / PrepareBar — after C2

Comment in tree: `/// State-list precompile hook … No-op until that lands` + `void state_list_run() {}`.

---

## 8. Performance ports applied this afternoon (local smoke only)

User asked to pull listed upstream perf work and build. Applied surgically; rebuilt with `rebuild_fsr4.bat`; deployed to smoke **2026-11-11 15:37**.

| Upstream | Local status |
|---|---|
| **#2175** zero big textures → GPU clear | **IN** `gpu/vk/vk_texture.cpp` (`upload_all_zero`, `RX_VK_ZERO_UPLOAD=0` off) |
| **#2206** `RX_VK_PCACHE_LOAD=0` skip multi-GB pcache | **IN** `gpu/vk/vk_draw.cpp` (`pcache_no_save`) — **opt-in env** |
| **#2187** first-start Quality preset by VRAM | **IN** `vk_menu.cpp` / `vk_menu.h` / `vk_main.cpp` (`menu_first_start_for_gpu`) |
| **#2127** vcore escape default 2 s (was 250 ms) | **IN** `host/hos_thread.c` |
| **#2186** prepare progress log every 10 s | **IN** `vk_draw.cpp` (`prepare_progress_log`) |
| **#1999** batched submits when GPU idle | **Already present** in `vk_replay.cpp` |
| **#1980** Builder state-list refresh | **SKIPPED** — needs Wave C |
| **#2181** FPS counter graph / frame-time levels | **SKIPPED** — ~1k LOC menu/host; diagnostics, not frame time |

**Not re-versioned / not uploaded** after these ports. `VERSION` / launcher still say **0.1.5d**. Next ship should be **0.1.5e** (or similar) if user wants online to get perf ports.

Optional tip for user RAM pressure: `RX_VK_PCACHE_LOAD=0` (~3 GB less resident; longer cold compiles).

---

## 9. Upstream watch (as of 2026-11-11 ~17:05)

- `origin/main` tip: **`49f75dc8`** (2026-11-11 07:30 -0500) — no newer merges since afternoon ports.
- Open PR to watch hard: **#2314** `perf(vk): texture shadow copies from cached RAM, not write-combined capture` (not merged).
- Also open / relevant: **#2322** vcores while movie decodes; **#2181** FPS graph; **#1980** still Wave-C gated.
- Rule: `.cursor/rules/upstream-perf-watch.mdc` — **flag** performance upstream; do **not** auto-port unless asked.
- GitHub PR auto-subscribe failed earlier (`Could not verify GitHub access to notrexed/hookshot`). Use `git fetch origin main` + API/WebFetch.

---

## 10. Hardware / play context (user)

- GPU: **NVIDIA GeForce RTX 5070 Ti** (not 3070 Ti — user corrected).
- Smoke log previously: DLSS NGX OK; FG “DLSS MFG (stand-in) 4x”; FSR 4 no model; fullscreen → swapchain **3840x2160**, present **immediate** (V-Sync Off).
- User cares about **extra performance** and whether **online users** share bugs (they did for the ini FPS bug → shipped 0.1.5d).

---

## 11. How to rebuild & deploy (smoke)

```bat
:: From repo root
runs\_release_prep\build_dlss_vk\rebuild_fsr4.bat
```

Then copy:

- `work\sa\build\mastersword.exe` → smoke
- `work\sa\build\mastersword_vk.dll` → smoke **and** `hookshot_vk.dll`
- FFX DLLs from `work\ffx-sdk\` if missing
- Launcher: `launcher\dist\MasterSwordLauncher.exe` after `pyinstaller --noconfirm MasterSwordLauncher.spec` in `launcher\`

Verify stamps: ASCII search for version string in exe/vk; FSR markers `AMD FSR 4` / `amd_fidelityfx_loader_vk` in vk.

Version string comes from root `VERSION` via `nx2/version.py` → `-DHOOKSHOT_VERSION=...`.

---

## 12. How to ship next release (if asked)

1. Bump `VERSION` + `launcher/MasterSwordLauncher.py` `CURRENT_VERSION`
2. Write `PATCH_NOTES_0.1.5e.md` matching prior style
3. Update `README_MASTERSWORD.md` stamps
4. Copy `stage_and_upload_0.1.5d.py` → `…0.1.5e.py` (PREV=`stage_0.1.5d`)
5. Rebuild exe+vk (FSR) + launcher
6. `python -u -X utf8 runs\_release_prep\stage_and_upload_0.1.5e.py --upload`
7. Deploy smoke; confirm warmer shows new version

Do **not** commit/push/PR unless user asks (user rules).

---

## 13. Important code map

| Concern | Where |
|---|---|
| Insert menu / options / FPS cap row | `gpu/vk/vk_menu.cpp`, `vk_menu.h` |
| Options path | menu → `mastersword_options.ini`; host → `hos_open_options_r` |
| Frame pacing / fps_max | `host/nn_vi.c` |
| V-Sync present mode | `vk_main.cpp` create_swapchain + `menu_vsync()` |
| FG stand-in | `vk_framegen.cpp` / menu FG rows |
| DLSS SR | `vk_dlss.cpp`, temporal / `upscaler_rows.h` |
| FSR 4 | `vk_fsr4.cpp`, Upscaling=9 |
| Pipeline cache | `vk_draw.cpp` `pcache_init` / `pipeline_cache_save` |
| Zero-texture upload | `vk_texture.cpp` `upload_all_zero` |
| Batch submit idle | `vk_replay.cpp` `batch_gpu_idle` |
| Vcores escape | `host/hos_thread.c` |
| Pointer mailbox | `host/display_pointer.h`, `display.c` |
| Tests | Many compile slices of `vk_menu.cpp` — run `python -I -B tests/run_all.py` after menu reshapes |

---

## 14. Open user threads / next likely asks

1. **Ship 0.1.5e** with afternoon perf ports (+ maybe enable notes for `RX_VK_PCACHE_LOAD=0`).
2. Port **#2314** when merged (texture shadow cached RAM).
3. Port **#2181** if user wants FPS graph (large; menu-heavy).
4. Wave C **#2046 → #1980 → #2032** if they want state-list / Builder prep (big, fragile).
5. Re-verify Insert menu at 4K fullscreen if still visually broken after ini fix.
6. Confirm FPS > 60 after setting cap + restart on 0.1.5d+ build (host now reads mastersword ini).

---

## 15. Session chronology (compressed)

1. Continued tip-port / smoke work; fullscreen menu fit; DLSS/FG OK on 5070 Ti.
2. Implemented FSR 4 + tip ports (#1754/#1428); Wave C deferred.
3. Packaged/shipped **0.1.5c**; smoke briefly left on pre-stamp 0.1.5b → user saw old version → redeployed.
4. User FPS stuck at 60 → explained default/restart → then found **ini filename bug** → fixed + shipped **0.1.5d**.
5. User asked to be told about upstream perf → rule + sticky; listed ahead-of-tree items.
6. User: pull those in and build → ported #2175/#2206/#2187/#2127/#2186; skipped #1980/#2181; #1999 already in; smoke updated 15:37; **not shipped**.
7. User: any other new perf? → nothing new on main; watch **#2314** open.
8. User: proper handover to Gemini Carbon → **this file**.

---

## 16. Do / Don’t cheat sheet for Carbon

**Do**

- Surgical ports from `origin` commits; rebuild FSR bat; deploy smoke; verify log lines.
- Flag new upstream perf; ask before shipping.
- Prefer current working tree over golden/bak restores.

**Don’t**

- Overwrite `bak_4x50`.
- Whole-file checkout/stash to “fix” compile.
- Assume online == local smoke (0.1.5d ≠ afternoon perf build).
- Auto-port Wave C / #2181 without user OK.
- Commit/push/PR unless asked.
- Exfiltrate game dump / `image.bin` / romfs.

---

*End of handover. If anything conflicts with always-apply rules, rules win.*
