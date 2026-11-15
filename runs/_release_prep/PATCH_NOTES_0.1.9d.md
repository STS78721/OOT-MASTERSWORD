[Updated on November 15, 2026]

MasterSword 0.1.9d

Hyrule Castle Town runs faster, and the game now learns the places you visit. Busy areas gain about 4% in the game's own frame rate, shaders for an area you have seen before are ready before you arrive, and saving no longer pauses play. Nothing in the picture changes: every improvement keeps the game's output identical.

■ Major Updates

- Added Biggoron's Forge. The game's busiest code is now compiled from a measured profile of real play in Castle Town instead of an estimate. The 699 functions that take the most time run with full optimisation, and the arithmetic they use most is done directly instead of through a helper call. In a repeatable test in Castle Town's market, frame time improved by about 2%.
- Improved the renderer's per-frame memory snapshot. The game's thread used to finish its share of the copy and then wait for the slower efficiency cores to finish theirs. The work is now split into smaller pieces, so the faster core takes more of them. In the same test, frame time improved by about another 1.7%.
- Added the MasterSword Triforce Shader Delivery. The game's shader code is kept in a pack on your drive and handed to the graphics driver directly, frequently used shaders are kept ready in memory, and the shaders of an area are prepared shortly before you need them.
- Added the Sheikah Slate. The game remembers which shaders and graphics programs each area uses, keeps that memory between sessions, and prepares them ahead of time when you return to a place you have visited.
- Added the Owl Statue. Saving now writes to the drive on its own thread, so the game no longer pauses while a save is written.

■ New & Improvements

○ Performance

- Improved frame time in Castle Town's market. Together, Biggoron's Forge and the snapshot change make the market about 4% faster in our tests (45.1 to about 46.8 fps on an RTX 5070 Ti and a Core Ultra 7 270K Plus).
- Improved the game's frame rate after returning to an area. Base frame rate no longer drops after travelling Hyrule Field, Castle Town, then Hyrule Field again.
- Improved memory reads while a new area loads. A small read is served from a copy while its memory is still being prepared in the background, instead of waiting.
- Improved how system memory is kept for the game. A soft minimum of the game's memory stays resident so Windows does not page it out during play.
- Improved how memory imports check memory. They now read the game's own record of which memory is in use instead of asking Windows each time (Lens of Truth).
- Improved draw recording. A draw takes its thread's pass and descriptor writer once instead of at every use.

○ Graphics

- Improved occlusion. The game now gets the GPU's real visibility counts, so it skips drawing what is hidden behind walls and buildings.
- Improved clustered lights. Each light now samples its own shadow map.
- Improved actor motion for models with one or two bone influences and for rigid one-bone models. These models now produce correct motion for DLSS Frame Generation, which reduces ghost copies around moving characters.
- Redrew the in-game menu, the FPS display, and the start-up screen.
- Changed the start-up screen's logo. The checkerboard background and a cut-off line of text around the logo are gone.

○ For Modders and Testers

- Added quick pipelines (RX_VK_QUICK_MS=8 in hookshot_env.txt). A draw whose pipeline is still being built gets a simpler one right away instead of being skipped or waiting.
- Added BC7 for large ASTC textures (RX_VK_ASTC_BC7=1). It uses the same graphics memory as BC3, with smoother gradients.
- Added Hero's Shade (RX_HEROS_SHADE=1). The renderer tries different numbers of recording threads in each area and keeps the fastest. In our tests its effect was too small to measure, so it stays off by default.
- Added developer tools to the source on GitHub: Mask of Truth (a profile of the game's own CPU time), Sun's Song (which thread wakes which), and a repeatable headless benchmark. They are not part of the download.

■ Bug Fixes

- Fixed the issue where the in-game update check could offer an older release as a new version when your game folder was newer than the latest release.
- Fixed the issue where the Scarecrow's Song recording and playback timing was wrong above 60 fps.
- Fixed the issue where the Deku Tree's floor web moved too fast above 60 fps.
- Fixed the issue where turning on a diagnostic wait log could crash the game after a thread had ended.

■ Other

- The first start after updating takes longer while the shader pack and the Sheikah Slate are set up. Later starts are quicker.
- Each system can be turned off with a line in hookshot_env.txt beside mastersword.exe:
  - RX_TRIFORCE_SONG=0 turns off preparing an area's shaders ahead of time.
  - HOS_OWL_STATUE=0 writes saves on the game's own thread, as before.
  - RX_MEM_RESIDENT_MB=0 turns off the soft memory minimum.
- Small hitches can still occur, most often while entering a new area or in busy places. Antivirus software that scans often in the background can add to them.
- A fresh install remains 1920×1080, windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, and stick speed 100%. V-Sync is Mailbox.
- An update does not replace mastersword_options.ini, saves, or your dump.

■ Update

- Close the game, then open MasterSwordLauncher.exe. It detects 0.1.9d and installs SteelTemple-MasterSword-0.1.9d-Update.zip.
- You can also extract that zip over the game folder.
- A new install uses SteelTemple-MasterSword-0.1.9d-Portable-StandAlone.zip. Place your own image.bin and romfs folder beside mastersword.exe.
- MasterSwordLauncher.exe alone refreshes the launcher.
