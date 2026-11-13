# NVIDIA Streamline (DLSS SR)

Headers under `include/` are the public MIT Streamline API (vendored from NVIDIA-RTX/Streamline).

Runtime DLLs are **not** shipped here. Copy beside `hookshot_vk.dll` / the game exe:

- `sl.interposer.dll`, `sl.common.dll`, `sl.dlss.dll`
- `nvngx_dlss.dll`

A convenient local set lives under Desktop `Apps\nvidiaDlssGlom\`.

Env: `RX_VK_DLSS=0` disables; `RX_VK_DLSS_VERBOSE=1` raises Streamline log level.
