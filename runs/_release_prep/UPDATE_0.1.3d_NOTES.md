# MasterSword 0.1.3d (prep)

## Why
Frame generation at 4x/6x locked the game to ~50 FPS on 1440p/4K (display thread kept OutSets busy ~20 ms while synthesising full-res intermediates). Clients on mixed PCs also need the upstream perf/stability already on this tree.

## Upstream already in this branch (0a91cb7 base)
No extra cherry needed for these — already merged via earlier upstream pulls:
- Write-back diff on by default (~+20% where measured); present queue stays off (#1161)
- Render-target image pool + VMA RT blocks (#1416 / #1436)
- Batched eviction write-backs (#840)
- Capture waits only pages it copies (#1243 family)
- Swapchain replace recovery (#1403)
- Deku Tree floor web on NVIDIA (#1442)
- Smoother-play bundle (#1485): upload budget, 8 GB VRAM clamp, HDR10, FPS cap, Discord, Debug tab

## This update (FG)
- FG engine: MasterSword = Hookshot only when chosen; DLSS MFG / SMFG / FSR / XeSS are separate rows
- DLSS MFG menu clamp: 2x/3x/4x (NVIDIA MFG); 6x no longer a fake option on that engine
- **Never block the game** waiting for a free FG OutSet (root cause of game 50)
- At 1440p+ stand-in **synth caps at 3x** so game stays locked 60 (log explains the cap); true 4x needs Streamline DLSS-G evaluate
- Failed high-mult image alloc no longer disables FG forever

## Build deployed 2026-11-10
- Built: `work/sa/build/mastersword_vk.dll` (~4.7 MB; Streamline DLSS SR stubbed in this rebuild — FG path is what changed)
- Deployed over: `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\mastersword_vk.dll`
- Backups: `mastersword_vk.dll.bak_4x50` (old 4x→50), `mastersword_vk.dll.bak_pre_fgfix` (immediately before this deploy)
- CPM: `RX_CPM` → oot-main-candidate `.cache\cpm` (local cache was purged)

## Test plan
- [ ] Title + dungeon: DLSS MFG **3x** → game 60 / ~180
- [ ] Same: **4x** → game **60** (shown may be ~180 if synth-capped at 1440p+; must not lock 50)
- [ ] Switch FG engine to MasterSword 3x → still game 60
- [ ] Low-end: Quality preset Minimum, FG off → playable
- [ ] Log contains `MasterSword/Hookshot` or `DLSS MFG (stand-in)` and no permanent `images were not made: off`

## GitHub prep
- Branch: `mastersword-fg` @ `0a91cb7` + uncommitted FG/menu/color WIP
- Do **not** commit until 4x retest passes and Streamline DLSS body is restored (current `vk_dlss.cpp` is a stub)
- Tests: many MasterSword-vs-upstream menu/text checks fail on this hybrid tree; FG source changes are local — re-run focused checks after Options stabilize

## Black screen fix (2026-11-10 13:12)
- Cause: WIP k_api.h was ABI **5**; smoke mastersword.exe is ABI **11** ? Vulkan refused to start ? WebGPU black window.
- Restored HEAD k_api.h (v11), rebuilt, redeployed. ak_4x50 remains the last known Streamline DLL.


## Black screen #2 (host ABI 10)
- Smoke `mastersword.exe` is **host ABI 10**; DLL is built as **11**. Working bak accepted 10/11/4 and echoed `table.version = h->version`.
- Strict `version == 11` rejected the exe ? WebGPU black screen.
- Restored that accept gate; **source FG fixes kept**; `bak_4x50` left untouched as rollback only.
