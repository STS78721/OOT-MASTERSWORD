# Upstream ports 2026-11-10b

Surgical ports into MasterSword working tree (no whole-file checkout).

## In this build
| PR | What | Where |
|----|------|--------|
| #1677 | Stick axis steers only after seen at rest | `host/pad_sdl.c` |
| #1612 | Cap >60 keeps real game speed (timed step) | `host/nn_vi.c` + `nx2/oot_patches.py` (both step divides in gen) |
| #1672 | Flush texture staging before GPU read | `vk_texture.cpp` / `vk_vma.cpp` / `vk_internal.h` |
| #1451 | IMNMX wide immediate → R251 | `lower_to_maxwell.h` |

## Not applicable on this tree (skipped)
| PR | Why |
|----|-----|
| #1648 | No NGX_SDK job deps path in our `nx2/build.py` (Streamline/stub DLSS) |
| #1517 | No `PreparedFor` / FILETIME seed-stamp path in our `vk_draw.cpp` prepare |

## Deploy
Smoke `mastersword.exe` + `mastersword_vk.dll` (golden `bak_4x50` untouched). Loader still prefers `mastersword_vk.dll`.
