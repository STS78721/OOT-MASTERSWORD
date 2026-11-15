"""MasterSword Sun's Song: who woke whom, from a HOS_WAKE_TRACE file (host/suns_song.c).

    python -I scripts/suns_song.py <trace.csv> [--thread sead::MainThread] [--min-ms 1] [--depth 6] [--top 20]

Each record: t_ns, thread (address), name, kind (wait/woke on an event, signal, send/recv/got on a message queue,
sleep/slept), key (the event or queue; a sleep's length in ns), lr (the guest caller).

For every block of --thread (a wait, a receive or a sleep) of at least --min-ms: what released it (the last signal of
that event, or send to that queue, before it ended; a sleep's own timer), and then the chain behind that: what the
releasing thread ran just before (its time since its own last block ended) and what had released that block, and so on
to --depth: the frame's critical path, link by link. Prints the blocks by what released them, then the chains, most time
first, by shape (thread and block kind at each link).
"""
import argparse
import bisect
import collections
import csv
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--thread", default="sead::MainThread")
    ap.add_argument("--min-ms", type=float, default=1.0)
    ap.add_argument("--depth", type=int, default=6)
    ap.add_argument("--top", type=int, default=20)
    a = ap.parse_args()

    recs = []
    with open(a.trace, encoding="utf-8", errors="replace", newline="") as f:
        for r in csv.DictReader(f):
            try:
                recs.append((int(r["t_ns"]), r["thread"], r["name"], r["kind"], r["key"], r["lr"]))
            except (KeyError, ValueError):
                continue
    if not recs:
        print("no records in", a.trace)
        return 1
    recs.sort()
    t_lo, t_hi = recs[0][0], recs[-1][0]
    names = {}
    for t, th, nm, *_ in recs:
        names.setdefault(th, nm)
    print("MasterSword Sun's Song: %s: %d records over %.2f s, %d threads" % (a.trace, len(recs), (t_hi - t_lo) / 1e9,
                                                                             len(names)))

    # releases: by (kind, key) the times of signals / sends, with the thread
    signals = collections.defaultdict(list)   # event key -> [(t, thread)]
    sends = collections.defaultdict(list)     # queue key -> [(t, thread)]
    blocks = collections.defaultdict(list)    # thread -> [(t0, t1, kind, key, lr)] (kind: wait / recv / sleep)
    open_ = {}
    for t, th, nm, kind, key, lr in recs:
        if kind == "signal":
            signals[key].append((t, th))
        elif kind == "send":
            sends[key].append((t, th))
        elif kind in ("wait", "recv", "sleep"):
            open_[th] = (t, kind, key, lr)
        elif kind in ("woke", "got", "slept"):
            o = open_.pop(th, None)
            if o:
                blocks[th].append((o[0], t, o[1], o[2], o[3]))
    for v in blocks.values():
        v.sort()
    sig_t = {k: [x[0] for x in v] for k, v in signals.items()}
    send_t = {k: [x[0] for x in v] for k, v in sends.items()}
    block_end = {th: [b[1] for b in v] for th, v in blocks.items()}

    def releaser(block):
        """(what, thread, time): what ended a block"""
        t0, t1, kind, key, lr = block
        if kind == "sleep":
            return ("timer", None, t1)
        src, ts = (signals, sig_t) if kind == "wait" else (sends, send_t)
        lst = ts.get(key)
        if not lst:
            return ("unseen", None, t1)
        i = bisect.bisect_right(lst, t1) - 1
        if i < 0:
            return ("before the trace", None, t1)
        t, th = src[key][i]
        if t < t0 - 1000:   # (signalled before the wait began: it found it set, or the signal is older than the trace)
            return ("already set", th, t)
        return ("host" if th == "0" else "thread", th, t)

    def prev_block(th, t):
        """the thread's last block that ended at or before t"""
        ends = block_end.get(th)
        if not ends:
            return None
        i = bisect.bisect_right(ends, t) - 1
        return blocks[th][i] if i >= 0 else None

    me = [th for th, nm in names.items() if nm == a.thread]
    if not me:
        print("no thread named %s; threads: %s" % (a.thread, ", ".join(sorted(set(names.values())))[:2000]))
        return 1
    mine = [b for th in me for b in blocks.get(th, [])]
    long_blocks = [b for b in mine if (b[1] - b[0]) >= a.min_ms * 1e6]
    total = sum(b[1] - b[0] for b in mine)
    span = t_hi - t_lo
    print("%s: %d blocks, %.1f%% of the trace blocked; %d of %.0f+ ms (%.1f%% of the trace)" % (
        a.thread, len(mine), 100.0 * total / span, len(long_blocks), a.min_ms,
        100.0 * sum(b[1] - b[0] for b in long_blocks) / span))

    by_rel = collections.Counter()
    by_rel_n = collections.Counter()
    lat = collections.defaultdict(list)
    chains = collections.Counter()
    chains_n = collections.Counter()
    examples = {}
    for b in long_blocks:
        what, th, t = releaser(b)
        who = names.get(th, "host") if th else what
        key = "%s on %s (lr %s) <- %s" % (b[2], b[3], b[4], who)
        by_rel[key] += b[1] - b[0]
        by_rel_n[key] += 1
        if th:
            lat[key].append(b[1] - t)
        # the chain behind it
        shape = ["%s:%s" % (a.thread, b[2])]
        text = ["%s %s %.2f ms" % (a.thread, b[2], (b[1] - b[0]) / 1e6)]
        cur_th, cur_t, cur_what = th, t, what
        for _ in range(a.depth):
            if not cur_th or cur_what not in ("thread",):
                shape.append(cur_what)
                text.append("<- " + cur_what)
                break
            nm = names.get(cur_th, "?")
            pb = prev_block(cur_th, cur_t)
            if not pb:
                shape.append(nm + ":ran")
                text.append("<- %s (no block of it in the trace)" % nm)
                break
            ran = (cur_t - pb[1]) / 1e6
            shape.append("%s:%s" % (nm, pb[2]))
            text.append("<- %s ran %.2f ms after its %s on %s (%.2f ms)" % (nm, ran, pb[2], pb[3], (pb[1] - pb[0]) / 1e6))
            cur_what, cur_th, cur_t = releaser(pb)
        s = " <- ".join(shape)
        chains[s] += b[1] - b[0]
        chains_n[s] += 1
        if s not in examples or (b[1] - b[0]) > examples[s][0]:
            examples[s] = (b[1] - b[0], " ".join(text))

    print("\n-- %s's long blocks by what ended them (ms over the trace, count, signal-to-running average)" % a.thread)
    for k, v in by_rel.most_common(a.top):
        l = lat.get(k)
        print("  %8.1f ms  %4d  %s%s" % (v / 1e6, by_rel_n[k], k, "  (%.0f us)" % (sum(l) / len(l) / 1e3) if l else ""))
    print("\n-- the chains behind them, by shape (ms of %s's blocking, count), each with its longest example" % a.thread)
    for k, v in chains.most_common(a.top):
        print("  %8.1f ms  %4d  %s" % (v / 1e6, chains_n[k], k))
        print("               e.g. %s" % examples[k][1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
