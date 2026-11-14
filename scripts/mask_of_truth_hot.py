"""MasterSword Mask of Truth, hot-split check: which game functions call the out-of-line float helpers (a64_fpx_*), and
whether the hottest functions are in the hot split (work/sa/gen/code_hot_*.c, -O2 with the helpers inlined).

    python -I scripts/mask_of_truth_hot.py <HOS_SAMPLE_ALL samples file> [--write work/measured_heat.txt]   (from the repo root)

--write: the measured heat for Biggoron's Forge (nx2/make_hot_split.py): one line per game function, "f_<addr> <samples>",
the samples in which it was the innermost game function and the thread was running in the game's code (not in a wait's spin).
RX_MAPS=<folder>: the maps of the build the sample was taken with (default work/sa/build), e.g. work/sa/_pre_biggoron.
"""
import bisect, collections, glob, os, re, sys
sys.path.insert(0, "scripts")
import prof
addrs, names = prof.load_map(os.path.join(os.environ.get("RX_MAPS", "work/sa/build"), "mastersword.map"), 0x140000000)
hot = set()
for p in glob.glob("work/sa/gen/code_hot_*.c"):   # (definitions only: a piece also declares what it calls)
    hot.update(re.findall(r"^void (f_[0-9a-f]+)\(Ctx \*__restrict", open(p, encoding="utf-8", errors="replace").read(), re.M))
print("functions in the hot split:", len(hot))
cache = {}
def nm(fr):
    r = cache.get(fr)
    if r is None:
        mod, _, off = fr.rpartition("+")
        r = "?"
        if mod == "mastersword.exe":
            i = bisect.bisect_right(addrs, int(off, 16)) - 1
            r = names[i] if i >= 0 else "?"
        else:
            r = mod
        cache[fr] = r
    return r
write = sys.argv[sys.argv.index("--write") + 1] if "--write" in sys.argv else None
SPIN = ("sleep_away", "spin_until_set", "hos_vcore_yield")   # (a wait's own loop: no code to speed up)
heat = collections.Counter()
fpx_by = collections.Counter(); self_f = collections.Counter(); run = 0; fpx_total = 0
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    head, _, st = line.rstrip("\n").partition("|")
    th = head.partition(" ")[2]
    if th.startswith("mkw_vk") or not st: continue
    fr = st.split(";")
    leaf = nm(fr[0])
    if not (fr[0].startswith("mastersword.exe")): continue
    run += 1
    if write and not leaf.startswith(SPIN):
        inner = next((n for n in map(nm, fr) if n.startswith("f_")), None)
        if inner:
            heat[inner] += 1
    caller = next((nm(x) for x in fr[1:] if nm(x).startswith("f_")), None)
    if leaf.startswith("a64_fpx_") or leaf.startswith("a64_vfpx_"):
        fpx_total += 1
        if caller: fpx_by[caller] += 1
    elif leaf.startswith("f_"):
        self_f[leaf] += 1
print("game-side samples running in the exe: %d; in outlined float helpers: %d (%.1f%%)" % (run, fpx_total, 100.0 * fpx_total / run))
print("-- callers of the outlined float helpers (share of those samples), in the hot split?")
cum = 0
for f, c in fpx_by.most_common(25):
    cum += c
    print("  %5.1f%%  (cum %5.1f%%)  %s  %s" % (100.0 * c / fpx_total, 100.0 * cum / fpx_total, f, "HOT" if f in hot else "cold"))
print("-- hottest game functions by own code (share of game-side running samples), in the hot split?")
for f, c in self_f.most_common(20):
    print("  %5.1f%%  %s  %s" % (100.0 * c / run, f, "HOT" if f in hot else "cold"))
cold_share = sum(c for f, c in fpx_by.items() if f not in hot)
print("outlined-helper samples whose caller is NOT in the hot split: %.1f%%" % (100.0 * cold_share / max(1, fpx_total)))
if write:
    with open(write, "w", encoding="utf-8", newline="\n") as f:
        f.write("# MasterSword Mask of Truth measured heat (scripts/mask_of_truth_hot.py) from %s: function samples\n"
                % os.path.basename(sys.argv[1]))
        for fn, c in heat.most_common():
            f.write("%s %d\n" % (fn, c))
    print("wrote %d functions (%d samples) to %s" % (len(heat), sum(heat.values()), write))
