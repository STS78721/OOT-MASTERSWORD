[Updated on November 12, 2026]

MasterSword 0.1.9b

Moving from one area to the next is smoother. The graphics card keeps less of the last area's unused memory. A hitch can still happen inside town.

■ Major Updates

- Improved area traversal when the graphics card is nearly full. Cold textures are cleared before a large new texture is created, and the card is not asked to reshuffle textures while the new area is still streaming in.
- Improved memory copies so an old area's unused range is cut down to the part the new area still uses, after it has stayed unused for a few seconds.

■ New & Improvements

○ Performance

- Changed memory imports from 16 MB blocks to 4 MB blocks on a card that keeps a video-memory copy. Empty padding in those blocks no longer counts as used memory.
- Added a trim for an import that is mostly outside every live game pool. The live part is copied first. The picture in the new area is kept.
- Improved write-back helpers so copies that were given back under memory pressure are made again once there is room, up to eight at a time. The game no longer stays heavy for the rest of the session after one tight scene.

■ Bug Fixes

- Fixed the issue where entering a new area could freeze for a long moment while the graphics card was already full.
- Fixed the issue where helper copies dropped under memory pressure stayed gone, so the next areas stayed slower.
- Fixed the issue where a 16 MB memory copy kept unused padding that still counted against the card.

■ Other

- A hitch can still occur inside town. Traversal between areas is the part this update improves.
- Mastersword Pacer (Experimental) stays Off until you turn it on.
- V-Sync Mailbox remains the default for a new install. A saved On or Off is kept.
- A fresh install remains 1920×1080, windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, and stick speed 100%.
- An update does not replace mastersword_options.ini, saves, or your dump.

■ Update

- Close the game, then open MasterSwordLauncher.exe. It detects 0.1.9b and installs SteelTemple-MasterSword-0.1.9b-Update.zip.
- You can also extract that zip over the game folder.
- A new install uses SteelTemple-MasterSword-0.1.9b-Portable-StandAlone.zip. Place your own image.bin and romfs folder beside mastersword.exe.
- MasterSwordLauncher.exe alone refreshes the launcher.
- The startup warmer and the Insert menu show 0.1.9b.

Your dump, saves, and personal settings stay on your PC.
