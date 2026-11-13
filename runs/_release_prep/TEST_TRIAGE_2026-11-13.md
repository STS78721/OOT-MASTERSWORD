# Test triage — 2026-11-13 (0.1.9c)

Suite: `python -I -B tests/run_all.py`. Before: 144 passed / 110 failed / 32 skipped. After today's fixes: 145 / 109 / 32.

Method: every failing test was also run against a clean export of the last commit (`0a91cb74`, 2026-11-10).
A test that passes there and fails in this tree marks code that changed since; each was then read to tell a
**lost fix** (restore the code) from a **stale test** (the change was meant; update the test some day).

| Group | Count | Meaning |
|---|---|---|
| Fails at the last commit too | 10 | Environment: `zstandard` missing, compiler differences, tests that need `--headers`/`--cc` |
| Untracked tests (written by agents during 0.1.5–0.1.9) | 26 | Some describe fixes that were never kept in code |
| Changed since the last commit | 76 | Mostly menu/builder/launcher text checks the agents' rewrites left behind |

## Lost fixes restored today

- `test_jobs_wait_runs_queued` — **graphics pipeline build jobs had lost their ready flag** (`vk_draw.cpp`
  `graphics_pipeline`). A draw that had to wait for its pipeline waited for every job queued before it instead
  of building it at once (the code's own note: frames stopped 6–12 s in a new area). Restored.
- `test_finalized_pools_and_vma_prealloc` (part 2) — **the image pool preallocated 15% of VRAM** even though textures
  and render targets have pools of their own: 2304 MB held 641 MB on an RTX 5070 Ti. Now 0 when split
  (`RX_VK_IMAGE_PREALLOC=<n>` to force).
- Same test (part 3, partly) — spare capture buffers capped at 128 MB above 6 GB (were unbounded).

## Checked and judged stale (the change was meant; test not updated)

- `test_bank_gen` — texture banks of the task/mesh stages now keyed by their own `tex_gen` (bumped per stage in
  `vk_replay.cpp`), a deliberate widening of the bank cache.
- `test_framegen_gate` — gate unchanged in meaning; a comment pushed the text past the test's 400-character window.
- `test_mem_trim` — the ASTC scratch check is still there, as `if (!bc_scratch_layout(...)) { astc_fail = ...; return false; }`.
- `test_recorded_wait_flush` — its own message says "update this test".
- `test_target_image_pool`, `test_import_chunk_default` — updated today with the code.

## Still to look at (lower impact)

- `test_frame_wait_bounds` — `RX_VK_WAIT_GPU=<n>` with a huge n indexes past the ring (debug switch only).
- `test_mesh_args_cleanup` — a failed mesh-argument init keeps its buffer (rare error path).
- `test_pick_tex_gen` — program picks may come from somewhere `tex_gen` does not cover (0.1.9's pick change):
  watch for a wrong-shader flicker on compute/mesh stages.
- `test_finalized_pools_and_vma_prealloc` part 1 — `retire_finalized_pools()` (retire imports of pools the game
  finalized) was never implemented; upstream is gone, so it needs writing from scratch.
- `test_device_lost_exit`, `test_reflex_swapchain_lifetime`, `test_present_copy_wait`, `test_ub_threads`,
  `test_parallel_mirror_make` (parallel replay is off by default).
- The menu/builder/launcher group (~50): update the tests to the MasterSword menu, or drop the ones for removed rows.
