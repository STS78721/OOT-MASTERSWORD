"""Quick pipelines (gpu/vk/quick_pipe.h, vk_draw.cpp quick_pipeline), MasterSword's port of Longshot ae96bad3; no game/GPU.

- quick_pipe.h's take() with fake handles: off at a budget of 0, one build an entry, the frame's first build always made
  and later ones held to the budget, a draw that must be drawn not held to it, a quick one that loses to the full build
  destroyed;
- vk_draw.cpp: off unless RX_VK_QUICK_MS is set, quick builds without the optimizer and into no pipeline cache, the
  worker's full build kept over a quick one, RX_VK_PIPE_SKIP=0 still waits for every full pipeline.
python -I -B tests/test_quick_pipelines_ms.py
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <atomic>
#include <cstdint>
#include <cstdio>
#include "quick_pipe.h"
static int failures = 0;
static void check(bool ok, const char* what) { if (!ok) std::printf("FAIL: %s\n", what), failures++; }
struct Entry { std::atomic<bool> ready{false}; std::atomic<uintptr_t> pipeline{0}; std::atomic<uint8_t> quick{0}; };
int main() {
    double clock = 0;
    auto now = [&] { return clock; };
    auto poll = [&] { clock += 1; };
    int built = 0, destroyed = 0;
    auto ask = [](bool& busy) -> uintptr_t { busy = false; return 0; };
    auto build = [&](bool& busy) -> uintptr_t { busy = false; built++; clock += 5; return (uintptr_t)(100 + built); };
    auto destroy = [&](uintptr_t) { destroyed++; };
    {   // off
        Entry e; quick_pipe::FrameBudget b;
        check(quick_pipe::take(e, false, 0, b, 1, ask, build, destroy, now, poll) == quick_pipe::Got::kNothing, "budget 0 is off");
        check(built == 0, "nothing built when off");
    }
    {   // one build an entry; the frame's first always, later ones by budget
        quick_pipe::FrameBudget b;
        Entry a, c, d;
        check(quick_pipe::take(a, false, 8, b, 1, ask, build, destroy, now, poll) == quick_pipe::Got::kQuick, "first build made");
        check(a.pipeline.load() != 0 && !a.ready.load(), "the entry draws with it, not ready");
        check(quick_pipe::take(a, false, 8, b, 1, ask, build, destroy, now, poll) == quick_pipe::Got::kOther, "an entry is built once");
        check(built == 1, "one build for one entry");
        check(quick_pipe::take(c, false, 8, b, 1, ask, build, destroy, now, poll) == quick_pipe::Got::kNothing,
              "a second 5 ms build does not fit the 3 ms left");
        check(quick_pipe::take(d, true, 8, b, 1, ask, build, destroy, now, poll) == quick_pipe::Got::kQuick,
              "a draw that must be drawn is not held to the budget");
        check(quick_pipe::take(c, false, 8, b, 2, ask, build, destroy, now, poll) == quick_pipe::Got::kQuick,
              "the next frame has its budget again");
    }
    {   // the full build came first: the quick one is destroyed
        quick_pipe::FrameBudget b;
        Entry e;
        auto late = [&](bool& busy) -> uintptr_t { busy = false; e.pipeline.store(7); return 9; };
        const int before = destroyed;
        check(quick_pipe::take(e, false, 8, b, 1, ask, late, destroy, now, poll) == quick_pipe::Got::kOther, "the full one won");
        check(destroyed == before + 1 && e.pipeline.load() == 7, "the quick one that lost is destroyed");
    }
    std::printf(failures ? "quick pipeline rules: %d failed\n" : "quick pipeline rules passed\n", failures);
    return failures != 0;
}
'''


def require(text, needle, why):
    if needle not in text:
        raise SystemExit(f'FAIL: {why} (missing: {needle})')


def main():
    draw = (ROOT / 'gpu/vk/vk_draw.cpp').read_text(encoding='utf-8')
    require(draw, 'const double v = quick_pipe::quick_ms(e, &bad);', 'RX_VK_QUICK_MS read by quick_pipe.h (0 unless set)')
    require(draw, 'ci.flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;\n    }\n    if (slow_full_build',
            'a quick build is made without the optimizer')
    require(draw, 'part >= 0 || driver_cache_only || quick_build ? VK_NULL_HANDLE : pcache',
            'a quick build goes into no pipeline cache')
    require(draw, 'pcache_built_since_save += !driver_cache_only && !quick_build;', 'a quick build does not count toward a save')
    require(draw, '} else if (job->e->pipeline.load(std::memory_order_acquire)) {',
            'a failed full build leaves the quick one')
    require(draw, '(always_wait || !quick_pipeline(*pe, p, s, at, topo, !skippable))',
            'RX_VK_PIPE_SKIP=0 waits for every full pipeline, no quick one')
    cc = shutil.which('g++') or shutil.which('clang++')
    if not cc:
        print('skip: no C++ compiler for the rules fixture')
        return
    with tempfile.TemporaryDirectory() as d:
        src, exe = Path(d) / 'fixture.cpp', Path(d) / 'fixture.exe'
        src.write_text(FIXTURE, encoding='utf-8')
        subprocess.run([cc, '-std=c++20', '-Wall', '-Werror', '-I', str(ROOT / 'gpu/vk'), str(src), '-o', str(exe)], check=True)
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.returncode:
            raise SystemExit(1)
    print('quick pipeline source checks passed')


if __name__ == '__main__':
    main()
