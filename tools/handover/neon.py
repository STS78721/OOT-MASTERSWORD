"""Count saturated neon foliage pixels (bright green / red / cyan stems) in RX_VK_SHOTS frames.

Usage: python tools/handover/neon.py <shots_dir> [lo_present] [hi_present]
"""
from __future__ import annotations

import os
import sys

from PIL import Image


def neon_mask(rgb):
    r, g, b = (int(x) for x in rgb[:3])
    # saturated stem colours: high green or red, low other channels; or cyan (high G+B, low R)
    if g >= 200 and r < 80 and b < 80:
        return True
    if r >= 200 and g < 80 and b < 80:
        return True
    if g >= 180 and b >= 180 and r < 80:
        return True
    if g >= 220 and r <= 40 and b <= 120:  # neon green with a little blue
        return True
    # yellow / yellow-white glowing stems (Crono #696 residual after cyan fixed)
    if g >= 200 and r >= 160 and b < 140 and (g + r) > 400:
        return True
    return False


def score(path: str) -> int:
    im = Image.open(path).convert("RGB")
    px = im.getdata()
    return sum(1 for p in px if neon_mask(p))


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: neon.py <shots_dir> [lo] [hi]")
    d = sys.argv[1]
    lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    hi = int(sys.argv[3]) if len(sys.argv) > 3 else 10**9
    files = sorted(f for f in os.listdir(d) if f.startswith("vk_") and f.endswith(".png"))
    rows = []
    for f in files:
        n = int(f[3:9])
        if n < lo or n > hi:
            continue
        c = score(os.path.join(d, f))
        rows.append((f, c))
    if not rows:
        raise SystemExit("no frames in range")
    total = sum(c for _, c in rows)
    peak = max(rows, key=lambda x: x[1])
    hot = sum(1 for _, c in rows if c > 0)
    print(f"frames {len(rows)} neon_total {total} neon_hot {hot} peak {peak[0]}={peak[1]}")
    for f, c in sorted(rows, key=lambda x: -x[1])[:8]:
        if c:
            print(f"  {f} {c}")


if __name__ == "__main__":
    main()
