"""Clean-room static hot-function extractor & linker order generator for lifted C.

Extracts genuinely hot engine, math/physics/animation (FPX-dense), and ZSTD streaming
functions from work/sa/gen/code_*.c into code_hot_0.c .. code_hot_7.c (compiled at -O2
with inlined a64_fpx_hot_* fast paths and call-graph co-location via split_hot.py), and
writes work/hot_order.txt so lld-link packs hot game + host symbols contiguously at the
start of .text for L1i / iTLB locality.
"""
import collections
import glob
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import kit  # noqa: E402,F401
import chunks  # noqa: E402
import lifted_c  # noqa: E402
import split_hot  # noqa: E402

CALL_RE = re.compile(r"\b(f_[0-9a-f]+)\(c\)")
DECL_FN_RE = re.compile(r"\b(f_[0-9a-f]+)\b")
HOT_FPX_DEF_RE = re.compile(r"\ba64_fpx_hot_([A-Za-z0-9_]+)\s*\(")


def restore_if_needed(gen, backup_dir):
    if not os.path.isdir(backup_dir):
        return
    for bak in glob.glob(os.path.join(backup_dir, "*")):
        shutil.copy2(bak, os.path.join(gen, os.path.basename(bak)))
    for old in glob.glob(os.path.join(gen, "code_hot*")):
        if os.path.isfile(old):
            os.remove(old)


def main():
    gen = os.path.join(ROOT, "work", "sa", "gen")
    work = os.path.join(ROOT, "work")
    backup_dir = os.path.join(gen, "_cold_backup")

    # Ensure idempotency if re-run.
    restore_if_needed(gen, backup_dir)
    os.makedirs(backup_dir, exist_ok=True)
    costs_path = os.path.join(gen, "chunk_costs.txt")
    if os.path.exists(costs_path) and not os.path.exists(os.path.join(backup_dir, "chunk_costs.txt")):
        shutil.copy2(costs_path, os.path.join(backup_dir, "chunk_costs.txt"))

    # Discover exact a64_fpx_hot_* helpers available in a64_fpx.h.
    fpx_header = os.path.join(gen, "a64_fpx.h")
    with open(fpx_header, encoding="utf-8") as f:
        hot_ops = sorted(set(HOT_FPX_DEF_RE.findall(f.read())))
    if not hot_ops:
        raise SystemExit("make_hot_split: no a64_fpx_hot_* helpers found in a64_fpx.h")
    fpx_replace_re = re.compile(r"\ba64_fpx_(" + "|".join(re.escape(op) for op in hot_ops) + r")\b")
    print(f"make_hot_split: {len(hot_ops)} inlinable a64_fpx_hot_* ops found")

    cold_files = sorted(
        p for p in glob.glob(os.path.join(gen, "code_*.c"))
        if not os.path.basename(p).startswith("code_hot")
    )
    print(f"make_hot_split: scanning {len(cold_files)} cold chunks...")

    file_preambles = {}
    extra_preamble_lines = []
    extra_seen = set()
    fn_file = {}
    fn_size = {}
    fn_fpx = {}
    fn_zstd = {}
    in_calls = collections.Counter()
    out_edges = {}

    # Pass 1: lightweight scan (store metadata + call graph without holding 1.6 GB of strings at once)
    for path in cold_files:
        bname = os.path.basename(path)
        with open(path, encoding="utf-8", newline="") as f:
            text = f.read()
        marks = list(lifted_c.HEAD_NOTED.finditer(text))
        if not marks:
            continue
        pre = text[:marks[0].start()]
        file_preambles[bname] = pre
        for line in pre.splitlines():
            s = line.strip()
            if s and s != '#include "a64.h"' and not s.startswith("void f_"):
                if s not in extra_seen:
                    extra_seen.add(s)
                    extra_preamble_lines.append(s)
        for i, m in enumerate(marks):
            name = m.group(1)
            end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
            body = text[m.start():end]
            sz = len(body)
            fn_file[name] = bname
            fn_size[name] = sz
            fn_fpx[name] = body.count("a64_fpx_")
            fn_zstd[name] = "/* ZSTD_DECODER */" in body
            callees = CALL_RE.findall(body)
            if callees:
                c_counts = collections.Counter(c for c in callees if c != name)
                if c_counts:
                    out_edges[name] = dict(c_counts)
                    for callee, cnt in c_counts.items():
                        in_calls[callee] += cnt

    print(f"make_hot_split: indexed {len(fn_size)} functions across {len(file_preambles)} chunks")

    # Pass 2: select genuinely hot functions while keeping cold-chunk touch count compact (~22 chunks)
    # 1) Always include ZSTD_DECODER functions.
    selected = {f for f, z in fn_zstd.items() if z}
    print(f"make_hot_split: {len(selected)} ZSTD_DECODER functions selected")

    # Score candidate functions (size <= 72 KB so no giant cutscene state machine bloats -O2).
    cand_scores = {}
    chunk_heat = collections.Counter()
    for f, sz in fn_size.items():
        if f in selected or sz > 72000 or sz < 90:
            continue
        inc = in_calls[f]
        fpx = fn_fpx[f]
        score = 0.0
        if inc >= 25:
            score += inc * 12.0
        if inc >= 5 and fpx >= 8:
            # High FPX density + called multiple times -> math/physics/skinning/camera/audio loops
            density = fpx / max(1.0, sz / 1000.0)
            score += inc * fpx * 2.5 + density * 40.0
        if score >= 320.0:
            cand_scores[f] = score
            chunk_heat[fn_file[f]] += score

    # Focus on the top 20 hottest chunks (plus chunks already touched by ZSTD_DECODER) so incremental
    # rebuild only recompiles ~22 cold chunks instead of all 75!
    hot_chunks = {fn_file[f] for f in selected}
    for bname, _ in chunk_heat.most_common(20):
        hot_chunks.add(bname)
    # Also allow ultra-universal hubs (in_calls >= 180) from any chunk (typically <= 4 extra chunks)
    for f, sc in sorted(cand_scores.items(), key=lambda kv: -kv[1]):
        if fn_file[f] not in hot_chunks and in_calls[f] < 220:
            continue
        selected.add(f)
        hot_chunks.add(fn_file[f])
        if sum(fn_size[x] for x in selected) >= 12_500_000 or len(selected) >= 680:
            break

    # Add small hot leaf callees inside ALREADY-touched hot_chunks so -O2 inlines them into hot callers
    # without touching additional cold chunks!
    leaf_edges = collections.Counter()
    for caller in list(selected):
        for callee, cnt in out_edges.get(caller, {}).items():
            if callee not in selected and callee in fn_size and fn_file[callee] in hot_chunks:
                if fn_size[callee] <= 12000:
                    leaf_edges[callee] += cnt
    for callee, cnt in leaf_edges.most_common():
        if cnt < 3:
            break
        if sum(fn_size[x] for x in selected) + fn_size[callee] > 14_500_000 or len(selected) >= 820:
            break
        selected.add(callee)

    touched_chunks = sorted({fn_file[f] for f in selected})
    total_hot_bytes = sum(fn_size[f] for f in selected)
    print(
        f"make_hot_split: selected {len(selected)} hot functions "
        f"({total_hot_bytes / (1024 * 1024):.2f} MB) across {len(touched_chunks)} cold chunks"
    )

    # Pass 3: back up touched cold chunks, extract hot bodies, rewrite touched cold chunks & chunk_costs.txt
    hot_bodies = {}
    cold_costs = {}
    for bname in touched_chunks:
        src_path = os.path.join(gen, bname)
        bak_path = os.path.join(backup_dir, bname)
        if not os.path.exists(bak_path):
            shutil.copy2(src_path, bak_path)
        with open(src_path, encoding="utf-8", newline="") as f:
            text = f.read()
        marks = list(lifted_c.HEAD_NOTED.finditer(text))
        pre = text[:marks[0].start()]
        kept_parts = [pre]
        chunk_cost = len(pre)
        for i, m in enumerate(marks):
            name = m.group(1)
            end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
            body = text[m.start():end]
            if name in selected:
                hot_bodies[name] = fpx_replace_re.sub(r"a64_fpx_hot_\1", body)
            else:
                kept_parts.append(body)
                chunk_cost += chunks.fn_cost(len(body))
        new_text = "".join(kept_parts)
        with open(src_path, "w", encoding="utf-8", newline="") as f:
            f.write(new_text)
        cold_costs[bname] = chunk_cost
    chunks.write_costs(gen, cold_costs)

    # Pass 4: assemble code_hot.c.whole with complete declarations and split into 8 balanced pieces
    needed_decls = set(hot_bodies.keys())
    for body in hot_bodies.values():
        needed_decls.update(DECL_FN_RE.findall(body))

    hot_pre_lines = ['#include "a64.h"']
    hot_pre_lines.extend(extra_preamble_lines)
    for fn in sorted(needed_decls):
        hot_pre_lines.append(f"void {fn}(Ctx *A64_NOESC);")
    hot_preamble = "\n".join(hot_pre_lines) + "\n"

    whole_path = os.path.join(gen, "code_hot.c.whole")
    with open(whole_path, "w", encoding="utf-8", newline="") as f:
        f.write(hot_preamble)
        for fn in sorted(hot_bodies):
            f.write(hot_bodies[fn])

    split_hot.split(gen, 8)

    # Pass 5: build work/hot_order.txt (host hot runtime + hot lifted functions + top called functions,
    # clustered by call affinity so callers and callees share i-cache lines and 2 MB iTLB pages)
    host_hot_syms = [
        "hos_wait_any",
        "hos_try_wait_any",
        "hos_signal_event",
        "hos_wait_event",
        "hos_try_wait_event",
        "hos_clear_event",
        "hos_acquire_mutex",
        "hos_release_mutex",
        "hos_wait_condvar",
        "hos_signal_condvar",
        "hos_send_message",
        "hos_receive_message",
        "hos_peek_message",
    ]
    seed_rank = sorted(
        fn_size.keys(),
        key=lambda f: (
            0 if f in selected else 1,
            -(in_calls[f] * 10 + fn_fpx[f] * 3),
            f,
        ),
    )[:4500]
    seed_set = set(seed_rank)
    ordered = []
    visited = set()
    for root_fn in seed_rank:
        if root_fn in visited:
            continue
        visited.add(root_fn)
        ordered.append(root_fn)
        # Place direct hot callees immediately after caller for fallthrough / L1i locality
        callees = sorted(
            (c for c in out_edges.get(root_fn, {}) if c in seed_set and c not in visited),
            key=lambda c: (-out_edges[root_fn][c], fn_size[c]),
        )
        for c in callees[:6]:
            visited.add(c)
            ordered.append(c)

    order_path = os.path.join(work, "hot_order.txt")
    with open(order_path, "w", encoding="utf-8", newline="\n") as f:
        for sym in host_hot_syms + ordered:
            f.write(sym + "\n")
    print(f"make_hot_split: wrote {len(host_hot_syms) + len(ordered)} symbols to {order_path}")


if __name__ == "__main__":
    main()
