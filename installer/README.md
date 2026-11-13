# OOT-MasterSword Setup

Windows installer UI modeled on [dkb-BANANAS](https://github.com/STS78721/dkb-BANANAS) and
[totk-pc-recomp-SteelTempleBuild](https://github.com/STS78721/totk-pc-recomp-SteelTempleBuild).

Deploys **MasterSword** play binaries next to the player’s own dump (no game files ship in the package).

## Run from source

```
pip install customtkinter pillow
python -m installer.modern_gui
```

## Build Setup.exe

Put a `payload/` folder (or set paths inside `modern_gui.py`) containing at least:

- `mastersword.exe`
- `mastersword_vk.dll`
- runtime DLLs (SDL2, FFmpeg, …)
- `MasterSwordLauncher.exe` (optional; built from `launcher/`)

Then:

```
pyinstaller OOT-MasterSword-Setup.spec
```

## Brand

| Item | Name |
|------|------|
| Setup | `OOT-MasterSword-Setup.exe` |
| Game | `mastersword.exe` |
| Vulkan | `mastersword_vk.dll` |
| Options | `mastersword_options.ini` |
| Launcher | `MasterSwordLauncher.exe` |
