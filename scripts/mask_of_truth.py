"""MasterSword Mask of Truth: where the game's own CPU time goes, read from an all-thread sample.

The Mask of Truth lets Link read what the Gossip Stones know. This reads what the game's threads were doing: the host's
sampler (host/mkw_glue.c, HOS_SAMPLE_ALL=<file>) writes, every 2 ms while <file>.go exists, one line per thread,
"<ms> <thread>|<frames>", the frames module+offset, leaf first (16 at most). Frames in mastersword.exe and
mastersword_vk.dll are named from the linker maps of the same build (work/sa/build, or RX_MAPS=<folder>); a game function
is f_<its guest address>, the name nx2/oot_patches.py uses.

    python -I scripts/mask_of_truth.py <samples file> [--top N] [--from MS] [--to MS] [--thread NAME]

Prints: each thread's running share; for the frame's two serial threads (the game's main thread and the Presentation
Thread, which records the frame) and for every game thread together: what kind of code the running time is in, the game
functions by inclusive and by self share, the SDK services called, the leaves; what a serial thread waits in when it is not
running; and what runs while the main thread waits (the frame's critical path).
"""
import argparse
import bisect
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prof  # noqa: E402  (load_map, demangle, BUILD)

EXE_MODS = ("mastersword.exe", "hookshot.exe")
VK_MODS = ("mastersword_vk.dll", "hookshot_vk.dll")
SERIAL = ("sead::MainThread", "Presentation_Thread")   # (host/hos_thread.c: the frame waits on each)
CRT = ("memcpy", "memmove", "memset", "memcmp", "strlen", "strcmp", "strncmp", "malloc", "free", "calloc", "realloc",
       "_alloca", "__chkstk", "sqrt", "sin", "cos", "tan", "atan", "pow", "exp", "log", "floor", "ceil", "fmod")

KINDS = ("game code (f_*)", "recompiler helpers (a64_*, recomp_*)", "SDK services (imp_*)", "host runtime (hos_*, nn_*)",
         "C runtime", "host, other", "renderer (mastersword_vk)", "GPU driver", "Windows (waits, heap, kernel)", "other modules")


def sdk_name(s):
    """_ZN2nn2os12CreateThreadE... -> nn::os::CreateThread (the nested name of an Itanium-mangled SDK symbol)"""
    if not s.startswith("_ZN"):
        return s
    i, parts = 3, []
    if s[i:i + 1] == "K":
        i += 1
    while i < len(s) and s[i].isdigit():
        j = i
        while j < len(s) and s[j].isdigit():
            j += 1
        n = int(s[i:j])
        parts.append(s[j:j + n])
        i = j + n
    return "::".join(parts) if parts else s


def kind_of(mod, sym):
    """What kind of code a leaf is: (kind, running). `sym` is the bare symbol in an exe frame"""
    low = mod.lower()
    if mod in EXE_MODS:
        if sym.startswith("f_"):
            return KINDS[0], True
        if sym.startswith(("a64_", "recomp_")):
            return KINDS[1], True
        if sym.startswith("imp_"):
            return KINDS[2], True
        if sym.startswith(("hos_", "nn_", "nvn", "nv_", "HLE", "hle_")):
            return KINDS[3], True
        if sym.lstrip("_").startswith(CRT):
            return KINDS[4], True
        return KINDS[5], True
    if mod in VK_MODS:
        return KINDS[6], True
    if low.startswith(("nvoglv", "nvgpucomp", "nvppex", "nvwgf", "nvcuda", "nvngx", "amdvlk", "igvk", "vulkan-1", "sl.", "nvlddmkm")):
        return KINDS[7], True
    if low in ("ntdll.dll", "kernelbase.dll", "kernel32.dll", "win32u.dll", "user32.dll", "ucrtbase.dll", "?"):
        return KINDS[8], False
    return KINDS[9], False


def main():
    ap = argparse.ArgumentParser(description="MasterSword Mask of Truth: the game's CPU time from an all-thread sample")
    ap.add_argument("path")
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--from", dest="t0", type=int, default=None, help="only samples from this ms on")
    ap.add_argument("--to", dest="t1", type=int, default=None, help="only samples before this ms")
    ap.add_argument("--thread", default=None, help="one more thread to break down (as the file names it)")
    a = ap.parse_args()

    maps = {}
    build = os.environ.get("RX_MAPS", prof.BUILD)
    for mod in EXE_MODS + VK_MODS:
        mp = os.path.join(build, mod.rsplit(".", 1)[0] + ".map")
        if os.path.exists(mp):
            maps[mod] = prof.load_map(mp, 0x180000000 if mod.endswith(".dll") else 0x140000000)
    if not any(m in maps for m in EXE_MODS):
        print("no linker map for the game's exe in %s: its frames stay module+offset (set RX_MAPS to the build's folder)" % build)
    cache = {}

    def name(frame):
        """(module, symbol): the symbol demangled, or the offset where there is no map"""
        r = cache.get(frame)
        if r is None:
            mod, _, off = frame.rpartition("+")
            sym = "+" + off
            if mod in maps:
                addrs, names = maps[mod]
                try:
                    i = bisect.bisect_right(addrs, int(off, 16)) - 1
                except ValueError:
                    i = -1
                if i >= 0:
                    sym = prof.demangle(names[i])
            r = cache[frame] = (mod, sym)
        return r

    def show(fr):
        mod, sym = fr
        return sym if mod in EXE_MODS else ("vk:" + sym if mod in VK_MODS else mod + sym)

    # ---- read: thread -> [(ms, frames named)] ----
    by_thread = collections.defaultdict(list)
    ticks = collections.defaultdict(dict)   # ms -> thread -> index into by_thread[thread]
    lo, hi = None, None
    with open(a.path, encoding="utf-8", errors="replace") as f:
        for line in f:
            head, _, stack = line.rstrip("\n").partition("|")
            ms, _, thread = head.partition(" ")
            if not stack or not ms.isdigit():
                continue
            ms = int(ms)
            if (a.t0 is not None and ms < a.t0) or (a.t1 is not None and ms >= a.t1):
                continue
            frames = [name(x) for x in stack.split(";") if x]
            if not frames:
                continue
            lo = ms if lo is None else min(lo, ms)
            hi = ms if hi is None else max(hi, ms)
            ticks[ms][thread] = len(by_thread[thread])
            by_thread[thread].append(frames)
    if not by_thread:
        print("no samples in %s (is it the HOS_SAMPLE_ALL file, and was its .go file there while the game ran?)" % a.path)
        return 1
    secs = max(0.001, (hi - lo) / 1000.0)
    print("MasterSword Mask of Truth: %s" % a.path)
    print("%d instants over %.1f s (%.1f ms apart), %d threads" % (len(ticks), secs, 1000.0 * secs / max(1, len(ticks) - 1),
                                                                  len(by_thread)))

    def is_game(th):
        return not th.startswith("mkw_vk")

    def leaf_kind(frames):
        return kind_of(*frames[0])

    # ---- every thread's running share ----
    print("\n-- running share by thread (its leaf in the game, the renderer or the driver)")
    rows = []
    for th, samples in by_thread.items():
        run = sum(1 for fr in samples if leaf_kind(fr)[1])
        rows.append((run / len(samples), run * secs / len(samples), th, len(samples)))
    for share, cpu, th, n in sorted(rows, reverse=True)[:32]:
        if share < 0.005:
            break
        print("  %5.1f%%  %-34s (%d samples)" % (100.0 * share, th, n))
    busy_game = sum(share for share, _, th, _ in rows if is_game(th))
    busy_vk = sum(share for share, _, th, _ in rows if not is_game(th))
    print("  in all: the game's threads %.2f cores busy, the renderer's own threads %.2f" % (busy_game, busy_vk))

    def breakdown(title, samples):
        n = len(samples)
        if not n:
            return
        kinds = collections.Counter()
        incl, self_, svc, leaves, waits = (collections.Counter() for _ in range(5))
        running = 0
        for fr in samples:
            k, run = leaf_kind(fr)
            kinds[k] += 1
            if not run:
                # (blocked: the innermost game function and the SDK service it called)
                imp = next((s for m, s in fr if m in EXE_MODS and s.startswith("imp_")), None)
                fn = next((s for m, s in fr if m in EXE_MODS and s.startswith("f_")), None)
                host = next((show(x) for x in fr if x[0] in EXE_MODS + VK_MODS and not x[1].startswith(("f_", "imp_"))), None)
                where = sdk_name(imp[4:]) if imp else host
                waits[(where + " <- " + fn) if where and fn else (fn or where or show(fr[0]))] += 1
                continue
            running += 1
            leaves[show(fr[0])] += 1
            seen = set()
            for m, s in fr:
                if m in EXE_MODS and s.startswith("f_") and s not in seen:
                    seen.add(s)
                    incl[s] += 1
            first_f = next((s for m, s in fr if m in EXE_MODS and s.startswith("f_")), None)
            if first_f:
                self_[first_f] += 1   # (the innermost game function: its own code and the helpers and services under it)
            imp = next((s for m, s in fr if m in EXE_MODS and s.startswith("imp_")), None)
            if imp:
                svc[sdk_name(imp[4:])] += 1
        print("\n== %s: %d samples, running %.1f%% of them" % (title, n, 100.0 * running / n))
        print("-- what the leaf is (share of all samples)")
        for k in KINDS:
            if kinds[k]:
                print("  %5.1f%%  %s" % (100.0 * kinds[k] / n, k))
        r = max(1, running)
        print("-- game functions, inclusive (anywhere in the 16 frames; share of the running samples)")
        for k, v in incl.most_common(a.top):
            print("  %5.1f%%  %s" % (100.0 * v / r, k))
        print("-- game functions, innermost (its own code with the helpers and services it calls)")
        for k, v in self_.most_common(a.top):
            print("  %5.1f%%  %s" % (100.0 * v / r, k))
        print("-- leaves (the function the instruction pointer was in)")
        for k, v in leaves.most_common(a.top):
            print("  %5.1f%%  %s" % (100.0 * v / r, k))
        if svc:
            print("-- SDK services in the stack")
            for k, v in svc.most_common(12):
                print("  %5.1f%%  %s" % (100.0 * v / r, k))
        if waits:
            print("-- not running: where (share of the not-running samples)")
            w = max(1, n - running)
            for k, v in waits.most_common(10):
                print("  %5.1f%%  %s" % (100.0 * v / w, k))

    for th in SERIAL + ((a.thread,) if a.thread else ()):
        if th in by_thread:
            breakdown(th, by_thread[th])
        else:
            print("\n== %s: not in the sample (threads: %s)" % (th, ", ".join(sorted(by_thread)[:40])))
    breakdown("every game thread together", [fr for th, s in by_thread.items() if is_game(th) for fr in s])
    breakdown("the renderer's own threads together", [fr for th, s in by_thread.items() if not is_game(th) for fr in s])

    # ---- the critical path: what runs while the main thread waits ----
    main_th = SERIAL[0]
    if main_th in by_thread:
        blocked = [t for t in ticks.values() if main_th in t and not leaf_kind(by_thread[main_th][t[main_th]])[1]]
        n = len(ticks)
        print("\n== the critical path: the main thread is not running in %.1f%% of instants" % (100.0 * len(blocked) / max(1, n)))
        if blocked:
            who = collections.Counter()
            none = 0
            for t in blocked:
                any_run = False
                for th, i in t.items():
                    if th != main_th and leaf_kind(by_thread[th][i])[1]:
                        who[th] += 1
                        any_run = True
                none += not any_run
            print("-- threads running then (share of those instants)")
            for th, c in who.most_common(12):
                print("  %5.1f%%  %s" % (100.0 * c / len(blocked), th))
            print("  %5.1f%%  nothing sampled is running (the frame waits on the GPU, the display or a timer)" %
                  (100.0 * none / len(blocked)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
