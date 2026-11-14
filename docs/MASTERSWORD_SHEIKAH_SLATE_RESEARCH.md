# MasterSword Sheikah Slate — research: what it sees, what it should see

*2026-11-14. Research only: nothing here changes the current deployment. Each proposal says how it would be added without
breaking what works, under the Forge & Temper cycle.*

## 1. What the Slate sees today

The Slate (`gpu/vk/vk_mempack.cpp`, file `mastersword_slate.bin`) learns from two signals:

| Signal | Where it comes from | What it feeds |
|---|---|---|
| **Shader uses** | `stage_bind`: a hot-cache hit or a module made, i.e. a **pipeline build** | Scores (decayed across sessions), area pictures, Song of Time's start-up replay |
| **Area streaming** | `slate_frame`: more than 8 textures made in one frame | Starts an area picture (the shaders first used in the next 15 s) |

## 2. What the runs show (10:52, 11:08, 11:20)

- **The Slate learns from start-up, not from play.** The start-up preparation builds nearly every pipeline the game will
  use (25,269 graphics + 1,374 compute in about 5 s), so almost all shader uses happen before the player moves. In play,
  entering an area triggers few or no new builds.
- **So area pictures never form:** 0 taken in 12 area streams. A picture needs 32 new shaders after a stream, and there
  aren't that many in play anymore. That's good news for stutter (in-play shader stalls are mostly gone), but it means the
  Slate's "eyes" point at the wrong moment.
- **Where start-up time really goes:** 5.0 s total; **3.7 s is opening and reading the caches** (`mkw_vk_pipelines.bin`
  1.4 GB, `hookshot_pipelines.bin` 654 MB, the notes, the program records); ~1.3 s is building. On a cold boot, with the
  file cache empty, the reading part grows.
- Disk footprint for reference: `hookshot_programs.bin` 7.4 GB (translated programs, read on demand),
  `mastersword_spirv.pack` 3.6 GB (the Triforce pack). The pack's SPIR-V duplicates part of the program records.

## 3. What it could see — candidate senses

Ranked by expected payoff. "Observe" means counters and log lines only; "Act" means behaviour changes.

| # | Sense (MasterSword name) | Signal | Cost to observe | What acting on it would give | Risk |
|---|---|---|---|---|---|
| **A** | **Sheikah Sensor: what play actually draws** | An atomic use counter per pipeline entry, bumped in `draw()` the first time each frame (or every Nth use), plus per-program dispatch counts | One relaxed atomic increment per draw, ~nothing | The Slate knows **which of the ~26,600 pipelines the player really uses**, instead of everything the start-up built. Enables **Farore's Wind** (below) | None while observing |
| **B** | **Farore's Wind: start where you left off** | A's counts, saved in the Slate | — | Start-up builds the Slate's played set first, lets the game start, and builds the rest on the E-cores in the background. Targets the 1.3 s build part, and lets the 3.7 s cache reading overlap with play | Medium: a pipeline built late means its draws wait or are skipped once (the existing pending-skip path). Gate it behind a switch, A/B it |
| **C** | **Sheikah Sensor, area signatures from draws** | After an area stream, the **program keys** drawn (stable across runs, unlike memory addresses) | A small set per burst | Area pictures that work with precompiled pipelines: the Slate can recognise areas and their transitions. What to *do* with that is open: texture-upload priorities, import sizing, or none | Low while observing. Acting needs a proven per-area action first |
| **D** | **Owl Statue awareness** | Save events from the Owl Statue (host -> renderer through the existing log or an exported flag) | ~nothing | The Slate's save, the census and other background work wait until a save's flush is done: no I/O pile-ups at save moments | Low |
| **E** | **Driver and hardware fingerprint** | GPU, driver version, pack and cache stamps, saved in the Slate | ~nothing | The Slate knows *why* a start was slow (driver update -> caches invalid), says so in the log, and keeps its learned state per driver instead of mixing two | Low |
| **F** | **Import learning per area** | Count and size of imports after each area stream (from C's signatures) | ~nothing | Data for the Lens of Truth's remaining half (driver pinning, 8-22 ms per import): which areas cause the most, and whether pinning larger ranges at quiet moments would help | Background pinning *during play* failed once (see handover); only observe until a safe action is designed |

**Not worth it (the honest verdicts):**
- **Warming the start-up cache files ahead (a Courage for them):** they're read immediately at start anyway, so there's
  nothing to get ahead of. The win is in *order* (Farore's Wind), not in pre-reading.
- **Recognising areas by memory addresses:** pool addresses change between runs. Program keys (hashes of shader code) don't.
- **Deduplicating the 7.4 GB program records against the 3.6 GB pack:** saves disk, not time or RAM. Not now.

## 4. How to add senses without breaking the deployment

1. **Observe before acting.** Each new sense lands first as counters and log lines only, behaviour unchanged. An action
   is forged only once a run's data shows it pays.
2. **Versioned Slate file.** Today's format is `SLT1`. New sections go in `SLT2` as optional blocks with their own
   lengths; the loader keeps reading `SLT1`, and an unknown block is skipped. An old Slate is never thrown away because of
   a new sense.
3. **Every action has an off switch** (`RX_SLATE_*`), default **off** until A/B-tested with the `AB_test` kit, then on.
4. **Hot-path cost budget:** a sense may add at most an atomic increment per draw, and no locks in `draw()`. Anything
   heavier runs on a worker or at save time.
5. **Each Forge gets its Temper.** After a sense or an action lands, the next pass measures its cost and accuracy.

## 5. Recommended order

**Status:** step 1 and the observing half of step 2 are done in Temper 2 (commit ee15b931): the Slate learns from draws,
pictures are keyed by program keys, SLT2 with the fingerprint, Owl Statue awareness. Waiting on play data.


1. **Temper 2 (Slate):** score only **in-play** uses (after the start-up preparation ends), so the scores mean "what you
   play", not "what start-up built". Add sense **E** (fingerprint) and **D** (Owl awareness): small and safe.
2. **Forge: Sheikah Sensor (A + C, observe only):** per-pipeline draw counts and program-key area signatures, saved in
   `SLT2`. One or two play sessions of data.
3. **Forge: Farore's Wind (B), switch default off:** a prioritised start-up from the Sensor's data, A/B against the normal
   start on a warm *and* a cold boot. On by default only if both are faster and no draws go missing.
4. Revisit **F** with the Sensor's per-area import data.
