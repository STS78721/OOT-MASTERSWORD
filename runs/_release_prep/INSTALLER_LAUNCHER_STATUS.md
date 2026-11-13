# Installer + launcher status (OOT-MasterSword)

## Done
- `launcher/MasterSwordLauncher.py` — BANANAS launcher rebranded (GitHub, exe, ini, art, palette)
- `launcher/assets/` — `mastersword.ico`, PNG, background (jpg/png), click/hover wav, showcase
- `installer/modern_gui.py` — BANANAS setup GUI rebranded (Setup title, paths, exe names)
- Shared installer modules copied from BANANAS (`events`, `runner`, `steps` helpers, `extract_nup`, …)
- `VERSION` = `0.1.0`
- PyInstaller specs: `OOT-MasterSword-Setup.spec`, `launcher/MasterSwordLauncher.spec`
- Runtime brand macros in `host/hos.h`; window title **OOT-MasterSword**
- Build outputs: `mastersword.exe` / `mastersword_vk.dll` (override with `HOOKSHOT_LEGACY_NAMES=1`)
- Options/log/version prefer MasterSword names; DLL/options still fall back to Hookshot names

## Next (before first public zip)
1. Finish GUI dump flow for Hookshot layout (`.nup` / `image.bin`+`romfs`) — still has some Bananza/TotK step wording in deep install paths
2. Wire `payload/` packing script from `work/sa/build` → Setup datas
3. Full rebuild + rename install to Test folder as MasterSword smoke
4. `pyinstaller` build Setup + Launcher on this machine
5. Push to `STS78721/OOT-MASTERSWORD` when you ask (never to NotRexed)

## Art
- Icon + sword-in-stone forest background generated and placed under `launcher/assets/` and `assets/mastersword.*`
