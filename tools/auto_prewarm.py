#!/usr/bin/env python3
"""Fully automate MasterSword shader census + Vulkan prepare + GPU-driver warm.

  python -I tools/auto_prewarm.py --game-dir "C:\\path\\to\\game"

Steps:
  1. If hookshot_shaders.bin is missing (or --force-census), extract every shader
     program from romfs via nx2/shader_census.py.
  2. Run mastersword.exe --warm-shaders (alias of --prepare-shaders): translates
     the seed, fills hookshot_programs.bin + mkw_vk_pipelines.bin, and predict_run
     creates pipelines into the GPU driver's implicit cache then discards them.

No game window. Progress lines go to stdout (@@progress / @@stage).
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Do not use python -I: zstandard (needed by shader_census) is often in the user site-packages.
PY = [sys.executable]


def log(msg: str) -> None:
    print(msg, flush=True)


def find_census_script() -> Path:
    """Locate nx2/shader_census.py whether this script lives in the repo or was copied into a game folder."""
    cands = []
    if os.environ.get("HOOKSHOT_ROOT"):
        cands.append(Path(os.environ["HOOKSHOT_ROOT"]) / "nx2" / "shader_census.py")
    here = Path(__file__).resolve()
    cands += [
        here.parents[1] / "nx2" / "shader_census.py",           # <repo>/tools/auto_prewarm.py
        here.parents[2] / "nx2" / "shader_census.py",
        Path(r"C:\Games\Hookshot - NotRexed - Test e400f88") / "nx2" / "shader_census.py",
    ]
    for c in cands:
        if c.is_file():
            return c
    raise SystemExit("nx2/shader_census.py not found (set HOOKSHOT_ROOT to the Hookshot/MasterSword repo)")


def need_census(game: Path, force: bool) -> bool:
    seed = game / "hookshot_shaders.bin"
    if force or not seed.is_file() or seed.stat().st_size < 1024:
        return True
    romfs = game / "romfs"
    if not romfs.is_dir():
        return False
    # Rebuild if romfs looks newer than the seed (rough: any Shader/ file mtime).
    seed_m = seed.stat().st_mtime
    for sub in ("Shader", "Lib", "Effect", "Movie"):
        d = romfs / sub
        if not d.is_dir():
            continue
        for p in d.rglob("*"):
            if p.is_file() and p.stat().st_mtime > seed_m + 1:
                return True
            break
    return False


def run_census(game: Path) -> None:
    romfs = game / "romfs"
    if not romfs.is_dir():
        raise SystemExit(f"romfs missing: {romfs}")
    out = game / "hookshot_shaders.bin"
    census = find_census_script()
    cmd = PY + [str(census), str(romfs), "--out", str(out)]
    log(f"[1/2] shader census → {out.name}")
    t0 = time.time()
    r = subprocess.run(cmd, cwd=str(census.parents[1]))
    if r.returncode != 0 or not out.is_file():
        raise SystemExit(f"shader census failed (exit {r.returncode})")
    log(f"[1/2] done in {time.time() - t0:.0f}s ({out.stat().st_size / (1 << 20):.1f} MB)")


def run_warm(game: Path, predict_s: int) -> None:
    exe = game / "mastersword.exe"
    if not exe.is_file():
        raise SystemExit(f"mastersword.exe missing: {exe}")
    env = os.environ.copy()
    env["RX_BACKEND"] = "vulkan"
    # Full prediction budget for a dedicated warm (0 = no limit).
    env["RX_VK_PREDICT_S"] = str(predict_s)
    # Ensure prepare runs even if a prior skip marker exists.
    skipped = game / "hookshot_prepare_skipped"
    if skipped.is_file():
        skipped.unlink()
    cmd = [str(exe), "--warm-shaders"]
    log(f"[2/2] {exe.name} --warm-shaders (RX_VK_PREDICT_S={predict_s})")
    log("      (quiet console is normal — live status from the game log below)")
    t0 = time.time()
    log_path = game / "mastersword_log.txt"
    # Truncate so we only echo this run's lines.
    try:
        log_path.write_text("", encoding="utf-8", errors="ignore")
    except OSError:
        pass

    proc = subprocess.Popen(cmd, cwd=str(game), env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1)

    import threading

    stop = threading.Event()

    def pump_stdout() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            line = line.rstrip()
            if line:
                log(line)

    def tail_game_log() -> None:
        """hos freopens stderr into mastersword_log.txt — mirror useful lines + a heartbeat."""
        pos = 0
        last_beat = 0.0
        interesting = ("prepared ", "precompile", "predicted ", "pipeline", "@@progress", "@@stage", "@@done",
                       "write-back", "presents on a queue", "seed", "Preparing")
        while not stop.is_set():
            try:
                if log_path.is_file():
                    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                        f.seek(pos)
                        chunk = f.read()
                        pos = f.tell()
                    for line in chunk.splitlines():
                        low = line.lower()
                        if any(k.lower() in low for k in interesting) or "vk: prepared" in low or "vk: predicted" in low:
                            # Strip the hos prefix for readability
                            msg = line
                            if "] " in msg:
                                msg = msg.split("] ", 1)[-1]
                            log(f"      {msg}")
            except OSError:
                pass
            now = time.time()
            if now - last_beat >= 10:
                last_beat = now
                prog = game / "hookshot_programs.bin"
                pipe = game / "mkw_vk_pipelines.bin"
                ps = f"{prog.stat().st_size / (1 << 20):.0f} MB" if prog.is_file() else "—"
                pl = f"{pipe.stat().st_size / (1 << 20):.0f} MB" if pipe.is_file() else "—"
                log(f"      … still working ({now - t0:.0f}s)  programs.bin={ps}  pipelines.bin={pl}")
            stop.wait(1.0)

    threads = [
        threading.Thread(target=pump_stdout, daemon=True),
        threading.Thread(target=tail_game_log, daemon=True),
    ]
    for t in threads:
        t.start()
    rc = proc.wait()
    stop.set()
    for t in threads:
        t.join(timeout=2)
    log(f"[2/2] finished in {time.time() - t0:.0f}s (exit {rc})")
    if rc != 0:
        raise SystemExit(rc)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--game-dir", type=Path, required=True, help="folder with mastersword.exe + romfs")
    ap.add_argument("--force-census", action="store_true", help="rebuild hookshot_shaders.bin even if present")
    ap.add_argument("--skip-census", action="store_true", help="only run --warm-shaders")
    ap.add_argument("--predict-s", type=int, default=0,
                    help="RX_VK_PREDICT_S seconds for driver-cache warm (0 = no limit, default)")
    a = ap.parse_args()
    game = a.game_dir.resolve()
    if not game.is_dir():
        raise SystemExit(f"game dir not found: {game}")

    if not a.skip_census and need_census(game, a.force_census):
        run_census(game)
    elif (game / "hookshot_shaders.bin").is_file():
        sz = (game / "hookshot_shaders.bin").stat().st_size / (1 << 20)
        log(f"[1/2] seed already present ({sz:.1f} MB), skipping census")
    else:
        log("[1/2] no seed and census skipped — prepare will only use play notes / empty seed")

    run_warm(game, a.predict_s)
    log("prewarm complete: hookshot_programs.bin + mkw_vk_pipelines.bin updated; driver cache warmed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
