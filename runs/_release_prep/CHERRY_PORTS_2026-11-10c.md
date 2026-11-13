# Upstream ports 2026-11-10c

Surgical into MasterSword tree; rebuilt + deployed to smoke (golden bak untouched).

## High
| PR | Status |
|----|--------|
| #1639 dry-pass clear marks | applied |
| #1014 texture handle banks | applied |
| #1447 SULD.P 1D | applied |
| #1714 draw owns queued translation | applied |
| #1505 BC3 upload retry | applied |

## Solid
| PR | Status |
|----|--------|
| #1514 staging arena fallback | applied (manual) |
| #81a6da5 64-bit layout + GPU size reject | applied (manual) |
| #1696 WaitAny no checkpoint under mw_lock | applied |
| #1376 RX_VK_VRAM_MB / iGPU over preset | applied (manual; also added 2560 MB iGPU own cap) |
| #1509 mesh-fallback barrier before clear | applied (manual) |
| #1512 mesh-fallback async compile + precompile | applied (manual) |

## Skipped
| PR | Why |
|----|-----|
| #1520 DLSS keep through loads | `vk_dlss.cpp` is Streamline stubs here — no `dlss_idle` / feature release path |
