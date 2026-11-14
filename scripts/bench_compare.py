"""MasterSword bench comparison: the frame rate of scripted bench runs (tools/handover/mastersword_bench_ab.ps1) over the same
stretch of presents, grouped by what was run (the run folder's last part: <tag>_<n>_<what>).

    python -I scripts/bench_compare.py <bench_runs folder> <tag> [--from 4800] [--to 99999] [--min-draws 2500]

A window is one frame-statistics line (120 presents). Only windows from --from presents on (the route has arrived and Link
stands still) with at least --min-draws draws (the market square, not a load) count. Prints each run's mean frame time,
then each group's mean, the spread between its runs, and the change against the first group.
"""
import argparse
import os
import re
import statistics

LINE = re.compile(r"vk: (\d+) presents, ([0-9.]+) fps; a frame: (\d+) draws")


def run_stats(path, lo, hi, min_draws):
    ms, draws = [], []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            p, fps, d = int(m.group(1)), float(m.group(2)), int(m.group(3))
            if lo <= p < hi and d >= min_draws and fps > 0:
                ms.append(1000.0 / fps)
                draws.append(d)
    return ms, draws


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("runs")
    ap.add_argument("tag")
    ap.add_argument("--from", dest="lo", type=int, default=4800)
    ap.add_argument("--to", dest="hi", type=int, default=99999)
    ap.add_argument("--min-draws", type=int, default=2500)
    a = ap.parse_args()
    groups, windows, run_windows = {}, {}, {}
    order = []
    for name in sorted(os.listdir(a.runs), key=lambda n: (len(n), n)):
        parts = name.split("_")
        if not name.startswith(a.tag + "_") or len(parts) < 3:
            continue
        log = os.path.join(a.runs, name, "log.txt")
        if not os.path.exists(log):
            print("%-24s no log" % name)
            continue
        ms, draws = run_stats(log, a.lo, a.hi, a.min_draws)
        what = "_".join(parts[2:])
        if not ms:
            print("%-24s no windows in range" % name)
            continue
        mean = statistics.mean(ms)
        print("%-24s %3d windows, %5.0f draws, %6.2f ms a frame (%5.1f fps), median %6.2f" % (
            name, len(ms), statistics.mean(draws), mean, 1000.0 / mean, statistics.median(ms)))
        if what not in groups:
            groups[what] = []
            windows[what] = []
            order.append(what)
        groups[what].append(mean)
        windows[what] += list(zip(draws, ms))
        run_windows.setdefault(what, []).append(list(zip(draws, ms)))
    if not order:
        return
    # (medians: a stall of the PC, a scan or an update, moves one run's mean by seconds of frames; the median window does not)
    print()
    for what in order:
        meds = [statistics.median(t for _, t in ws) for ws in run_windows[what]]
        hitches = [sum(1 for _, t in ws if t > 2 * statistics.median(x for _, x in ws)) for ws in run_windows[what]]
        print("%-10s median window per run: %s ms -> %.2f ms (%.1f fps); windows over twice the median: %s" % (
            what, " ".join("%.2f" % m for m in meds), statistics.mean(meds), 1000.0 / statistics.mean(meds),
            " ".join(str(h) for h in hitches)))
    base = statistics.mean(groups[order[0]])
    print()
    for what in order:
        v = groups[what]
        m = statistics.mean(v)
        spread = (max(v) - min(v)) if len(v) > 1 else 0.0
        print("%-10s %d runs: %6.2f ms a frame (%5.1f fps), spread between runs %.2f ms, vs %s %+.2f ms (%+.1f%%)" % (
            what, len(v), m, 1000.0 / m, spread, order[0], m - base, 100.0 * (m - base) / base))
    # (the crowd differs between runs: the frame time at the same draws, with the ms-per-draw slope from within each group)
    sxy = sxx = 0.0
    for what in order:
        ds = [d for d, _ in windows[what]]
        ts = [t for _, t in windows[what]]
        md, mt = statistics.mean(ds), statistics.mean(ts)
        sxy += sum((d - md) * (t - mt) for d, t in windows[what])
        sxx += sum((d - md) ** 2 for d in ds)
    slope = sxy / sxx if sxx else 0.0
    all_d = statistics.mean(d for what in order for d, _ in windows[what])
    print("\nat the same draws (%.0f; %.3f ms per 100 draws, from within each group):" % (all_d, slope * 100))
    adj = {}
    for what in order:
        ds = [d for d, _ in windows[what]]
        ts = [t for _, t in windows[what]]
        adj[what] = statistics.mean(ts) - slope * (statistics.mean(ds) - all_d)
    for what in order:
        print("%-10s %6.2f ms a frame (%5.1f fps), vs %s %+.2f ms (%+.1f%%)" % (
            what, adj[what], 1000.0 / adj[what], order[0], adj[what] - adj[order[0]],
            100.0 * (adj[what] - adj[order[0]]) / adj[order[0]]))


if __name__ == "__main__":
    main()
