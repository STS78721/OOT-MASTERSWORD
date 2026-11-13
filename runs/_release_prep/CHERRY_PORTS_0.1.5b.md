# Cherry ports for MasterSword 0.1.5b

Surgical A+B catch-up on top of 0.1.5. No blind merge. No `vk_menu` / `vk_fsr3` / present-ahead / FSR4.

## Taken

### A
| PR | Notes |
|----|--------|
| #1645 | RT exclusive marks (a64-kit) |
| #1646 | Loader RELA addend |
| #1806 #1800 #1778 #1810 | Ampere lowers (R2UR / immediate / LDG / END) |
| #1513 | Mesh fallback vertex defaults |
| #1789 | ISBERD output |
| #1627 | TXQ array type |
| #1688 | Image-view retry + `cached_image_view` (hand-merged onto MS `make_view`) |
| #1706 | Texel-buffer ownership |
| #1731 | Mesh shader query feature |
| #1766 | Win driver-fault log |
| #1763 | Framelog host clock (`host->now_ns`) |

### B
| PR | Notes |
|----|--------|
| #1515 | Present device-lost → `device_lost()` (partial; adapted) |
| #1568 | `draw_flush_recording` at submit |
| #1776 | WB-diff shadow written tracking |
| #1660 #1765 #1772 | Pipeline-cache play stack (`create_cached` / play files); **skipped `vk_fsr3.cpp`** |
| #1747 | AMD cloth key + proprietary no-opt + `task_mesh_fallback` / shader bits; **skipped mesh allocator / emit_spirv_mesh** |

## Skipped / incomplete
| PR | Why |
|----|-----|
| #1625 menu half | `vk_menu` discipline; draw `menu_env_on` context drifted — not forced |
| #1754 | Sampler-admission hunks rejected on drifted sampler table — deferred |
| Present-ahead / FSR4 / Anti-Lag / ultrawide / capture / menu redesign | Still out |

## Verify
Smoke: `0.1.5b` stamps, ABI host 10 / DLL 11, DLSS + FG 4x.
