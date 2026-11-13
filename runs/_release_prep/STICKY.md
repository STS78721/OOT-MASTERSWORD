# Sticky — MasterSword FG / smoke (keep short; paste into chat after compaction)

```
ABI host 10 / DLL 11; load mastersword_vk.dll; golden = bak_4x50
No whole-file checkout/stash/bak deploy unless I say so — surgical diffs only
Do not touch vk_menu.cpp unless the task is the menu
FG: no OutSet wait; menu 4x synthesises fully; soft-cap only 6x/8x→4 at 1440p+; DLSS MFG max 4x
GOLDEN RULE (GitHub Releases): NEVER ship personal PC settings (.ini/.json/caches) and ALWAYS keep exe/DLL/Launcher fresh-install defaults on safe 1080p Performance/Default profile (never 4K/DLSS-forced out of the box)
Plan mode before restore/rebuild strategy
```

Golden: `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\mastersword_vk.dll.bak_4x50`
Smoke: `C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\`

Upstream perf watch: tell me when NotRexed/Hookshot lands FPS/pacing/CPU-GPU/cache/VRAM work we have not ported (see `.cursor/rules/upstream-perf-watch.mdc`).
