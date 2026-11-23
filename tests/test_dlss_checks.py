"""NVIDIA DLSS's small checks (gpu/vk/dlss_checks.h): which scene sizes DLSS takes for an output (the handheld profile's
512x288 at Ultra Performance too, which a fixed 640x360 floor turned away), and Linux paths for NGX: which are ASCII (NGX's
Linux library takes only those; the others go to it as /proc/self/fd/<n>), and UTF-8 decoded (accents, CJK, bytes that are
not UTF-8). Built as C++ and run; also that vk_dlss.cpp and vk_draw.cpp use them. No game,
GPU or NVIDIA SDK required.

python -I -B tests/test_dlss_checks.py [--cxx g++]
"""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check_sources():
    dlss = (ROOT / "gpu" / "vk" / "vk_dlss.cpp").read_text(encoding="utf-8")
    draw = (ROOT / "gpu" / "vk" / "vk_draw.cpp").read_text(encoding="utf-8")
    temporal = (ROOT / "gpu" / "vk" / "vk_temporal.cpp").read_text(encoding="utf-8")
    # (no fixed floor on the scene's size: it follows the output)
    for name, src in (("vk_dlss.cpp", dlss), ("vk_temporal.cpp", temporal), ("vk_draw.cpp", draw)):
        assert not re.search(r"\b(width|height)\s*<\s*(640|360)\b", src), name + ": a fixed scene size floor"
    assert "!temporal_scene_size(width, height)" in temporal, "temporal_note_scene_draw: the output's sizes"
    assert "!temporal_scene_size(at.width, at.height)" in draw, "temporal_skin_check: the output's sizes"
    # (the Linux wide(): decoded, not each byte widened)
    widened = [l.strip() for l in dlss.splitlines() if "std::wstring(s.begin(), s.end())" in l]
    assert widened == ["if (dlss_checks::ascii(s)) return std::wstring(s.begin(), s.end());"], \
        "wide(): bytes widened one by one only for ASCII paths"
    assert "dlss_checks::utf8_to_wide(s, &bad)" in dlss, "wide(): UTF-8 decoded"
    # (NGX's Linux library converts its wide paths back in the "C" locale, ASCII only: a folder named otherwise goes to it
    # as /proc/self/fd/<n>)
    assert "dlss_checks::ascii(s)" in dlss and '"/proc/self/fd/"' in dlss, "wide(): non-ASCII folders by descriptor"
    # (bounded retry on transient VRAM OOM; cleared on feature creation, release(), and dlss_retry())
    assert "dlss_checks::retry::verdict(s.refusal, key, texture_frame_no())" in dlss, "prepare: checks retry verdict"
    assert "dlss_checks::retry::failed(s.refusal, key, out_of_memory, texture_frame_no())" in dlss and \
        "NVSDK_NGX_Result_FAIL_OutOfGPUMemory" in dlss, "prepare: records OOM retry"
    assert "dlss_checks::retry::made(s.refusal)" in dlss, "prepare: made clears refusal"
    rel_start = dlss.index("void release() {")
    release = dlss[rel_start:dlss.index("\nuint64_t memory()", rel_start)]
    retry_start = dlss.index("void dlss_retry() { st().refusal")
    retry = dlss[retry_start:dlss.index("}", retry_start) + 1]
    assert "s.refusal = {}" in release, "release() clears refusal"
    assert "st().refusal = {}" in retry, "dlss_retry() clears refusal"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    check_sources()
    with tempfile.TemporaryDirectory(prefix="dlss-checks-") as tmp:
        exe = Path(tmp) / "dlss-checks"
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(ROOT / "tests" / "test_dlss_checks.cpp"),
                        "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
