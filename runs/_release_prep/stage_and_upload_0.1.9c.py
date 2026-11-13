#!/usr/bin/env python3
"""0.1.9c: smooth play on hybrid CPUs. Intel Hybrid CPU Patch (menu), parallel replay by default, replay pin, VRAM and texture fixes.
Default: stage only (no upload). Pass --upload to publish."""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(r"C:\Games\Hookshot - NotRexed - Test e400f88")
PREP = ROOT / "runs" / "_release_prep"
PREV = PREP / "stage_0.1.9b"
STAGE = PREP / "stage_0.1.9c"
DIST = PREP / "dist"
BUILD_EXE = ROOT / "work" / "sa" / "build" / "mastersword.exe"
BUILD_VK = ROOT / "work" / "sa" / "build" / "mastersword_vk.dll"
LAUNCHER = ROOT / "launcher" / "dist" / "MasterSwordLauncher.exe"
SHIPPED_STATES = Path(r"C:\Games\Ocarina of Time PC\Hookshot Test - smoke_0.1.3b\hookshot_pipeline_states_shipped.bin")
FFX = ROOT / "work" / "ffx-sdk"
REPO = "STS78721/OOT-MASTERSWORD"
TAG = "0.1.9c"
VER = "0.1.9c"
FFX_FILES = (
    "amd_fidelityfx_loader_vk.dll",
    "amd_fidelityfx_upscaler_vk.dll",
    "LICENSE-AMD-FSR-SDK.md",
)


def git_token() -> str:
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
    raise SystemExit("no token")


def copy(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def zip_dir(src_dir: Path, zip_path: Path) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for path in sorted(src_dir.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(src_dir).as_posix())
    print(f"zip {zip_path.name} ({zip_path.stat().st_size} bytes)")


def backup_sources() -> None:
    bzip = PREP / f"backup_{VER}_src.zip"
    if bzip.exists():
        bzip.unlink()
    src_dirs = ("host", "gpu/vk", "nx2", "launcher")
    with zipfile.ZipFile(bzip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for rel_d in src_dirs:
            d = ROOT / rel_d
            if not d.is_dir():
                continue
            for p in sorted(d.rglob("*")):
                if not p.is_file():
                    continue
                if any(part in {"build", "dist", "__pycache__", "eden", "sirit"} for part in p.parts):
                    continue
                if p.suffix.lower() in {".c", ".cpp", ".h", ".py", ".comp", ".spec"}:
                    zf.write(p, p.relative_to(ROOT).as_posix())
        if (ROOT / "VERSION").is_file():
            zf.write(ROOT / "VERSION", "VERSION")
    print(f"source backup {bzip.name} ({bzip.stat().st_size} bytes)")


def stage() -> None:
    if not (PREV / "portable").is_dir():
        raise SystemExit(f"missing {PREV}")
    for p in (BUILD_EXE, BUILD_VK, LAUNCHER, SHIPPED_STATES):
        if not p.is_file():
            raise SystemExit(f"missing {p}")
    if SHIPPED_STATES.stat().st_size < 10_000_000 or SHIPPED_STATES.read_bytes()[:8] not in (b"HKSTATE1", b"HKSTATE2"):
        raise SystemExit(f"invalid shipped pipeline state list: {SHIPPED_STATES}")
    for name in FFX_FILES:
        if not (FFX / name).is_file():
            raise SystemExit(f"missing FSR runtime {FFX / name} — run scripts/ffx_sdk.py --accept-amd-license")

    data = BUILD_EXE.read_bytes()
    if b"0.1.9c" not in data:
        raise SystemExit("mastersword.exe missing 0.1.9c stamp — rebuild first")
    for stale in (b"0.1.9b", b"0.1.9\x00", b"0.1.8\x00", b"0.1.7\x00", b"0.1.6\x00", b"0.1.5e", b"0.1.5d"):
        if stale in data:
            raise SystemExit(f"mastersword.exe still contains stale {stale!r} stamp")

    vk = BUILD_VK.read_bytes()
    if b"0.1.9c" not in vk:
        raise SystemExit("mastersword_vk.dll missing 0.1.9c stamp — rebuild vk")
    if b"Intel Hybrid CPU Patch" not in vk or b"Intel Hybrid CPU Patch" not in data:
        raise SystemExit("missing the Intel Hybrid CPU Patch (menu row in the DLL, its log line in the exe)")
    for stale in (b"0.1.9b", b"0.1.9\x00", b"0.1.8\x00", b"0.1.7\x00", b"0.1.6\x00", b"0.1.5e", b"0.1.5d"):
        if stale in vk:
            raise SystemExit(f"mastersword_vk.dll still contains stale {stale!r} stamp")
    if b"Left stick speed" not in vk or b"Apex FG (Fast) (recommended)" not in vk:
        raise SystemExit("mastersword_vk.dll missing 0.1.9 stick-speed or Apex recommended markers")
    if b"Field of view" not in vk:
        raise SystemExit("mastersword_vk.dll missing Field of view menu row")
    if b"AMD FSR 4" not in vk and b"amd_fidelityfx_loader_vk" not in vk:
        raise SystemExit("mastersword_vk.dll missing FSR 4 markers — rebuild with HOOKSHOT_FSR=1")
    if b"nvngx_dlss" not in vk or b"nvngx_dlssg" not in vk:
        raise SystemExit("mastersword_vk.dll missing NGX DLSS / DLSS-G markers — rebuild with HOOKSHOT_NGX_SDK")
    if b"descriptor slotgen ready" not in vk or b"sampler cache ready" not in vk:
        raise SystemExit("mastersword_vk.dll missing descriptor/sampler fast-path markers")
    if b"render targets' images from blocks of their own" not in vk:
        raise SystemExit("mastersword_vk.dll missing render-target pooling marker")
    if b"hookshot_pipeline_states_shipped.bin" not in vk:
        raise SystemExit("mastersword_vk.dll missing shipped pipeline state list marker")

    # Verify --probe-dpi passes (required by MasterSwordLauncher update verifier)
    probe = subprocess.run([str(BUILD_EXE), "--probe-dpi"], capture_output=True, text=True, timeout=15, check=False)
    probe_out = (probe.stdout or "") + "\n" + (probe.stderr or "")
    if probe.returncode != 0 or "probe-dpi: PASS (0.1.9c" not in probe_out:
        raise SystemExit(f"mastersword.exe --probe-dpi failed: rc={probe.returncode} out={probe_out!r}")

    # GOLDEN RULE CHECK 1: gpu/vk/vk_menu.h fresh-install Options struct defaults
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

    # GOLDEN RULE CHECK 2: MasterSwordLauncher.py version + safe 1080p vendor-neutral defaults
    launcher_src = ROOT / "launcher" / "MasterSwordLauncher.py"
    src = launcher_src.read_text(encoding="utf-8", errors="ignore")
    if 'CURRENT_VERSION = "0.1.9c"' not in src:
        raise SystemExit("MasterSwordLauncher.py not bumped to 0.1.9c")
    if "STS78721/OOT-MASTERSWORD" not in src:
        raise SystemExit("launcher source missing OOT-MASTERSWORD updater target")
    if "nvngx_dlssg.dll" not in src:
        raise SystemExit("launcher source missing nvngx_dlssg.dll NGX installer support")
    for required_ui_default in (
        '"output_res":"1920x1080"',
        '"upscaler":"off"',
        '"fg_mode":"off"',
        '"reflex":"off"',
        '"fullscreen":"0"',
        '"fov":"100"',
    ):
        if required_ui_default not in src:
            raise SystemExit(f"GOLDEN RULE VIOLATION in MasterSwordLauncher.py: missing {required_ui_default}")
    if LAUNCHER.stat().st_size < 1_000_000:
        raise SystemExit("MasterSwordLauncher.exe looks truncated")

    backup_sources()

    if STAGE.exists():
        shutil.rmtree(STAGE)
    notes = (PREP / "PATCH_NOTES_0.1.9c.md").read_text(encoding="utf-8")
    forbidden_names = {
        "mastersword_options.ini", "hookshot_options.ini",
        "mastersword_launcher.json", "launcher_config.json",
        "image.bin", "mkw_vk_pipelines.bin", "hookshot_programs.bin",
        "hookshot_prepared.bin", "hookshot_state_list_prepared.bin", "hookshot_computes.bin",
        "nvngx_dlss.dll", "nvngx_dlssg.dll", "nvngx_dlssd.dll", "_nvngx.dll",
    }
    forbidden_exts = {".ini", ".json", ".log", ".bak"}
    for kind in ("portable", "update"):
        shutil.copytree(PREV / kind, STAGE / kind)
        folder = STAGE / kind
        copy(BUILD_EXE, folder / "mastersword.exe")
        copy(BUILD_VK, folder / "mastersword_vk.dll")
        copy(BUILD_VK, folder / "hookshot_vk.dll")
        copy(LAUNCHER, folder / "MasterSwordLauncher.exe")
        copy(SHIPPED_STATES, folder / "hookshot_pipeline_states_shipped.bin")
        for name in FFX_FILES:
            copy(FFX / name, folder / name)
        (folder / "PATCH_NOTES.md").write_text(notes, encoding="utf-8")
        (folder / "mastersword_version.txt").write_text(f"version={VER}\n", encoding="utf-8")
        # GOLDEN RULE CHECK 3: Never ship personal .ini/.json settings, logs, shader caches, saves, dumps, or NVIDIA NGX binaries
        for p in folder.rglob("*"):
            low = p.name.lower()
            if (
                low in forbidden_names
                or p.suffix.lower() in forbidden_exts
                or low.startswith(("hookshot_log", "mastersword_log", "romfs", "user", "shader-cache", "sl."))
            ):
                raise SystemExit(f"GOLDEN RULE VIOLATION: personal/config/dump/proprietary file {p} found in staged {kind}!")

    (STAGE / "portable" / "READ ME.txt").write_text(
        "SteelTemple MasterSword 0.1.9c — Smooth play on hybrid CPUs\n\n"
        "Requires image.bin + romfs\\ beside mastersword.exe.\n"
        "Out of the box: 1920x1080 Windowed, Upscaling Off, Frame Generation Off, Reflex Off, Field of view 100%, V-Sync Mailbox.\n"
        "Intel Hybrid CPU Patch: On (Recommended for 12th/13th/14th gen Core and Core Ultra) in the Insert / F1 menu, Graphics tab.\n"
        "Tip: Win + G > Settings > General > Remember this is a game (with the game window focused) makes play smoother.\n"
        "Includes hookshot_pipeline_states_shipped.bin (41,839 pre-baked pipeline states) for zero-stutter gameplay.\n"
        "NVIDIA: DLSS SR (presets K/M/L + DLAA) + Hardware NGX DLSS MFG (2x/3x/4x) + Reflex.\n"
        "AMD: FSR 4 (amd_fidelityfx_*_vk.dll included) + Temporal FSR 3.1 SR & FG.\n"
        "Use SETTINGS → Download & Install DLSS SDK in MasterSwordLauncher.exe to install ngx\\nvngx_dlss.dll & ngx\\nvngx_dlssg.dll.\n",
        encoding="utf-8",
    )
    DIST.mkdir(parents=True, exist_ok=True)
    zip_dir(STAGE / "portable", DIST / f"SteelTemple-MasterSword-{VER}-Portable-StandAlone.zip")
    zip_dir(STAGE / "update", DIST / f"SteelTemple-MasterSword-{VER}-Update.zip")
    copy(LAUNCHER, DIST / "MasterSwordLauncher.exe")
    setup = ROOT / "dist" / "SteelTemple-MasterSword-Setup.exe"
    if setup.exists():
        copy(setup, DIST / "SteelTemple-MasterSword-Setup.exe")
    print(f"staged {VER} cleanly (all Golden Rule checks PASSED)")


def api(token, method, url, data=None, content_type=None):
    headers = {
        "User-Agent": "MasterSword-Release",
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
    }
    if content_type:
        headers["Content-Type"] = content_type
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            body = resp.read()
            return json.loads(body.decode()) if body else {}
    except urllib.error.HTTPError as e:
        err = e.read().decode(errors="replace")
        raise RuntimeError(f"{method} {url} -> {e.code}: {err}") from e


def ensure_release(token, body):
    payload = {
        "tag_name": TAG,
        "name": f"MasterSword {VER}",
        "body": body,
        "draft": False,
        "prerelease": False,
        "make_latest": "true",
    }
    try:
        return api(
            token,
            "POST",
            f"https://api.github.com/repos/{REPO}/releases",
            data=json.dumps(payload).encode(),
            content_type="application/json",
        )
    except RuntimeError as e:
        if "422" not in str(e) and "already_exists" not in str(e):
            raise
        rel = api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/tags/{TAG}")
        api(
            token,
            "PATCH",
            f"https://api.github.com/repos/{REPO}/releases/{rel['id']}",
            data=json.dumps({"name": payload["name"], "body": body, "make_latest": "true"}).encode(),
            content_type="application/json",
        )
        return api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/tags/{TAG}")


def upload_asset(token, upload_url, path, content_type):
    url = f"{upload_url}?name={path.name}"
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
                print(f"uploaded {path.name} ({len(data)} bytes)")
                return
        except urllib.error.HTTPError as e:
            err = e.read().decode(errors="replace")
            last = e
            if e.code == 422 and "already_exists" in err:
                rel = api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/tags/{TAG}")
                for asset in rel.get("assets", []):
                    if asset.get("name") == path.name:
                        api(token, "DELETE", f"https://api.github.com/repos/{REPO}/releases/assets/{asset['id']}")
                        break
                continue
            if e.code in (408, 502, 503, 504) and attempt < 2:
                print(f"upload {path.name} got {e.code}, retrying")
                continue
            raise RuntimeError(f"upload {path.name}: {e.code} {err}") from e
        except (TimeoutError, urllib.error.URLError) as e:
            last = e
            if attempt < 2:
                print(f"upload {path.name} stalled, retrying")
                continue
            raise
    raise RuntimeError(f"upload {path.name} failed") from last


def update_github_readme(token: str) -> None:
    import base64

    local = (PREP / "README_MASTERSWORD.md").read_text(encoding="utf-8")
    (ROOT / "VERSION").write_text(VER + "\n", encoding="utf-8")
    encoded = base64.b64encode(local.encode("utf-8")).decode("ascii")
    # GitHub has both casings. Each one is a separate file, and the site can show either.
    for name in ("README.md", "readme.md"):
        meta = api(token, "GET", f"https://api.github.com/repos/{REPO}/contents/{name}")
        if base64.b64decode(meta.get("content", "")).decode("utf-8", errors="replace") == local:
            print(f"GitHub {name} already current")
            continue
        payload = {
            "message": f"docs: MasterSword {VER} (smooth play on hybrid CPUs)",
            "content": encoded,
            "branch": "main",
            "sha": meta["sha"],
        }
        api(
            token,
            "PUT",
            f"https://api.github.com/repos/{REPO}/contents/{name}",
            data=json.dumps(payload).encode(),
            content_type="application/json",
        )
        print(f"updated GitHub {name}")


def upload() -> None:
    notes = (PREP / "PATCH_NOTES_0.1.9c.md").read_text(encoding="utf-8")
    token = git_token()
    update_github_readme(token)
    rel = ensure_release(token, notes)
    print(rel.get("html_url"))
    base = rel["upload_url"].split("{", 1)[0]
    for path, ctype in [
        (DIST / f"SteelTemple-MasterSword-{VER}-Portable-StandAlone.zip", "application/zip"),
        (DIST / f"SteelTemple-MasterSword-{VER}-Update.zip", "application/zip"),
        (DIST / "MasterSwordLauncher.exe", "application/octet-stream"),
    ]:
        upload_asset(token, base, path, ctype)
    setup = DIST / "SteelTemple-MasterSword-Setup.exe"
    if setup.exists():
        upload_asset(token, base, setup, "application/octet-stream")
    print("done", rel.get("html_url"))


def upload_assets_only() -> None:
    token = git_token()
    rel = api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/tags/{TAG}")
    print(rel.get("html_url"))
    base = rel["upload_url"].split("{", 1)[0]
    for path, ctype in [
        (DIST / f"SteelTemple-MasterSword-{VER}-Portable-StandAlone.zip", "application/zip"),
        (DIST / f"SteelTemple-MasterSword-{VER}-Update.zip", "application/zip"),
        (DIST / "MasterSwordLauncher.exe", "application/octet-stream"),
    ]:
        upload_asset(token, base, path, ctype)
    print("done", rel.get("html_url"))


def main():
    if "--assets-only" in sys.argv:
        upload_assets_only()
        return
    stage()
    if "--upload" in sys.argv:
        upload()
    else:
        print("stop before upload — pass --upload when ready to publish")


if __name__ == "__main__":
    main()
