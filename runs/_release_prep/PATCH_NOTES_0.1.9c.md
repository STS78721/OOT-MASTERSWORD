[Updated on November 13, 2026]

MasterSword 0.1.9c

The game's own frames are far steadier. The game now runs on the CPU the way it was made to run on the Switch 2, and many graphical glitches are gone with it. Small hitches can still occur.

■ Major Updates

- Added the Intel Hybrid CPU Patch to the Graphics tab of the in-game menu (Insert or F1). It is On by default and recommended for Intel hybrid processors: 12th, 13th, and 14th gen Core and Core Ultra. The Switch 2 gives a game six identical cores. With the patch on, the game's threads run on your performance cores (P-cores), and background work such as shader building runs on your efficiency cores (E-cores). On a Core Ultra 7 270K Plus this made frame pacing much steadier and removed most of the graphical glitches that came from game threads running on the slower cores. The change applies after a restart.
- Parallel replay is now on by default. The renderer records each frame on several threads at once. The number of threads is chosen from your CPU's performance cores: 8 performance cores use 4 threads, 6 to 7 use 2 to 3, and fewer than 6 keep it off.
- The renderer now records each frame on a CPU core kept free of the game's threads, then returns. The game's threads no longer share a core with the renderer while it works.

■ New & Improvements

○ Performance

- Improved frame time in busy places such as Castle Town and Hyrule Field. A frame's recording work no longer runs on a single thread.
- Improved graphics memory use. A reserve of up to 15% of the card that sat mostly empty is no longer held, about 1.6 GB on a 16 GB card at 4K. Textures have more room, and fewer are cleared and loaded again.
- Improved texture loading. Fully opaque textures are made in their final, smaller format from the start, and checking them no longer reads from slow graphics memory. Texture loads that took 16 to 28 ms now take a fraction of that.
- Improved texture memory upkeep so it waits until a new area has finished loading before it rearranges memory.
- Improved how render targets are reused while graphics memory is nearly full. A small set of recent ones is kept instead of none, so they are not made again every frame.
- Improved the check of very large shadow textures when an area loads. It now runs on several threads.
- Improved how a draw that needs a new pipeline gets it. The draw builds its own pipeline at once instead of waiting behind every other build in the queue.
- Changed spare capture memory to a 128 MB limit, which leaves more system memory for the game.

○ Launcher

- Added a one-time guide on the first PLAY that explains how to tell Windows that MasterSword is a game (Game Bar: Remember this is a game). Windows gives a recognized game its game scheduling. The launcher does not change any Windows setting itself.

○ For Modders and Testers

- Added a detailed per-draw timing report for performance testing (RX_VK_PROF_FINE=1 in hookshot_env.txt).
- Added the CPU Lab to the source on GitHub (tools/cpulab). It lets you try your own core placement rules per game thread. It is not part of the download.

■ Bug Fixes

- Fixed the issue where parallel replay could leave houses, rocks, and bushes missing, tear geometry, or make lighting too dark.
- Fixed the issue where a draw waiting for its pipeline could stall the game for a long time when entering a new area.
- Fixed the issue where a game thread on a slower efficiency core could make the next frames late or show a wrong frame.
- Fixed the issue where a buffer check could fail on a very large buffer size.

■ Other

- To turn the core placement off, set Intel Hybrid CPU Patch to Off in the Graphics tab and restart the game.
- To turn other parts off, add a line to hookshot_env.txt beside mastersword.exe:
  - RX_VK_PARALLEL=0 turns parallel replay off.
  - HOS_GAME_P_CORES=0 keeps only the game's frame threads on performance cores.
  - RX_VK_REPLAY_PIN=0 lets the renderer record on any core.
- On a processor with one kind of core (most AMD Ryzen and older Intel processors), the Intel Hybrid CPU Patch does nothing, and parallel replay uses the number of cores.
- Small hitches can still occur, most often while standing in busy places. This is being investigated.
- Tip: in Windows Game Bar (Win + G > Settings > General, with the game's window in focus), tick Remember this is a game. It made play noticeably smoother in testing.
- A fresh install remains 1920×1080, windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, and stick speed 100%. V-Sync is Mailbox.
- An update does not replace mastersword_options.ini, saves, or your dump.

■ Update

- Close the game, then open MasterSwordLauncher.exe. It detects 0.1.9c and installs SteelTemple-MasterSword-0.1.9c-Update.zip.
- You can also extract that zip over the game folder.
- A new install uses SteelTemple-MasterSword-0.1.9c-Portable-StandAlone.zip. Place your own image.bin and romfs folder beside mastersword.exe.
- MasterSwordLauncher.exe alone refreshes the launcher.
- The startup warmer and the Insert menu show 0.1.9c.

Your dump, saves, and personal settings stay on your PC.
