#!/usr/bin/env python3
"""0.2.1: Vulkan PGO, Threaded Rendering, Selective Capture + Shadow Baseline, Native Zstd & Name Search,
Off-Thread Resource Reaper, Shared Pipeline Cache Locks, and RTX 50 Water Hang / White-Link Fix.
Default: stage only. Pass --upload (or --upload-github) to publish to GitHub."""
from __future__ import annotations

import base64
import json
import shutil
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(r"C:\Games\Hookshot - NotRexed - Test e400f88")
PREP = ROOT / "runs" / "_release_prep"
SEED_PORTABLE = PREP / "stage_0.2.0" / "portable"
if not SEED_PORTABLE.is_dir():
    SEED_PORTABLE = PREP / "stage_0.1.9d" / "portable"
STAGE = PREP / "stage_0.2.1"
DIST = PREP / "dist"
BUILD_EXE = ROOT / "work" / "sa" / "build" / "mastersword.exe"
BUILD_VK = ROOT / "work" / "sa" / "build" / "mastersword_vk.dll"
LAUNCHER = ROOT / "launcher" / "dist" / "MasterSwordLauncher.exe"
FFX = ROOT / "work" / "ffx-sdk"
VER = "0.2.1"
TAG = "0.2.1"
GH_REPO = "STS78721/OOT-MASTERSWORD"

FFX_FILES = (
    "amd_fidelityfx_loader_vk.dll",
    "amd_fidelityfx_upscaler_vk.dll",
    "LICENSE-AMD-FSR-SDK.md",
)

COMMON_ALLOWLIST = {
    "mastersword.exe",
    "mastersword_vk.dll",
    "hookshot_vk.dll",
    "MasterSwordLauncher.exe",
    "mastersword.png",
    "mastersword.ico",
    "hookshot_pipeline_states_shipped.bin",
    "amd_fidelityfx_loader_vk.dll",
    "amd_fidelityfx_upscaler_vk.dll",
    "LICENSE-AMD-FSR-SDK.md",
    "nvngx_dlss.license.txt",
    "PATCH_NOTES.md",
    "mastersword_version.txt",
}

PORTABLE_ALLOWLIST = COMMON_ALLOWLIST | {"Launch MasterSword.cmd", "READ ME.txt"}
UPDATE_ALLOWLIST = set(COMMON_ALLOWLIST)

FORBIDDEN_NAMES = {
    "mastersword_options.ini", "hookshot_options.ini", "mastersword_launcher.json", "launcher_config.json",
    "hookshot_env.txt", "image.bin", "mkw_vk_pipelines.bin", "hookshot_programs.bin", "hookshot_prepared.bin",
    "hookshot_state_list_prepared.bin", "hookshot_computes.bin", "mastersword_spirv.pack", "mastersword_spirv.idx",
    "mastersword_spirv.song", "mastersword_slate.bin", "nvngx_dlss.dll", "nvngx_dlssg.dll", "nvngx_dlssd.dll",
    "_nvngx.dll",
}
FORBIDDEN_EXTS = {".ini", ".json", ".log", ".bak"}


def copy(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def zip_dir(src_dir: Path, zip_path: Path, expected_names: set[str]) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for path in sorted(src_dir.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(src_dir).as_posix())
    with zipfile.ZipFile(zip_path, "r") as zf:
        names = set(zf.namelist())
    if names != expected_names:
        raise SystemExit(f"ALLOWLIST MISMATCH in {zip_path.name}: extra={names - expected_names} missing={expected_names - names}")
    print(f"zip {zip_path.name} ({zip_path.stat().st_size} bytes, {len(names)} allowlisted files)")


def check_forbidden(folder: Path, expected_names: set[str]) -> None:
    actual = set()
    for p in folder.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(folder).as_posix()
        actual.add(rel)
        low = p.name.lower()
        if (
            low in FORBIDDEN_NAMES
            or p.suffix.lower() in FORBIDDEN_EXTS
            or low.startswith(("hookshot_log", "mastersword_log", "romfs", "user", "shader-cache", "sl."))
        ):
            raise SystemExit(f"GOLDEN RULE VIOLATION: forbidden/personal file {p}")
    if actual != expected_names:
        raise SystemExit(f"ALLOWLIST MISMATCH in {folder}: extra={actual - expected_names} missing={expected_names - actual}")


def stage() -> None:
    shipped_states = SEED_PORTABLE / "hookshot_pipeline_states_shipped.bin"
    dlss_license = SEED_PORTABLE / "nvngx_dlss.license.txt"
    launch_cmd = SEED_PORTABLE / "Launch MasterSword.cmd"
    logo_png = ROOT / "assets" / "mastersword.png"
    icon_ico = ROOT / "assets" / "mastersword.ico"
    notes_path = PREP / f"PATCH_NOTES_{VER}.md"

    for p in (BUILD_EXE, BUILD_VK, LAUNCHER, shipped_states, dlss_license, launch_cmd, logo_png, icon_ico, notes_path):
        if not p.is_file():
            raise SystemExit(f"missing required input: {p}")
    if shipped_states.stat().st_size < 10_000_000 or shipped_states.read_bytes()[:8] not in (b"HKSTATE1", b"HKSTATE2"):
        raise SystemExit(f"invalid shipped pipeline state list: {shipped_states}")
    for name in FFX_FILES:
        if not (FFX / name).is_file():
            raise SystemExit(f"missing FSR runtime {FFX / name}")

    exe_bytes = BUILD_EXE.read_bytes()
    if VER.encode() not in exe_bytes:
        raise SystemExit(f"mastersword.exe missing {VER} stamp")
    if GH_REPO.encode("utf-16-le") not in exe_bytes and GH_REPO.encode() not in exe_bytes:
        raise SystemExit(f"mastersword.exe missing {GH_REPO} update endpoint")
    vk_bytes = BUILD_VK.read_bytes()
    if VER.encode() not in vk_bytes:
        raise SystemExit(f"mastersword_vk.dll missing {VER} stamp")
    if b"__llvm_prf" in vk_bytes or b".lprfd" in vk_bytes:
        raise SystemExit("mastersword_vk.dll contains PGO instrumentation counters")
    for stale in (b"MasterSword 0.2.0", b"0.1.9d", b"0.1.9c", b"0.1.9b", b"0.1.9\x00", b"0.1.8\x00", b"0.1.7\x00", b"0.1.6\x00", b"0.1.5e", b"0.1.5d"):
        if stale in exe_bytes:
            raise SystemExit(f"mastersword.exe contains stale stamp {stale!r}")
        if stale in vk_bytes:
            raise SystemExit(f"mastersword_vk.dll contains stale stamp {stale!r}")
    for marker in (
        b"Intel Hybrid CPU Patch",
        b"Left stick speed",
        b"Apex FG (Fast) (recommended)",
        b"Field of view",
        b"AMD FSR 4",
        b"nvngx_dlss",
        b"nvngx_dlssg",
        b"descriptor slotgen ready",
        b"sampler cache ready",
        b"render targets' images from blocks of their own",
        b"hookshot_pipeline_states_shipped.bin",
        b"Threaded rendering",
    ):
        if marker not in vk_bytes:
            raise SystemExit(f"mastersword_vk.dll missing expected marker {marker!r}")

    probe = subprocess.run([str(BUILD_EXE), "--probe-dpi"], capture_output=True, text=True, timeout=15, check=False)
    probe_out = (probe.stdout or "") + "\n" + (probe.stderr or "")
    if probe.returncode != 0 or f"probe-dpi: PASS ({VER}" not in probe_out:
        raise SystemExit(f"mastersword.exe --probe-dpi failed: rc={probe.returncode} out={probe_out!r}")

    # GOLDEN RULE CHECK 1: C++ Options fresh-install defaults (1080p baseline)
    vk_menu_h = (ROOT / "gpu" / "vk" / "vk_menu.h").read_text(encoding="utf-8", errors="ignore")
    for required_default in (
        "int fg_backend = 0;",
        "int framegen = 0;",
        "int output_res = 0;",
        "int upscale = 0;",
        "int fullscreen = 0;",
        "int reflex = 0;",
        "int preset = 0;",
        "int handheld = 0;",
        "int fov = 100;",
    ):
        if required_default not in vk_menu_h:
            raise SystemExit(f"GOLDEN RULE VIOLATION in vk_menu.h: missing '{required_default}'")

    # GOLDEN RULE CHECK 2: Launcher version, updater target, and safe 1080p UI defaults
    launcher_src = (ROOT / "launcher" / "MasterSwordLauncher.py").read_text(encoding="utf-8", errors="ignore")
    if f'CURRENT_VERSION = "{VER}"' not in launcher_src:
        raise SystemExit(f"MasterSwordLauncher.py not set to {VER}")
    if GH_REPO not in launcher_src:
        raise SystemExit(f"launcher missing GitHub {GH_REPO} updater target")
    if "nvngx_dlssg.dll" not in launcher_src:
        raise SystemExit("launcher missing nvngx_dlssg.dll support")
    for req_ui in (
        '"output_res":"1920x1080"',
        '"upscaler":"off"',
        '"fg_mode":"off"',
        '"reflex":"off"',
        '"fullscreen":"0"',
        '"fov":"100"',
    ):
        if req_ui not in launcher_src:
            raise SystemExit(f"GOLDEN RULE VIOLATION in MasterSwordLauncher.py: missing {req_ui}")
    if LAUNCHER.stat().st_size < 10_000_000:
        raise SystemExit("MasterSwordLauncher.exe looks truncated")

    notes = notes_path.read_text(encoding="utf-8")
    if STAGE.exists():
        shutil.rmtree(STAGE)

    readme_txt = (
        f"SteelTemple MasterSword {VER}\n\n"
        "Requires image.bin + romfs\\ beside mastersword.exe.\n"
        "Out of the box: 1920x1080 Windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, V-Sync Mailbox.\n"
        "Intel Hybrid CPU Patch: On (Recommended for 12th/13th/14th gen Core and Core Ultra) in the Insert / F1 menu, Graphics tab.\n"
        "Tip: Win + G > Settings > General > Remember this is a game (with the game window focused) makes play smoother.\n"
        "Includes hookshot_pipeline_states_shipped.bin (41,839 pre-baked pipeline states) for zero-stutter gameplay.\n"
        "NVIDIA: DLSS SR (presets K/M/L + DLAA) + Hardware NGX DLSS MFG (2x/3x/4x) + Reflex (with RTX 50 volume-box hang fix).\n"
        "AMD: FSR 4 (amd_fidelityfx_*_vk.dll included) + Temporal FSR 3.1 SR & FG + low-VRAM protection + idle import give-back.\n"
        "Use SETTINGS -> Download & Install DLSS SDK in MasterSwordLauncher.exe to install ngx\\nvngx_dlss.dll & ngx\\nvngx_dlssg.dll.\n"
    )

    for kind, expected in (("portable", PORTABLE_ALLOWLIST), ("update", UPDATE_ALLOWLIST)):
        folder = STAGE / kind
        folder.mkdir(parents=True, exist_ok=True)
        copy(BUILD_EXE, folder / "mastersword.exe")
        copy(BUILD_VK, folder / "mastersword_vk.dll")
        copy(BUILD_VK, folder / "hookshot_vk.dll")
        copy(LAUNCHER, folder / "MasterSwordLauncher.exe")
        copy(logo_png, folder / "mastersword.png")
        copy(icon_ico, folder / "mastersword.ico")
        copy(shipped_states, folder / "hookshot_pipeline_states_shipped.bin")
        copy(dlss_license, folder / "nvngx_dlss.license.txt")
        for name in FFX_FILES:
            copy(FFX / name, folder / name)
        (folder / "PATCH_NOTES.md").write_bytes(notes.replace("\r\n", "\n").encode("utf-8"))
        (folder / "mastersword_version.txt").write_bytes(f"version={VER}\n".encode("utf-8"))
        if kind == "portable":
            copy(launch_cmd, folder / "Launch MasterSword.cmd")
            (folder / "READ ME.txt").write_bytes(readme_txt.replace("\r\n", "\n").encode("utf-8"))
        check_forbidden(folder, expected)

    DIST.mkdir(parents=True, exist_ok=True)
    zip_dir(STAGE / "portable", DIST / f"SteelTemple-MasterSword-{VER}-Portable-StandAlone.zip", PORTABLE_ALLOWLIST)
    zip_dir(STAGE / "update", DIST / f"SteelTemple-MasterSword-{VER}-Update.zip", UPDATE_ALLOWLIST)
    copy(LAUNCHER, DIST / "MasterSwordLauncher.exe")
    print(f"staged {VER} cleanly (all Golden Rule checks PASSED)")


def release_files() -> list[tuple[Path, str]]:
    return [
        (DIST / f"SteelTemple-MasterSword-{VER}-Portable-StandAlone.zip", "application/zip"),
        (DIST / f"SteelTemple-MasterSword-{VER}-Update.zip", "application/zip"),
        (DIST / "MasterSwordLauncher.exe", "application/octet-stream"),
    ]


def github_token() -> str:
    res = subprocess.run(
        ["git", "credential", "fill"],
        input="protocol=https\nhost=github.com\n\n",
        text=True,
        capture_output=True,
        check=False,
    )
    for line in res.stdout.splitlines():
        if line.startswith("password="):
            return line.split("=", 1)[1].strip()
    raise SystemExit("no GitHub token in git credential manager")


def gh_api(token: str, method: str, url: str, data: bytes | None = None, content_type: str | None = None) -> dict:
    headers = {
        "User-Agent": "MasterSword-Release",
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
    }
    if content_type:
        headers["Content-Type"] = content_type
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            body = resp.read()
            return json.loads(body.decode("utf-8")) if body else {}
    except urllib.error.HTTPError as e:
        err = e.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"GitHub {method} {url} -> {e.code}: {err}") from e


def update_github_readme(token: str) -> None:
    local = (PREP / "README_MASTERSWORD.md").read_text(encoding="utf-8")
    encoded = base64.b64encode(local.encode("utf-8")).decode("ascii")
    for name in ("README.md", "readme.md"):
        try:
            meta = gh_api(token, "GET", f"https://api.github.com/repos/{GH_REPO}/contents/{name}")
        except RuntimeError as e:
            if "404" in str(e):
                continue
            raise
        cur = base64.b64decode(meta.get("content", "")).decode("utf-8", errors="replace")
        if cur == local:
            print(f"GitHub {name} already current")
            continue
        payload = {
            "message": f"docs: MasterSword {VER}",
            "content": encoded,
            "branch": "main",
            "sha": meta["sha"],
        }
        gh_api(token, "PUT", f"https://api.github.com/repos/{GH_REPO}/contents/{name}", json.dumps(payload).encode("utf-8"), "application/json")
        print(f"updated GitHub {name}")


def gh_ensure_release(token: str, body: str) -> dict:
    payload = {
        "tag_name": TAG,
        "name": f"MasterSword {VER}",
        "body": body,
        "draft": False,
        "prerelease": False,
        "make_latest": "true",
    }
    try:
        return gh_api(token, "POST", f"https://api.github.com/repos/{GH_REPO}/releases", json.dumps(payload).encode("utf-8"), "application/json")
    except RuntimeError as e:
        if "422" not in str(e) and "already_exists" not in str(e):
            raise
        rel = gh_api(token, "GET", f"https://api.github.com/repos/{GH_REPO}/releases/tags/{TAG}")
        gh_api(
            token,
            "PATCH",
            f"https://api.github.com/repos/{GH_REPO}/releases/{rel['id']}",
            json.dumps({"name": payload["name"], "body": body, "draft": False, "prerelease": False, "make_latest": "true"}).encode("utf-8"),
            "application/json",
        )
        return gh_api(token, "GET", f"https://api.github.com/repos/{GH_REPO}/releases/tags/{TAG}")


def gh_upload_asset(token: str, upload_url: str, path: Path, content_type: str) -> None:
    url = f"{upload_url}?name={urllib.parse.quote(path.name)}"
    data = path.read_bytes()
    headers = {
        "User-Agent": "MasterSword-Release",
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "Content-Type": content_type,
        "Content-Length": str(len(data)),
    }
    last = None
    for attempt in range(3):
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=600) as resp:
                resp.read()
                print(f"GitHub uploaded {path.name} ({len(data)} bytes)")
                return
        except urllib.error.HTTPError as e:
            err = e.read().decode("utf-8", errors="replace")
            last = e
            if e.code == 422 and "already_exists" in err:
                rel = gh_api(token, "GET", f"https://api.github.com/repos/{GH_REPO}/releases/tags/{TAG}")
                for asset in rel.get("assets", []):
                    if asset.get("name") == path.name:
                        gh_api(token, "DELETE", f"https://api.github.com/repos/{GH_REPO}/releases/assets/{asset['id']}")
                        break
                continue
            if e.code in (408, 502, 503, 504) and attempt < 2:
                print(f"GitHub upload {path.name} got {e.code}, retrying")
                continue
            raise RuntimeError(f"GitHub upload {path.name}: {e.code} {err}") from e
        except (TimeoutError, urllib.error.URLError) as e:
            last = e
            if attempt < 2:
                print(f"GitHub upload {path.name} stalled, retrying")
                continue
            raise
    raise RuntimeError(f"GitHub upload {path.name} failed") from last


def upload_github() -> None:
    token = github_token()
    notes = (PREP / f"PATCH_NOTES_{VER}.md").read_text(encoding="utf-8")
    update_github_readme(token)
    rel = gh_ensure_release(token, notes)
    base = rel["upload_url"].split("{", 1)[0]
    files = release_files()
    existing = {a["name"]: a for a in rel.get("assets", [])}
    for path, ctype in files:
        ex = existing.get(path.name)
        if ex and ex.get("size") == path.stat().st_size:
            print(f"GitHub already has {path.name} ({ex['size']} bytes)")
            continue
        if ex:
            gh_api(token, "DELETE", f"https://api.github.com/repos/{GH_REPO}/releases/assets/{ex['id']}")
        gh_upload_asset(token, base, path, ctype)
    rel = gh_api(token, "GET", f"https://api.github.com/repos/{GH_REPO}/releases/tags/{TAG}")
    got = {a["name"]: a["size"] for a in rel.get("assets", [])}
    for path, _ in files:
        if got.get(path.name) != path.stat().st_size:
            raise SystemExit(f"GitHub size mismatch for {path.name}: {got.get(path.name)} vs {path.stat().st_size}")
    print("GitHub release ready:", rel.get("html_url"))


def main() -> None:
    stage()
    if "--upload" in sys.argv or "--upload-github" in sys.argv:
        upload_github()
    else:
        print("stage-only complete — pass --upload when ready to publish to GitHub")


if __name__ == "__main__":
    main()
