# SteelTempSaint's MasterSword Launcher

Sony/Nixxes-style launcher for **The Legend of Zelda: Ocarina of Time - PC Recompiled**
(**SteelTemple MasterSword Edition**), matching
[totk-pc-recomp-SteelTempleBuild](https://github.com/STS78721/totk-pc-recomp-SteelTempleBuild) and
[dkb-BANANAS](https://github.com/STS78721/dkb-BANANAS).

## Run from source

```
pip install -r requirements.txt
python MasterSwordLauncher.py
```

## Build

```
pyinstaller MasterSwordLauncher.spec
```

Looks for `mastersword.exe` beside the launcher or in a remembered game folder.
