SteelTemple MasterSword — Client Diagnostic Pack
================================================

For the pink / washed 3D world (HUD still OK) on Output 1440p/4K.

WHAT TO DO (client)
-------------------
1. Copy this whole "client_diag" folder INTO your MasterSword game folder
   (the folder that already has mastersword.exe + image.bin + romfs\).

2. Double-click:  MasterSword-Diagnose.cmd

3. A window opens. Press Enter to:
   - back up and clear old shader caches (programs / pipelines)
   - start the game with extra diagnostic logging
   - keep logging until you close the game

4. Play 30–60 seconds in the pink area (castle courtyard is fine).
   Then quit the game normally (close the window).

5. The tool writes a zip on your Desktop named like:
      MasterSword-Diag-YYYYMMDD-HHMMSS.zip

6. Send that zip back (WhatsApp / Discord / email).


WHAT IS COLLECTED (no game dump, no saves by default)
-----------------------------------------------------
- mastersword_log.txt (and rotated .1–.4 if present)
- mastersword_options.ini / hookshot_options.ini
- mastersword_version.txt / VERSION if present
- short SYSTEM.txt (GPU name, Windows, file sizes)
- KEYLINES.txt (resolution / crop / scale / unbound buffers / crashes)


SAFE
----
Does not upload anything by itself.
Does not touch image.bin or romfs.
Shader caches are moved to a backup folder first, not permanently deleted.
