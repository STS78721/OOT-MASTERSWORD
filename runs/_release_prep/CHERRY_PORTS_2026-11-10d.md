# Upstream ports 2026-11-10d (ship 0.1.5)

Surgical into MasterSword tree; no blind merge of ~168 commits.

## Ported

| PR | Notes |
|----|--------|
| #1506 | Texture squeeze: no 97% ratchet while VMA blocks have room (`vk_texture.cpp`) |
| #1762 | Four submitted pages for replay reads (`vk_memory.cpp`) |
| #1769 | Lower `TLD.MS` to Maxwell TLDS 2D.LZ.MS |
| #1507 | AMD 64-lane warps + fragment shuffle fallback (`vk_shader` / warp / main / profile) |
| #1497 | Long paths / `\\?\` (`host/nn_fs.c`, `nx2/nup.py`, `make_game.long_path`) |
| #1510+#1511 | Mesh fallback record allocator + capacity **65535** (SPIR-V mesh + `vk_draw` fallback) |

## Skipped this ship

| PR | Why |
|----|-----|
| #1008 / #1773 FSR4 | Conflicts with Streamline DLSS stack |
| #1448 presentation crop | Touches `vk_framegen.cpp`; FG 4x ship-critical |
| #1502 capture readback | Large `vk_replay` overlap |
| Builder / Linux / CI / RecompLauncher | Not needed for Windows play zip |

Branding/version **0.1.5** already on smoke before these ports.
