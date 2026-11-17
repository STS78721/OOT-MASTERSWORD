// (longshot #769) What the order's watch thread (vk_replay.cpp sync_watch) does with queues stopped at a wait for longer than
// hold_ms: let them be, hold them on, or make them go past their waits (drain).
//
// When the GPU took longer than hold_ms over work a game thread waits on (e.g. a load's uploads: ~100 ms), forcing the
// stopped queues past their event wait caused frames to draw before the data the game writes after its submit (Link dark for a frame).
#pragma once

namespace mkwvk::hold_gpu {
enum class Choice { Wait, Hold, Force };

constexpr Choice choose(bool on, bool gpu_wait, double held_ms, double since_gpu_wait_ms, double hold_ms, double most_ms) {
    if (held_ms <= hold_ms) return Choice::Wait;
    if (!on || held_ms >= most_ms) return Choice::Force;
    if (gpu_wait || since_gpu_wait_ms <= hold_ms) return Choice::Hold;
    return Choice::Force;
}
} // namespace mkwvk::hold_gpu
