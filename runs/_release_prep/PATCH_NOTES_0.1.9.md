[Updated on November 12, 2026]

MasterSword 0.1.9

Frame generation now makes the multiplier you select. Mastersword Pacer is on the Graphics tab and stays off until you turn it on. A new install presents with Mailbox.

■ Major Updates

- Added Mastersword Pacer (Experimental) on the Graphics tab, under Frame pacing. It is Off by default. Turn it on to hold the game's own frames to an even beat. A frame more than 2 ms late starts a new beat, so the next frame is not held for a full extra interval.
- Changed Frame Generation so the selected multiplier is the one that is synthesized, including every step above 4x.
- Changed V-Sync to On, Mailbox, and Off. Mailbox is the default for a new install and for a settings file with no V-Sync line. A saved On or Off is kept.

■ New & Improvements

○ Frame Generation

- MasterSword: 2x, 3x, 4x, 6x, 8x.
- DLSS Multi Frame Generation: 2x, 3x, 4x, 6x.
- Nodix SMFG: 2x, 3x, 4x, 5x, 6x.
- Apex FG (Fast): 2x, 3x, 4x, 5x, 6x. The row is marked Recommended. Its motion search is tighter when that engine is selected.
- XeSS Multi Frame Generation: 2x, 3x, 4x.
- FSR 3.1 Frame Generation: 2x.
- 7x is not a step on any engine.
- The DLSS frame-generation model row is shown only when DLSS Multi Frame Generation is the selected engine.

○ Presentation

- On waits for the display refresh.
- Mailbox shows the latest finished frame on the next refresh, without tearing. If the driver has no mailbox mode, FIFO is used and the log says so.
- Off presents immediately and can tear.
- The launcher's presentation setting is written into the game as the same V-Sync choice. The launcher reports V 0.1.9.

○ Mastersword Pacer

- The wait happens before the next frame's work begins. The game's present is not delayed by a sleep.
- A frame that itself takes longer than 16.7 ms still presents late. The pacer removes the short frame that used to follow a late one.
- Off returns to the previous start-time cap. The change applies on the next present.

○ Controls

- Added Left stick speed and Right stick speed on the Controls tab. Each side runs from 10% to 500%. 100% matches 0.1.8.

○ Performance

- Improved a one-off texture upload while the world streams. A Deku Tree camera turn no longer imports a whole memory pool on the replay thread. The picture is unchanged.
- Improved program selection for graphics, mesh, and compute, so a texture-stable choice is kept across a uniform-only update.
- Parallel replay stays off unless RX_VK_PARALLEL is set.

■ Bug Fixes

- Fixed the issue where every Frame Generation setting above 4x still synthesized only 4 frames at 1440p and 4K.
- Fixed the issue where Apex FG (Fast) stayed at 4x at any resolution when a higher multiplier was selected.
- Fixed the issue where the launcher still displayed V 0.1.8 after the game had moved to 0.1.9.

■ Other

- A fresh install remains 1920×1080, windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, and stick speed 100%. V-Sync is Mailbox.
- An update does not replace mastersword_options.ini, saves, or your dump.
- Nodix SMFG and Apex FG follow the multipliers those projects publish. The frames in this build are drawn by MasterSword's motion synthesis. Apex Super Resolution is not part of this update.
- At 4K, some drivers provide six spare swapchain images. 6x can be shown ahead of the game frame. 8x is still generated, and a generated frame that misses its beat is skipped.

■ Update

- Close the game, then open MasterSwordLauncher.exe. It detects 0.1.9 and installs SteelTemple-MasterSword-0.1.9-Update.zip.
- You can also extract that zip over the game folder.
- A new install uses SteelTemple-MasterSword-0.1.9-Portable-StandAlone.zip. Place your own image.bin and romfs folder beside mastersword.exe.
- MasterSwordLauncher.exe alone refreshes the launcher.
- The startup warmer and the Insert menu show 0.1.9.

Your dump, saves, and personal settings stay on your PC.
