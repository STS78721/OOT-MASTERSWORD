# MasterSword — research: putting idle hardware to work in a CPU-bound game

*2026-11-14. Research and plan only; nothing here is built yet. Every idea is judged against MasterSword's own measurements,
and each one would land as a Forge with an off switch, followed by its Temper.*

## 1. The situation, measured

A heavy area (Town / Market, ~3,000 draws) at ~47-48 fps, a frame of ~21 ms:

| Who | Busy per frame | Idle |
|---|---|---|
| The game's own threads (6 virtual cores on P-cores) | ~12 ms, ~4.5 ms of it waiting for the frame's GPU work | — |
| Renderer recording (replay thread + 4-6 workers, P-cores) | ~7.4-8.5 ms wall, ~15 ms of work across threads | — |
| **GPU (RTX 5070 Ti)** | **~5.5-6 ms** | **~70%** |
| **Tensor cores** | DLSS SR + frame generation, a few ms | most of the frame |
| **NPU (Intel AI Boost)** | nothing | **100%** |
| **E-cores (16)** | shader/pipeline builds, imports, saves: bursts | most of the frame |

The bottleneck is CPU work on the P-cores: recording ~3,000 draws into Vulkan (~2.8 us of command recording per draw plus
descriptor work) and the game's frame, which waits on that recording's GPU work. **The cores that are idle can't run that
work as it is written today**: an NPU runs neural networks, Tensor cores run matrix math, and neither can run Vulkan driver
calls or recompiled game code. So the question is which CPU work can be *reshaped* into work they can do.

## 2. What this hardware exposes (vulkaninfo, this PC)

`VK_EXT_device_generated_commands` (+ `deviceGeneratedCommandsMultiDrawIndirectCount`), `VK_NV_device_generated_commands`
(+ compute), `VK_EXT_descriptor_buffer` (+ `descriptorBufferPushDescriptors`), `VK_KHR_cooperative_matrix`,
`VK_NV_cooperative_matrix2`, `VK_EXT_multi_draw`, `multiDrawIndirect`, `VK_EXT_mesh_shader`, `VK_KHR_maintenance6`.
NPU: "Intel(R) AI Boost", status OK.

## 3. The ideas, ranked by what they could take off the critical path

| Rank | Name (MasterSword) | Idea | Hardware | Takes off | Effort | Risk |
|---|---|---|---|---|---|---|
| **1** | **Din's Fire** — GPU-driven submission | The GPU builds the draw stream: the CPU writes compact per-draw records (pipeline, descriptor indices, vertex/index ranges) into a buffer, and `VK_EXT_device_generated_commands` turns them into real draws on the GPU. Consecutive draws sharing a pipeline become one indirect call | The idle **GPU** (70%) | A large part of the ~2.8 us/draw command recording | Large: the replay's draw path changes shape | Medium: correctness of every state the game sets; needs bindless first |
| **2** | **Bindless descriptors** (Din's Fire's foundation) | `VK_EXT_descriptor_buffer`: descriptors become plain memory the CPU writes (memcpy) or the GPU indexes, instead of `vkCmdPushDescriptorSet` per draw (~1.5 pushes and ~7.6 descriptor writes per draw measured) | GPU + CPU memory | The descriptor share of recording (~3.7 ms of the ~15 ms of draw work) | Medium | Low-medium; can be A/B'd alone |
| 3 | **E-core recording helpers** | Light command buffers (the short ones now recorded on the replay thread) recorded on E-cores in parallel with the heavy one | **E-cores** (idle) | Part of the replay thread's serial time | Small-medium | E-cores are ~1.5-2x slower per thread; only worth it for work that would otherwise wait |
| 4 | **Nayru's Love** — NPU frame-pacing predictor | A tiny model on the NPU predicts the next frame's cost from the Sheikah Sensor's signals (area picture, draws, streaming bursts) and steers the pacing: when frame generation inserts frames, when background work (census, Slate saves, imports) is allowed to run, when Hero's Shade may trial | **NPU** | Not base fps directly: smoother pacing, fewer collisions | Medium (OpenVINO / Windows ML) | Low (advisory only) |
| 5 | **Tensor-core helpers via cooperative matrix** | Matrix-heavy *GPU* work the renderer adds (actor-motion skinning for DLSS, texture re-encodes, upscaling filters) moved onto Tensor cores | **Tensor cores** | GPU time, which isn't the bottleneck | Medium | Low |
| 6 | **Neural texture compression** (NVIDIA RTX NTC style) | Textures stored compressed by a small network, decoded on the Tensor cores | Tensor cores | VRAM and streaming bandwidth, not CPU | Large | **Fidelity risk** (golden rule): only if visually lossless |

**Not viable:** running the recompiled game's logic on the GPU or NPU (it's sequential branchy CPU code); running Vulkan
recording on the NPU (it can't make driver calls); a shader-delivery ML model (delivery is already near zero, see the
playbook's ML section).

## 4. The honest expectations

- **The real prize is #1/#2, and it uses the GPU's idle compute, not the Tensor cores or the NPU.** It's the same
  philosophy as SR and FG ("move work to the hardware that's idle"), applied to draw submission. Modern engines do this
  (GPU-driven rendering), and this PC's driver supports it. If Town's recording fell from ~8 ms to ~3-4 ms, the game's
  wait for its frame's GPU work would shrink with it: that is the path to 60 in Town.
- **The NPU's realistic role is advisory** (#4): it can't do the heavy lifting, but a cheap always-on predictor can make the
  existing systems smarter (Hero's Shade's noisy trials are a good first customer: predict whether a trial sample is
  representative).
- **The Tensor cores' realistic role** in a CPU-bound game is limited: they accelerate GPU math, and the GPU already has
  room. They matter more if Din's Fire moves enough work to the GPU that it becomes busier.

## 5. Recommended order (Forge & Temper)

1. **Measure (Temper):** split the replay's "commands" time by Vulkan call type (push descriptors, binds, dynamic state,
   draws) in a Town frame, to size #2 before building it.
2. **Forge: bindless descriptors (#2)**, behind a switch, A/B in Town. Low risk, measurable alone, and the foundation for #1.
3. **Forge: Din's Fire (#1)**, GPU-driven submission for the draw types that dominate Town (from the Sheikah Sensor's
   hottest programs), behind a switch, falling back to normal recording for anything unusual.
4. **Forge: Nayru's Love (#4)** once there are signals worth predicting (Hero's Shade's trials, pacing).
5. #3, #5, #6 only if measurements later point at them.

Before any of this: settle Hero's Shade (one run with `RX_HEROS_SHADE=0`; see the handover).

## 6. Round 2: what the "security camera" pattern teaches (engineering level)

**The rule that decides everything: latency vs. batching.** A camera's NPU works because it gets a steady stream of
same-shaped inputs and can answer late (a few ms). A dispatch to the NPU costs ~0.1-1 ms round trip; to the GPU, a
submit-and-wait costs ~0.05-0.2 ms plus a frame of pipelining. So offload fits work that is (a) batchable, (b) can be
answered a frame or more late, or (c) can run ahead speculatively. It never fits a game function the CPU waits on mid-frame.

The camera's real trick is **recognition**: noticing "this is the same thing I saw before" and skipping the work. That
applies to MasterSword directly, mostly without ML:

| Name | Idea | Hardware | Lifts base fps? | Notes |
|---|---|---|---|---|
| **Gossip Stones** — recognised command segments | Town's draw stream is nearly identical frame to frame. Fingerprint each parallel-replay segment (pipelines, descriptor contents, buffer ranges, dynamic state); when a segment's fingerprint matches last frame's, re-submit its already-recorded command buffer instead of recording it again. Only changing data (uniforms) is rewritten | CPU hashing; secondary command buffers (none used today); needs bindless/descriptor buffers so segments don't embed per-frame descriptors | **Yes, potentially the most**: a matched segment costs a hash instead of ~2.8 us/draw | Fingerprint must be cheaper than recording; guest addresses that move between frames break matches. Measure the match rate first (a counter in the Sheikah Sensor) |
| **Rito copy engine** (dedicated transfer queue) | Mirror syncs and imports use CPU memcpy / the graphics queue today (no transfer queue in the tree). The GPU's DMA copy engines are idle and run in parallel with rendering | GPU copy engines | Some, on streaming-heavy frames and transitions | Needs queue-ownership transfers and semaphores; A/B on returns to Field |
| **Deku Scrub SIMD** — the game's own matrix math | OoT computes limb/skeleton matrices and the matrix stack on the CPU. That is tensor-shaped work, but the right tensor unit at that latency is the CPU's own AVX2/AVX-VNNI. Profile the hottest guest functions; replace the matrix ones with vectorised host versions (the oot_patches.py site mechanism already exists) | P-core SIMD | **Yes**, on the game's ~12 ms side, which nothing else on this list touches | Bit-exact results required (game logic depends on them) |
| **Navi** — "Hey, listen!" next-area prediction | A small model on the NPU watches Link's position, heading and the current area picture and predicts the next transition a few seconds early; MasterSword warms shaders, imports and the Slate picture before the loading zone | NPU | No; it targets the transition dips | Exactly the camera pattern: a steady stream, late answers are fine, a wrong guess costs only some background work |
| **Stone of Agony** — hitch anomaly detection | An always-on classifier over the Sheikah Sensor's per-frame telemetry flags abnormal frames and labels the cause (shader build, import, save, lock wait, GPU), writing it to the Slate so the next session pre-empts it | NPU (or a CPU heuristic first) | Indirectly | Start with a rule-based version; use the NPU only if the rules miss |
| **Twinrova iGPU** | The CPU's integrated Intel GPU is idle; it could take async work that tolerates cross-adapter copies (Slate/pack compression, offline texture re-encodes) | iGPU | No | Low priority; cross-adapter copies are slow |

**Revised order:** measure Vulkan call types and the Gossip Stones match rate (both counters, one Temper run) →
bindless descriptors → Gossip Stones → Din's Fire for the segments that don't match → Deku Scrub SIMD (profile first) →
Rito copy engine → Navi / Stone of Agony.
