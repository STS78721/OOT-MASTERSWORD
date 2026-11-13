"""The Legend of Zelda: Ocarina of Time - PC Recompiled (SteelTemple MasterSword Edition)
Standalone Sony/Nixxes-style Launcher & Settings Manager — SteelTempSaint (STS78721).
"""
from __future__ import annotations
import base64, ctypes, hashlib, json, os, re, shutil, subprocess, sys, tempfile, threading, time, urllib.request, zipfile
from datetime import datetime
from pathlib import Path
from ctypes import wintypes
import webbrowser
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageDraw, ImageTk

CURRENT_VERSION = "0.1.9c"
EDITION_NAME = "SteelTemple MasterSword Edition"
APP_TITLE = "The Legend of Zelda: Ocarina of Time - PC Recompiled"
VERSION_STRING = f"V {CURRENT_VERSION}"
GITHUB_URL = "https://github.com/STS78721/OOT-MASTERSWORD"
RELEASES_URL = GITHUB_URL + "/releases"
RELEASES_API_URL = "https://api.github.com/repos/STS78721/OOT-MASTERSWORD/releases/latest"
COLOR_BG_DARK = "#0a0c10"
COLOR_GOLD = "#ffd24d"
COLOR_GOLD_HOVER = "#fff2a3"
COLOR_WHITE = "#ffffff"
COLOR_TEXT_DIM = "#9ba3af"
COLOR_CARD = "#101820"
COLOR_CARD_BORDER = "#2a3d32"
COLOR_ACCENT_GREEN = "#3ecf8e"
COLOR_TAB_ACTIVE = "#1a2a22"
COLOR_BUTTON_HOVER = "#243428"
COLOR_TRACK_BG = "#263040"
COLOR_TRACK_ACTIVE = COLOR_GOLD
MOD_DISABLED_SUFFIX = ".disabled"


def resource_dir():
    return Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))


def app_dir():
    return Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent


def find_asset(filename):
    candidates = [
        resource_dir() / "assets" / filename,
        app_dir() / "assets" / filename,
        Path.cwd() / "assets" / filename,
        resource_dir() / filename,
        app_dir() / filename,
        Path.cwd() / "launcher" / "assets" / filename,
        Path(__file__).resolve().parent / "assets" / filename,
    ]
    for c in candidates:
        if c.is_file(): return c
    return None


def open_folder(path):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    try:
        if os.name == "nt": os.startfile(str(path))
        elif sys.platform == "darwin": subprocess.Popen(["open", str(path)])
        else: subprocess.Popen([shutil.which("xdg-open") or "xdg-open", str(path)])
    except (OSError, subprocess.SubprocessError): pass


def play_ui_sound(kind):
    if not getattr(play_ui_sound, "enabled", True): return
    try:
        import winsound
        sound = find_asset("hover.wav" if kind == "hover" else "click.wav")
        if sound: winsound.PlaySound(str(sound), winsound.SND_FILENAME | winsound.SND_ASYNC | winsound.SND_NODEFAULT)
    except Exception: pass


class _RECT(ctypes.Structure):
    _fields_ = [("left", wintypes.LONG), ("top", wintypes.LONG), ("right", wintypes.LONG), ("bottom", wintypes.LONG)]
class _MONITORINFOEX(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("rcMonitor", _RECT), ("rcWork", _RECT), ("dwFlags", wintypes.DWORD), ("szDevice", wintypes.WCHAR * 32)]
class _DEVMODE(ctypes.Structure):
    _fields_ = [("dmDeviceName", wintypes.WCHAR * 32), ("dmSpecVersion", wintypes.WORD), ("dmDriverVersion", wintypes.WORD),
                ("dmSize", wintypes.WORD), ("dmDriverExtra", wintypes.WORD), ("dmFields", wintypes.DWORD),
                ("dmOrientation", ctypes.c_short), ("dmPaperSize", ctypes.c_short), ("dmPaperLength", ctypes.c_short),
                ("dmPaperWidth", ctypes.c_short), ("dmScale", ctypes.c_short), ("dmCopies", ctypes.c_short),
                ("dmDefaultSource", ctypes.c_short), ("dmPrintQuality", ctypes.c_short), ("dmColor", ctypes.c_short),
                ("dmDuplex", ctypes.c_short), ("dmYResolution", ctypes.c_short), ("dmTTOption", ctypes.c_short),
                ("dmCollate", ctypes.c_short), ("dmFormName", wintypes.WCHAR * 32), ("dmLogPixels", wintypes.WORD),
                ("dmBitsPerPel", wintypes.DWORD), ("dmPelsWidth", wintypes.DWORD), ("dmPelsHeight", wintypes.DWORD),
                ("dmDisplayFlags", wintypes.DWORD), ("dmDisplayFrequency", wintypes.DWORD)]


def enumerate_displays():
    displays = []
    if os.name != "nt": return [{"index":1,"device":"PRIMARY","resolution":"1920x1080","refresh":60,"left":0,"top":0,"monitor_width":1920,"monitor_height":1080,"primary":True}]
    try:
        user32 = ctypes.windll.user32
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMONITOR, wintypes.HDC, ctypes.POINTER(_RECT), wintypes.LPARAM)
        user32.GetMonitorInfoW.argtypes = [wintypes.HMONITOR, ctypes.POINTER(_MONITORINFOEX)]
        user32.EnumDisplaySettingsW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, ctypes.POINTER(_DEVMODE)]
        def visit(handle, _dc, _rect, _data):
            info = _MONITORINFOEX(); info.cbSize = ctypes.sizeof(info)
            if not user32.GetMonitorInfoW(handle, ctypes.byref(info)): return True
            mode = _DEVMODE(); mode.dmSize = ctypes.sizeof(mode)
            user32.EnumDisplaySettingsW(info.szDevice, -1, ctypes.byref(mode))
            width = mode.dmPelsWidth or info.rcMonitor.right-info.rcMonitor.left
            height = mode.dmPelsHeight or info.rcMonitor.bottom-info.rcMonitor.top
            displays.append({"index":len(displays)+1,"device":info.szDevice,"resolution":f"{width}x{height}",
                "refresh":int(mode.dmDisplayFrequency or 60),"left":int(info.rcMonitor.left),"top":int(info.rcMonitor.top),
                "monitor_width":int(info.rcMonitor.right-info.rcMonitor.left),"monitor_height":int(info.rcMonitor.bottom-info.rcMonitor.top),
                "primary":bool(info.dwFlags & 1)})
            return True
        user32.EnumDisplayMonitors(None,None,callback_type(visit),0)
    except Exception: pass
    if not displays: displays=[{"index":1,"device":"PRIMARY","resolution":"1920x1080","refresh":60,"left":0,"top":0,"monitor_width":1920,"monitor_height":1080,"primary":True}]
    return displays


def place_window_on_monitor(window, monitor):
    window.update_idletasks(); w,h=window.winfo_width(),window.winfo_height()
    left=monitor["left"]+max(0,(monitor["monitor_width"]-w)//2); top=monitor["top"]+max(0,(monitor["monitor_height"]-h)//2)
    window.geometry(f"{w}x{h}+{left}+{top}")


def classify_gpu_name(name):
    low=name.lower()
    vendor="NVIDIA" if any(v in low for v in ("nvidia","geforce")) else "AMD" if any(v in low for v in ("amd","radeon")) else "Intel" if "intel" in low else "Other"
    return name,vendor

def detect_gpu():
    if hasattr(detect_gpu,"result"):return detect_gpu.result
    if os.name != "nt": result=("GPU detection is available on Windows", "Other")
    else:
        try:
            cmd="Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name | ConvertTo-Json -Compress"
            r=subprocess.run(["powershell.exe","-NoProfile","-NonInteractive","-Command",cmd],capture_output=True,text=True,timeout=8,
                             creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
            names=json.loads(r.stdout.strip()) if r.returncode==0 and r.stdout.strip() else []
            if isinstance(names,str):names=[names]
            if names:
                names.sort(key=lambda n:(not any(v in n.lower() for v in ("nvidia","geforce","amd","radeon")),n))
                result=classify_gpu_name(names[0])
            else:result=("GPU not detected","Other")
        except Exception:result=("GPU not detected","Other")
    detect_gpu.result=result
    return result


APP_CONFIG = app_dir() / "mastersword_launcher.json"
LEGACY_APP_CONFIG = app_dir() / "launcher_config.json"

def load_config():
    for path in (APP_CONFIG, LEGACY_APP_CONFIG):
        try:
            cfg = json.loads(path.read_text(encoding="utf-8"))
            cfg.setdefault("show_launcher", True)
            cfg.setdefault("launcher_sound_effects", True)
            cfg.setdefault("auto_install_updates", True)
            return cfg
        except Exception:
            pass
    return {"show_launcher": True, "launcher_sound_effects": True, "auto_install_updates": True}

def save_config(cfg):
    APP_CONFIG.parent.mkdir(parents=True,exist_ok=True)
    tmp=APP_CONFIG.with_suffix(".tmp"); tmp.write_text(json.dumps(cfg,indent=2),encoding="utf-8"); tmp.replace(APP_CONFIG)

def valid_game_dir(path):
    if not path: return False
    p = Path(path)
    if not p.is_dir(): return False
    return (p / "mastersword.exe").is_file() or (p / "build" / "oot" / "mastersword.exe").is_file()

def find_game_dir(cfg):
    choices=[]
    if cfg.get("game_dir"): choices.append(Path(cfg["game_dir"]))
    if os.environ.get("MASTER_SWORD_GAME_DIR"): choices.append(Path(os.environ["MASTER_SWORD_GAME_DIR"]))
    a=app_dir(); choices += [a, Path.cwd(), a/"build"/"oot", a/"jogo"/"build"/"oot", a.parent/"build"/"oot", Path.cwd()/"build"/"oot"]
    for path in choices:
        if valid_game_dir(path):
            res = path.resolve()
            if (res / "build" / "oot" / "mastersword.exe").is_file() and not (res / "mastersword.exe").is_file():
                res = (res / "build" / "oot").resolve()
            return res
    return None

GAME_DIR = None
INI_PATH = None
PROFILE_DIR = None
MODS_DIR = None
ACTIVE_MODS_DIR = None
LOGS_DIR = None
SHADER_CACHE_DIR = None
LAUNCHER_CONFIG = load_config()

def settings_ini_path(game_dir=None):
    """Game folder ini when known; otherwise launcher-local settings (usable outside the game)."""
    root = Path(game_dir).resolve() if game_dir else (GAME_DIR.resolve() if GAME_DIR else app_dir())
    return root / "mastersword_options.ini"

def set_game_dir(path):
    global GAME_DIR, INI_PATH, PROFILE_DIR, MODS_DIR, ACTIVE_MODS_DIR, LOGS_DIR, SHADER_CACHE_DIR
    GAME_DIR=Path(path).resolve()
    INI_PATH=settings_ini_path(GAME_DIR)
    PROFILE_DIR=Path(LAUNCHER_CONFIG.get("profile_dir") or (GAME_DIR.parent/"profile-oot"))
    if not PROFILE_DIR.exists() and (GAME_DIR.parent/"profile-oot").exists(): PROFILE_DIR=GAME_DIR.parent/"profile-oot"
    if not PROFILE_DIR.exists() and (GAME_DIR/"profile").exists(): PROFILE_DIR=GAME_DIR/"profile"
    MODS_DIR=GAME_DIR/"mods"
    ACTIVE_MODS_DIR=GAME_DIR/"romfs-mods-active"
    LOGS_DIR=GAME_DIR/"logs"
    SHADER_CACHE_DIR=GAME_DIR/"shader-cache"


# NVIDIA NGX DLSS & DLSS-G runtimes. Placed under <game>/ngx/ where vk_dlss.cpp looks first.
# Fetched directly from NVIDIA's official DLSS repo; never Streamline interposer.
NVIDIA_DLSS_SDK_FILES = [
    ("ngx/nvngx_dlss.dll", "NVIDIA DLSS Super Resolution (NGX)"),
    ("ngx/nvngx_dlssg.dll", "NVIDIA DLSS Frame Generation (NGX)"),
]
NGX_SDK_VERSION = "v310.9.16"
NGX_SDK_COMMIT = "9981c6350dc58c0ce21f024fd65e31e851c31689"
NGX_SDK_BASE_URL = f"https://raw.githubusercontent.com/NVIDIA/DLSS/{NGX_SDK_COMMIT}"
NGX_SDK_PINNED_FILES = (
    ("LICENSE.txt", "LICENSE.txt", "d4216e39ebef5f9b50a6712ebb37beeb5379862a67733a9999c651f21592aaf0"),
    ("lib/Windows_x86_64/rel/nvngx_dlss.dll", "runtime/nvngx_dlss.dll", "d00b5b0ab29c3ca101b70c3a5ecb3a01b7ef33766a3383a28a18623083e6123e"),
    ("lib/Windows_x86_64/rel/nvngx_dlssg.dll", "runtime/nvngx_dlssg.dll", "8d59d49b36410dd6b62fef2503a5871d347415bd85d943fa0848563a95dd50c7"),
)
NGX_SDK_HINT = f"NVIDIA DLSS {NGX_SDK_VERSION} (NGX)"


def nvidia_sdk_cache_dir() -> Path:
    base = Path(os.environ.get("LOCALAPPDATA") or (Path.home() / "AppData" / "Local"))
    return base / "MasterSword" / "sdk_cache"


def nvidia_dlss_sdk_sources():
    """Folders that may already hold redistributable Streamline / nvngx DLLs."""
    home = Path(os.environ.get("USERPROFILE", ""))
    cands = [
        nvidia_sdk_cache_dir() / "bin",
        home / "Desktop" / "Apps" / "nvidiaDlssGlom",
        app_dir() / "sdk" / "nvidia",
        app_dir() / "nvidia_sdk",
        app_dir(),
        Path(r"C:\Games\Hookshot - NotRexed - Test e400f88") / "runs" / "_release_prep" / "stage_0.1.3c" / "portable",
        Path(r"C:\Games\Ocarina of Time PC") / "Hookshot Test - smoke_0.1.3b",
        Path(r"C:\Games\Tears of the Kingdom PC") / "GAME",
    ]
    if GAME_DIR:
        cands.insert(0, GAME_DIR)
    out = []
    for p in cands:
        try:
            if p and p.is_dir():
                out.append(p.resolve())
        except OSError:
            pass
    seen, uniq = set(), []
    for p in out:
        if p not in seen:
            seen.add(p)
            uniq.append(p)
    return uniq


def find_nvidia_dlss_file(name):
    leaf = Path(name).name
    for folder in nvidia_dlss_sdk_sources():
        for hit in (folder / name, folder / leaf, folder / "ngx" / leaf, folder / "runtime" / leaf):
            if hit.is_file() and hit.stat().st_size > 1024:
                return hit
    return None


def nvidia_dlss_status(game_dir=None):
    root = Path(game_dir) if game_dir else GAME_DIR
    if not root:
        return 0, len(NVIDIA_DLSS_SDK_FILES), []
    present, missing = 0, []
    for name, _desc in NVIDIA_DLSS_SDK_FILES:
        if (root / name).is_file():
            present += 1
        else:
            missing.append(name)
    return present, len(NVIDIA_DLSS_SDK_FILES), missing


def download_url_with_progress(url: str, dest: Path, progress_cb=None, expected_size: int = 0):
    """Download url → dest with optional progress_cb(done_bytes, total_bytes, label)."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    partial = dest.with_suffix(dest.suffix + ".partial")
    req = urllib.request.Request(url, headers={"User-Agent": "MasterSwordLauncher/" + CURRENT_VERSION})
    with urllib.request.urlopen(req, timeout=120) as resp:
        total = int(resp.headers.get("Content-Length") or 0) or int(expected_size or 0)
        done = 0
        chunk = 1024 * 256
        with open(partial, "wb") as out:
            while True:
                buf = resp.read(chunk)
                if not buf:
                    break
                out.write(buf)
                done += len(buf)
                if progress_cb:
                    progress_cb(done, total or done, dest.name)
    partial.replace(dest)
    if progress_cb:
        progress_cb(dest.stat().st_size, dest.stat().st_size, dest.name)
    return dest


def extract_streamline_sdk_dlls(zip_path: Path, out_dir: Path, progress_cb=None):
    """Pull production bin/x64 DLLs from the official Streamline SDK zip into out_dir."""
    out_dir.mkdir(parents=True, exist_ok=True)
    wanted = {name for name, _ in NVIDIA_DLSS_SDK_FILES}
    extracted = []
    with zipfile.ZipFile(zip_path, "r") as zf:
        # Prefer bin/x64/<dll> over bin/x64/development/<dll>
        members = {}
        for info in zf.infolist():
            name = Path(info.filename.replace("\\", "/")).name.lower()
            if name not in wanted or not name.endswith(".dll"):
                continue
            norm = info.filename.replace("\\", "/").lower()
            if "/development/" in norm:
                continue
            if "/bin/x64/" not in norm and not norm.startswith("bin/x64/"):
                continue
            # Keep the shortest path under bin/x64 (production root)
            prev = members.get(name)
            if prev is None or len(info.filename) < len(prev.filename):
                members[name] = info
        total = max(1, len(members))
        for i, (name, info) in enumerate(sorted(members.items())):
            target = out_dir / Path(info.filename.replace("\\", "/")).name
            if progress_cb:
                progress_cb(i, total, f"Extracting {target.name}")
            with zf.open(info) as src, open(target, "wb") as dst:
                shutil.copyfileobj(src, dst, length=1024 * 256)
            extracted.append(target.name)
            if progress_cb:
                progress_cb(i + 1, total, f"Extracted {target.name}")
    return extracted


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def ensure_nvidia_dlss_sdk_cache(progress_cb=None, force_download: bool = False):
    """
    Ensure LOCALAPPDATA/MasterSword/sdk_cache/ngx-sdk has NVIDIA's pinned NGX DLSS runtime
    (v310.9.16, SHA-256 verified directly over HTTPS — works in frozen MasterSwordLauncher.exe
    with no Python or build repo required) and return the folder holding nvngx_dlss.dll.
    progress_cb(fraction_0_1, message)
    """
    cache = nvidia_sdk_cache_dir() / "ngx-sdk"
    runtime_sr = cache / "runtime" / "nvngx_dlss.dll"
    runtime_fg = cache / "runtime" / "nvngx_dlssg.dll"

    def report(frac, msg):
        if progress_cb:
            progress_cb(max(0.0, min(1.0, frac)), msg)

    if (runtime_sr.is_file() and runtime_sr.stat().st_size > 1024
            and runtime_fg.is_file() and runtime_fg.stat().st_size > 1024
            and not force_download):
        report(1.0, "Using cached NVIDIA NGX DLSS & DLSS-G runtimes")
        return cache / "runtime"

    cache.mkdir(parents=True, exist_ok=True)
    for idx, (remote_rel, local_rel, expected_sha) in enumerate(NGX_SDK_PINNED_FILES):
        dst = cache / Path(local_rel)
        if dst.is_file() and not force_download and _sha256_file(dst) == expected_sha:
            continue
        url = f"{NGX_SDK_BASE_URL}/{remote_rel}"
        base_frac = 0.05 if idx == 0 else (0.10 if idx == 1 else 0.85)
        span_frac = 0.05 if idx == 0 else (0.75 if idx == 1 else 0.15)
        report(base_frac, f"Downloading {dst.name} ({NGX_SDK_VERSION})…")

        def on_bytes(done, total, label, b=base_frac, s=span_frac):
            frac = b + s * (done / max(1, total))
            mb_done = done / (1024 * 1024)
            mb_tot = total / (1024 * 1024)
            report(frac, f"Downloading {label}… {mb_done:.1f} / {mb_tot:.1f} MB")

        download_url_with_progress(url, dst, progress_cb=on_bytes)
        if _sha256_file(dst) != expected_sha:
            dst.unlink(missing_ok=True)
            raise RuntimeError(f"SHA-256 mismatch for downloaded {dst.name}")

    if not runtime_sr.is_file() or not runtime_fg.is_file():
        raise RuntimeError("NGX SDK download did not produce runtime/nvngx_dlss.dll and runtime/nvngx_dlssg.dll")
    report(1.0, "NGX SDK cache ready")
    return cache / "runtime"



# Keys the game Insert menu / host read as flat integers beside mastersword.exe.
# Names match vk_menu.cpp ini_key() (row title → snake_case) plus a few aliases.
GAME_FLAT_KEYS = {
    "fullscreen", "output_res", "output_resolution", "sharpness", "ultrawide", "resolution",
    "fps_counter", "game_speed", "realtime", "handheld_profile", "robust_buffers", "gpu_device",
    "upscaling", "dlss_quality", "fsr_4_quality", "fsr_quality", "nodix_quality", "xess_quality", "smaa",
    "fg_engine", "fg_backend", "frame_generation", "fg_model", "framegen_preset", "frame_gen_preset",
    "flow_scale", "fg_flow", "frame_pacing", "reflex", "options_version",
    "descriptor_reuse", "sampler_cache", "vsync", "frame_rate_cap", "mesh_shaders",
    "colour_profile", "brightness", "contrast", "saturation", "gamma",
    "layout_preset", "mouse_sensitivity", "controller_in_background", "field_of_view",
}
PAD_LAYOUT_TO_GAME = {"auto": 0, "nintendo": 0, "xbox": 1, "xbox_prompts": 1, "swap": 2, "custom": 3}
PAD_LAYOUT_FROM_GAME = {0: "nintendo", 1: "xbox", 2: "swap", 3: "custom"}

# Launcher UI labels ↔ Insert-menu integers (gpu/vk/vk_menu.h Options).
UPSCALER_TO_GAME = {
    "off": 0, "catmull": 1, "catmull-rom": 1, "lanczos": 2, "bilinear": 2,
    "xess": 3, "fsr3": 4, "fsr31": 4, "fsr_3.1": 4, "fsr": 5, "fsr1": 5,
    "smrs": 6, "smai": 7, "dlss": 8, "fsr4": 9, "fsr_4": 9,
}
UPSCALER_FROM_GAME = {
    0: "off", 1: "catmull", 2: "lanczos", 3: "xess", 4: "fsr3", 5: "fsr", 6: "smrs", 7: "smai", 8: "dlss", 9: "fsr4",
}
FG_MODE_TO_GAME = {
    "off": 0, "mastersword": 1, "msm": 1, "hookshot": 1,
    "msm_fg": 2, "smfg": 2, "nodix": 2,
    "dlss_fg": 3, "dlss": 3,
    "fsr3_fg": 4, "fsr_fg": 4,
    "xefg": 5, "xess_fg": 5,
}
FG_MODE_FROM_GAME = {0: "off", 1: "mastersword", 2: "msm_fg", 3: "dlss_fg", 4: "fsr3_fg", 5: "xefg"}
PRESET_TO_DLSS = {
    "native": 4, "dlaa": 4, "ultra_quality": 0, "quality": 0, "balanced": 1,
    "performance": 2, "ultra_performance": 3,
}
PRESET_FROM_DLSS = {0: "quality", 1: "balanced", 2: "performance", 3: "ultra_performance", 4: "native"}
# (v8 / #2147) FSR 3.1 / 1.0 quality: 0=Native AA 100%, 1=92%, 2=83%, 3=75%, 4=Quality 67%, 5=Balanced 58%, 6=Performance 50%, 7=Ultra Performance 33%
PRESET_TO_FSR = {
    "native": 0, "dlaa": 0, "ultra_quality": 3, "quality": 4, "balanced": 5,
    "performance": 6, "ultra_performance": 7,
}
PRESET_FROM_FSR = {
    0: "native", 1: "native", 2: "ultra_quality", 3: "ultra_quality",
    4: "quality", 5: "balanced", 6: "performance", 7: "ultra_performance",
}
PRESET_TO_NODIX = {"quality": 0, "ultra_quality": 0, "balanced": 1, "performance": 2, "ultra_performance": 2, "native": 0}
PRESET_FROM_NODIX = {0: "quality", 1: "balanced", 2: "performance"}
PRESET_TO_XESS = {
    "native": 0, "ultra_quality": 2, "quality": 3, "balanced": 4, "performance": 5, "ultra_performance": 6,
}
PRESET_FROM_XESS = {0: "native", 1: "ultra_quality", 2: "ultra_quality", 3: "quality", 4: "balanced", 5: "performance", 6: "ultra_performance"}
RENDER_SCALE_TO_RESOLUTION = {
    "game": 0, "auto": 0, "100": 1, "75": 2, "67": 3, "50": 4,
}
RESOLUTION_TO_RENDER_SCALE = {0: "game", 1: "100", 2: "75", 3: "67", 4: "50"}
FG_MULT_TO_GAME = {"off": 0, "0": 0, "1": 0, "2x": 2, "2": 2, "3x": 3, "3": 3, "4x": 4, "4": 4, "5x": 5, "5": 5, "6x": 6, "6": 6, "7x": 7, "7": 7, "8x": 8, "8": 8}
FG_MODEL_TO_GAME = {"a": 0, "a (original)": 0, "0": 0, "b": 1, "b (ui)": 1, "1": 1, "c": 2, "c (performance)": 2, "2": 2}
FG_MODEL_FROM_GAME = {0: "A", 1: "B", 2: "C"}
# Game output_res: 0 = 1080p, 1 = 1440p, 2 = 4K.
OUTPUT_RES_ENUM = {0: "1920x1080", 1: "2560x1440", 2: "3840x2160"}
OUTPUT_RES_ALIAS = {
    "1920x1080": 0, "2560x1440": 1, "3840x2160": 2,
    "2048x1152": 0, "1280x720": 0, "1280x800": 0, "1600x900": 0,
    "3200x1800": 2, "3840x1600": 2, "5120x2160": 2, "5120x1440": 2, "3440x1440": 1,
}

def parse_ini(path):
    values={}
    if not path or not Path(path).is_file(): return values
    try:
        for line in Path(path).read_text(encoding="utf-8",errors="ignore").splitlines():
            s=line.strip()
            if not s or s.startswith(("#",";","[")) or "=" not in s: continue
            k,v=s.split("=",1); values[k.strip().lower()]=v.split("#")[0].split(";")[0].strip()
    except OSError: pass
    return values

def config_text_value(value):
    value=str(value).strip()
    if value.casefold() in {"true","on"}: return "1"
    if value.casefold() in {"false","off"}: return "0"
    return value

def output_res_to_enum(value):
    """Map launcher WxH / enum strings to the game's output_res 0/1/2 (1080p / 1440p / 4K)."""
    s = str(value or "").strip().lower().replace(" ", "")
    if s in {"0", "1", "2"}: return int(s)
    if s in OUTPUT_RES_ALIAS: return OUTPUT_RES_ALIAS[s]
    try:
        w, h = [int(p) for p in s.lower().split("x", 1)]
        if h >= 2000 or w >= 3500: return 2
        if h >= 1300 or w >= 2400: return 1
        return 0
    except Exception:
        return 0

def output_res_to_ui(value, fallback="1920x1080"):
    """Map game enum or WxH into a launcher dropdown label."""
    s = str(value or "").strip()
    if not s: return fallback
    if "x" in s.lower(): return s.replace(" ", "")
    try:
        n = int(s)
        return OUTPUT_RES_ENUM.get(n, fallback)
    except ValueError:
        return fallback

def _int_or(value, default=0):
    try:
        return int(float(str(value).strip()))
    except (TypeError, ValueError):
        return default


def launcher_ui_from_game_ini(out):
    """Populate launcher dropdown keys from Insert-menu integers so Settings mirrors in-game."""
    if "upscaling" in out:
        out["upscaler"] = UPSCALER_FROM_GAME.get(_int_or(out["upscaling"]), out.get("upscaler", "off"))
    up = str(out.get("upscaler", "off")).casefold()
    opt_ver = _int_or(out.get("options_version", 8), 8)
    if up == "dlss" and "dlss_quality" in out:
        out["upscaler_preset"] = PRESET_FROM_DLSS.get(_int_or(out["dlss_quality"]), out.get("upscaler_preset", "quality"))
    elif up == "fsr4" and ("fsr_4_quality" in out or "dlss_quality" in out):
        q = _int_or(out.get("fsr_4_quality", out.get("dlss_quality", 1)))
        out["upscaler_preset"] = PRESET_FROM_DLSS.get(q, out.get("upscaler_preset", "quality"))
    elif up in {"fsr", "fsr3", "fsr31"} and "fsr_quality" in out:
        fq = _int_or(out["fsr_quality"])
        if 0 < opt_ver < 8 and 1 <= fq <= 4:
            fq += 3
        out["upscaler_preset"] = PRESET_FROM_FSR.get(fq, out.get("upscaler_preset", "quality"))
    elif up in {"smrs", "smai"} and "nodix_quality" in out:
        out["upscaler_preset"] = PRESET_FROM_NODIX.get(_int_or(out["nodix_quality"]), out.get("upscaler_preset", "quality"))
    elif up == "xess" and "xess_quality" in out:
        out["upscaler_preset"] = PRESET_FROM_XESS.get(_int_or(out["xess_quality"]), out.get("upscaler_preset", "quality"))

    eng = out.get("fg_engine", out.get("fg_backend"))
    if eng is not None and str(eng).strip() != "":
        out["fg_mode"] = FG_MODE_FROM_GAME.get(_int_or(eng), out.get("fg_mode", "off"))
    fg = out.get("frame_generation")
    if fg is not None and str(fg).strip() != "":
        n = _int_or(fg, 0)
        out["fg_mult"] = "off" if n < 2 else f"{n}x"
        out["frame_gen"] = "0" if n < 2 else "1"
    model = out.get("fg_model", out.get("framegen_preset", out.get("frame_gen_preset")))
    if model is not None and str(model).strip() != "":
        out["fg_preset"] = FG_MODEL_FROM_GAME.get(_int_or(model), out.get("fg_preset", "B"))
    pace = out.get("frame_pacing")
    if pace is not None and str(pace).strip() != "":
        out["fg_type"] = "adaptive" if _int_or(pace) else "fixed"
    if "flow_scale" in out and "fg_flow" not in out:
        out["fg_flow"] = out["flow_scale"]
    if "resolution" in out:
        out["render_scale"] = RESOLUTION_TO_RENDER_SCALE.get(_int_or(out["resolution"]), out.get("render_scale", "67"))
    if "fps_counter" in out and "perf_overlay" not in out:
        out["perf_overlay"] = "fps" if _int_or(out["fps_counter"]) else "off"
    if "handheld_profile" in out and "operation_mode" not in out:
        out["operation_mode"] = "handheld" if _int_or(out["handheld_profile"]) else "docked"
    layout = out.get("layout_preset")
    if layout is not None and str(layout).strip() != "":
        out["pad_layout"] = PAD_LAYOUT_FROM_GAME.get(_int_or(layout), out.get("pad_layout", "nintendo"))
    if "mouse_sensitivity" in out and "mouse_sens" not in out:
        try:
            ms = float(out["mouse_sensitivity"])
            out["mouse_sens"] = str(ms / 100.0 if ms > 10 else ms)
        except ValueError:
            out["mouse_sens"] = out["mouse_sensitivity"]
    if "field_of_view" in out:
        out["fov"] = str(max(50, min(150, _int_or(out["field_of_view"], 100))))
    # Insert menu V-Sync: 0 Mailbox, 1 On (FIFO), 2 Off. Missing key is Mailbox, the build default.
    if "vsync" in out and str(out["vsync"]).strip() != "":
        out["vk_present"] = {0: "mailbox", 1: "fifo", 2: "immediate"}.get(_int_or(out["vsync"]), "mailbox")
    elif not str(out.get("vk_present", "")).strip():
        out["vk_present"] = "mailbox"
    return out


def game_keys_from_launcher_ui(updates):
    """Translate SETTINGS dropdowns into the flat keys vk_menu.cpp actually loads."""
    out = dict(updates or {})
    up = str(out.get("upscaler", "off")).strip().casefold()
    out["upscaling"] = str(UPSCALER_TO_GAME.get(up, 0))
    preset = str(out.get("upscaler_preset", "quality")).strip().casefold()
    if up == "dlss":
        out["dlss_quality"] = str(PRESET_TO_DLSS.get(preset, 1))
    elif up in {"fsr4", "fsr_4"}:
        out["dlss_quality"] = str(PRESET_TO_DLSS.get(preset, 1))
        out["fsr_4_quality"] = out["dlss_quality"]
    elif up in {"fsr", "fsr3", "fsr31", "fsr_3.1", "fsr1"}:
        out["fsr_quality"] = str(PRESET_TO_FSR.get(preset, 5))
    elif up in {"smrs", "smai"}:
        out["nodix_quality"] = str(PRESET_TO_NODIX.get(preset, 1))
    elif up == "xess":
        out["xess_quality"] = str(PRESET_TO_XESS.get(preset, 3))

    fg_mode = str(out.get("fg_mode", "off")).strip().casefold()
    eng = FG_MODE_TO_GAME.get(fg_mode, 0)
    out["fg_engine"] = str(eng)
    out["fg_backend"] = str(eng)  # older Nodix alias still accepted on load
    mult_raw = str(out.get("fg_mult", "2x")).strip().casefold()
    mult = 0 if eng == 0 or fg_mode == "off" else FG_MULT_TO_GAME.get(mult_raw, 2)
    # Per engine: MasterSword 8, Nodix ×2–×6 (including ×5), DLSS 6, FSR 2, XeSS 4. 7x is nobody's step.
    fg_hi = {1: 8, 2: 6, 3: 6, 4: 2, 5: 4}.get(eng, 0)
    if eng > 0 and mult < 2:
        mult = 2
    if mult == 7:
        mult = 6
    if eng != 2 and mult == 5:
        mult = 4
    if eng > 0 and mult > fg_hi:
        mult = fg_hi
    out["frame_generation"] = str(mult)
    out["frame_gen"] = "0" if mult < 2 else "1"
    model = str(out.get("fg_preset", out.get("fg_model", "B"))).strip().casefold()
    out["fg_model"] = str(FG_MODEL_TO_GAME.get(model, 1))
    out["framegen_preset"] = out["fg_model"]
    fg_type = str(out.get("fg_type", "fixed")).strip().casefold()
    out["frame_pacing"] = "1" if fg_type in {"adaptive", "1", "on"} else "0"
    if "fg_flow" in out:
        out["flow_scale"] = str(_int_or(out["fg_flow"], 35))

    scale = str(out.get("render_scale", "67")).strip().casefold()
    if scale in RENDER_SCALE_TO_RESOLUTION:
        out["resolution"] = str(RENDER_SCALE_TO_RESOLUTION[scale])
    # Quality-owned scales (58/33/…) leave Resolution alone; the quality row owns % of Output.

    overlay = str(out.get("perf_overlay", "off")).strip().casefold()
    out["fps_counter"] = "0" if overlay in {"off", "0"} else "1"
    op = str(out.get("operation_mode", out.get("device_profile", "docked"))).strip().casefold()
    out["handheld_profile"] = "1" if op in {"handheld", "deck"} else "0"
    pad = str(out.get("pad_layout", "nintendo")).strip().casefold()
    out["layout_preset"] = str(PAD_LAYOUT_TO_GAME.get(pad, 0))
    if "mouse_sens" in out:
        try:
            ms = float(out["mouse_sens"])
            out["mouse_sensitivity"] = str(int(round(ms * 100))) if ms <= 10 else str(int(round(ms)))
        except ValueError:
            pass
    if "reflex" in out:
        reflex_to_game = {"off": "0", "0": "0", "on": "1", "1": "1", "boost": "2", "2": "2"}
        out["reflex"] = reflex_to_game.get(str(out["reflex"]).strip().casefold(), "0")
    if "fov" in out:
        out["field_of_view"] = str(max(50, min(150, _int_or(out["fov"], 100))))
    # The game reads vsync, not vk_present. Mailbox is 0 and is the default when the dropdown is unset.
    present = str(out.get("vk_present", "mailbox")).strip().casefold()
    out["vsync"] = {"mailbox": "0", "fifo": "1", "on": "1", "immediate": "2", "off": "2"}.get(present, "0")
    out["options_version"] = "9"
    return out


def normalize_settings_for_ui(values, display_resolution="1920x1080"):
    """Make game flat keys and launcher keys agree so Settings doesn't snap back to defaults."""
    out = dict(values or {})
    # Prefer the last launcher WxH label when the game ini only has 0/1/2.
    if out.get("output_res_ui"):
        out["output_res"] = str(out["output_res_ui"]).replace(" ", "")
    elif "output_res" in out:
        out["output_res"] = output_res_to_ui(out["output_res"], display_resolution)
    elif "output_resolution" in out:
        out["output_res"] = output_res_to_ui(out["output_resolution"], display_resolution)
    # Prefer explicit launcher reflex; fall back to low_latency alias. Accept 0/1/2 from Insert menu.
    if "reflex" not in out and "low_latency" in out:
        out["reflex"] = out["low_latency"]
    reflex_map = {"0": "off", "1": "on", "2": "boost", "off": "off", "on": "on", "boost": "boost"}
    if "reflex" in out:
        out["reflex"] = reflex_map.get(str(out["reflex"]).strip().lower(), str(out["reflex"]).strip().lower())
    if "low_latency" not in out and "reflex" in out:
        out["low_latency"] = out["reflex"]
    # Game colour_profile enum → launcher color_preset names when only the game wrote the file.
    color_map = {"0": "default", "1": "vivid", "2": "clarity", "3": "custom"}
    if "color_preset" not in out and out.get("colour_profile") in color_map:
        out["color_preset"] = color_map[out["colour_profile"]]
    if "color_brightness" not in out and "brightness" in out:
        out["color_brightness"] = out["brightness"]
    if "color_contrast" not in out and "contrast" in out:
        out["color_contrast"] = out["contrast"]
    if "color_saturation" not in out and "saturation" in out:
        out["color_saturation"] = out["saturation"]
    if "color_gamma" not in out and "gamma" in out:
        try:
            g = float(out["gamma"])
            out["color_gamma"] = f"{g/100:.2f}" if g > 3 else str(g)
        except ValueError:
            out["color_gamma"] = out["gamma"]
    return launcher_ui_from_game_ini(out)

def load_user_settings(game_dir=None, display_resolution="1920x1080"):
    """Reload settings on every launcher / Settings open: game ini first, then durable launcher backup."""
    global LAUNCHER_CONFIG
    LAUNCHER_CONFIG = load_config()
    path = settings_ini_path(game_dir)
    merged = parse_ini(path)
    backup = LAUNCHER_CONFIG.get("settings") or {}
    if isinstance(backup, dict):
        # Backup fills launcher-only keys the Insert menu never writes; game ini wins on shared keys when present.
        for key, value in backup.items():
            k = str(key).lower()
            if k not in merged or merged.get(k) in (None, ""):
                merged[k] = str(value)
            elif k not in GAME_FLAT_KEYS and k not in {"output_res", "fullscreen", "sharpness"}:
                merged[k] = str(value)
    return normalize_settings_for_ui(merged, display_resolution)

def persist_launcher_settings(updates):
    """Durable copy in mastersword_launcher.json so Settings survive Insert-menu rewrites / exit."""
    global LAUNCHER_CONFIG
    LAUNCHER_CONFIG = load_config()
    cur = dict(LAUNCHER_CONFIG.get("settings") or {})
    for key, value in updates.items():
        cur[str(key).lower()] = str(value)
    LAUNCHER_CONFIG["settings"] = cur
    save_config(LAUNCHER_CONFIG)

def _upsert_flat_ini_key(lines, key, value):
    """Write key=value with no spaces (host/nn_applet and Insert menu parse that form)."""
    key_l = key.casefold()
    out = f"{key}={value}\n"
    for i, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", ";", "[")):
            continue
        if "=" in stripped and stripped.split("=", 1)[0].strip().casefold() == key_l:
            lines[i] = out
            return
    if lines and not lines[-1].endswith("\n"):
        lines[-1] += "\n"
    lines.append(out)

def save_ini_settings(path, updates):
    path=Path(path); raw_lines=path.read_text(encoding="utf-8",errors="ignore").splitlines(keepends=True) if path.exists() else []
    obsolete={"dlss","dlaa","dlss_preset","xess_preset","antilag2","ao","ao_intensity"}
    lines=[]
    for line in raw_lines:
        stripped=line.strip()
        if "=" in stripped and not stripped.startswith(("#",";")):
            k=stripped.split("=",1)[0].strip().casefold()
            if k in obsolete:
                continue
        lines.append(line)
    # Always emit game-native flat keys the runtime reads, even when the UI used WxH / names.
    flat_updates = game_keys_from_launcher_ui(dict(updates))
    if "output_res" in flat_updates:
        flat_updates["output_res"] = str(output_res_to_enum(flat_updates["output_res"]))
        flat_updates["output_resolution"] = flat_updates["output_res"]
    if "reflex" in flat_updates:
        flat_updates["low_latency"] = flat_updates["reflex"]
    color_to_game = {"default": "0", "vivid": "1", "clarity": "2", "custom": "3"}
    if flat_updates.get("color_preset", "").casefold() in color_to_game:
        flat_updates["colour_profile"] = color_to_game[flat_updates["color_preset"].casefold()]
    for src, dst in (("color_brightness", "brightness"), ("color_contrast", "contrast"),
                     ("color_saturation", "saturation"), ("color_gamma", "gamma")):
        if src in flat_updates and dst not in flat_updates:
            flat_updates[dst] = flat_updates[src]
    if "color_gamma" in flat_updates:
        try:
            g = float(flat_updates["color_gamma"])
            if g <= 3.0:
                flat_updates["gamma"] = str(int(round(g * 100)))
        except ValueError:
            pass
    # realtime toggle shares the Insert-menu "Game speed" key.
    if "realtime" in flat_updates:
        flat_updates["game_speed"] = flat_updates["realtime"]
    for key, value in flat_updates.items():
        if key.casefold() in obsolete:
            continue
        # Game-facing keys stay flat so nn_applet / Insert menu keep them.
        if key.casefold() in GAME_FLAT_KEYS or key.casefold() in {
            "low_latency", "reflex", "vk_present", "window_size", "upscaler", "upscaler_preset",
            "fg_mode", "fg_mult", "fg_type", "fg_preset", "render_scale", "aspect", "fov",
            "perf_overlay", "device_profile", "operation_mode", "frame_gen",
            "color_preset", "colour_profile", "brightness", "contrast", "saturation", "gamma",
            "color_brightness", "color_contrast", "color_saturation", "color_gamma", "locale", "lang",
            "fps_limit", "assume_focus", "volume", "audio_channels", "pad_layout", "mouse_sens",
            "fxaa", "smaa", "realtime", "cloth_realtime", "highfps_fix",
        }:
            _upsert_flat_ini_key(lines, key, value)
            continue
        found=False
        active_section=""
        target_section=next((section for section, keys in {
            "frame rate":{"fps_limit","fps60","realtime","cloth_realtime","highfps_fix"},
            "display":{"fullscreen","window_size","vk_present","output_res","render_scale","upscaler","upscaler_preset","reflex","sharpness","aspect","fov","color_preset","color_brightness","color_contrast","color_saturation","color_gamma","device_profile","assume_focus","low_latency","frame_gen","fg_mode","fg_mult","fg_type","fg_preset","fg_still","fg_flow","perf_overlay","gpu_index","fxaa"},
            "language":{"locale","lang","menu_lang","operation_mode"},
            "audio":{"volume","audio_channels"},
            "input":{"motion","rumble_strength","rumble_curve","rumble_triggers","pad_layout","stick_deadzone","mouse_sens"},
            "performance":{"pipe_wait","pipe_skip","pipe_prebuild","lib_prebuild","experimental_geometry","fix_vs_fault","fix_zero","lod_guard","fix_indirect_reset","astc","memory","tex_alias","astc_bc","native_gs","fix_c0","bind_skip","shader_memo","pipe_memo","tex_memo","desc_template","push_skip","split_draws","split_us","vk_validation","vk_dyn_vertex","vk_dyn_a2c","vk_subgroup_pin","async_preempt","replay_async","timer_1ms","nx2_read_nowait"},
            "files":{"romfs_mods"}
        }.items() if key.casefold() in keys), "")
        if target_section:
            for i,line in enumerate(lines):
                stripped=line.strip()
                if stripped.startswith("[") and stripped.endswith("]"):
                    active_section=stripped[1:-1].casefold()
                    continue
                if active_section!=target_section or stripped.startswith(("#",";")):continue
                if "=" in line and line.split("=",1)[0].strip().casefold()==key.casefold():
                    lines[i]=f"{key}={value}\n"; found=True; break
            if not found:
                sec_idx = -1
                for idx, line in enumerate(lines):
                    if line.strip().casefold() == f"[{target_section}]":
                        sec_idx = idx
                        break
                if sec_idx != -1:
                    lines.insert(sec_idx + 1, f"{key}={value}\n")
                else:
                    if lines and lines[-1].strip(): lines.append("\n")
                    lines.append(f"[{target_section}]\n")
                    lines.append(f"{key}={value}\n")
        else:
            _upsert_flat_ini_key(lines, key, value)
    path.parent.mkdir(parents=True,exist_ok=True)
    tmp=path.with_suffix(path.suffix+".tmp"); tmp.write_text("".join(lines),encoding="utf-8"); tmp.replace(path)
    persist_launcher_settings(flat_updates)
    return True

def runtime_defaults():
    return {"locale":"en-US","menu_lang":"en","experimental_geometry":"0","native_gs":"0"}



def version_tuple(ver):
    """Comparable key for MasterSword tags (e.g. 0.1.5 < 0.1.5d < 0.1.5e < 0.1.6)."""
    m = re.search(r"(\d+(?:\.\d+)+)([a-z]*)", str(ver or "").strip().lower())
    if not m:
        return ((), "")
    nums = tuple(int(x) for x in m.group(1).split("."))
    return (nums, m.group(2))


def is_same_version(ver1, ver2):
    def norm(v):
        v = re.sub(r"^v", "", str(v or "").strip().lower())
        return re.sub(r"[^a-z0-9]", "", v)
    return norm(ver1) == norm(ver2)


def is_up_to_date(local_ver, remote_tag):
    if is_same_version(local_ver, remote_tag):
        return True
    lv, rv = version_tuple(local_ver), version_tuple(remote_tag)
    return bool(lv[0] and rv[0] and lv >= rv)

def find_running_game_processes():
    """Find running MasterSword game processes, excluding the launcher."""
    if os.name != "nt" or not GAME_DIR:
        return []
    target = str((GAME_DIR / "mastersword.exe").resolve()).replace("'", "''")
    command = (
        f"$target = '{target}'; "
        "$items = @(Get-CimInstance Win32_Process -Filter \\\"Name='mastersword.exe'\\\" -ErrorAction SilentlyContinue | "
        "Where-Object { $_.ExecutablePath -and "
        "[String]::Equals([IO.Path]::GetFullPath($_.ExecutablePath), $target, "
        "[StringComparison]::OrdinalIgnoreCase) } | "
        "Select-Object ProcessId, ExecutablePath); "
        "if ($items.Count -eq 1) { $items[0] | ConvertTo-Json -Compress } "
        "else { $items | ConvertTo-Json -Compress }"
    )
    try:
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command],
            capture_output=True, text=True, timeout=8,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)
        )
        if result.returncode == 0:
            output = result.stdout.strip()
            if not output or output == "null":
                return []
            parsed = json.loads(output)
            if isinstance(parsed, dict):
                return [{"pid": parsed.get("ProcessId"), "path": parsed.get("ExecutablePath")}]
            elif isinstance(parsed, list):
                return [{"pid": item.get("ProcessId"), "path": item.get("ExecutablePath")} for item in parsed if isinstance(item, dict)]
    except Exception:
        pass
    return []

def game_process_warning(action):
    processes = find_running_game_processes()
    if not processes:
        return None
    details = []
    for process in processes:
        label = []
        if process.get("pid") is not None:
            label.append(f"PID {process['pid']}")
        if process.get("path"):
            label.append(str(process["path"]))
        details.append(" - ".join(label) if label else "MasterSword game process")
    return "A MasterSword game process is currently running:\n" + "\n".join(details) + f"\n\nPlease close the game before {action}."

def merge_update_ini(current_ini, update_ini, output_ini):
    """Preserve every existing user setting and add only new keys from an update's mastersword_options.ini."""
    current_ini, update_ini, output_ini = Path(current_ini), Path(update_ini), Path(output_ini)
    if not current_ini.is_file():
        shutil.copy2(update_ini, output_ini)
        return
    lines = current_ini.read_text(encoding="utf-8", errors="ignore").splitlines(keepends=True)
    existing = set(parse_ini(current_ini))
    additions = []
    for line in update_ini.read_text(encoding="utf-8", errors="ignore").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", ";", "[")) or "=" not in stripped:
            continue
        key = stripped.split("=", 1)[0].strip().lower()
        if key and key not in existing:
            additions.append(line.rstrip("\r\n"))
            existing.add(key)
    if additions:
        if lines and not lines[-1].endswith(("\n", "\r")):
            lines[-1] += "\n"
        lines.extend(["\n# New settings supplied by the update; existing values were preserved.\n"]
                     + [line + "\n" for line in additions])
    output_ini.write_text("".join(lines), encoding="utf-8")

def extract_update_archive(archive_path, stage_dir):
    """Validate the official flat update archive and stage safe update files."""
    archive_path, stage_dir = Path(archive_path), Path(stage_dir)
    selected = {}
    with zipfile.ZipFile(archive_path) as archive:
        bad_member = archive.testzip()
        if bad_member:
            raise ValueError(f"Update archive CRC check failed for {bad_member}.")
        for info in archive.infolist():
            if info.is_dir():
                continue
            name = info.filename
            path = Path(name.replace("\\", "/"))
            if path.is_absolute() or len(path.parts) != 1 or ".." in path.parts:
                raise ValueError(f"Unexpected path in update archive: {name}")
            mode = (info.external_attr >> 16) & 0o170000
            if mode == 0o120000:
                raise ValueError(f"Symbolic links are not allowed in updates: {name}")
            key = path.name.casefold()
            if key.startswith((".", "exefs", "romfs", "profile", "save")):
                continue
            if key in selected:
                raise ValueError(f"Duplicate update file: {name}")
            if info.file_size <= 0:
                raise ValueError(f"Update file is empty: {name}")
            selected[key] = info

        for required in ("mastersword.exe", "masterswordlauncher.exe"):
            if required not in selected:
                raise ValueError(f"Update archive is missing required binary: {required}.")
        if selected["mastersword.exe"].file_size < 1024 * 1024 or selected["masterswordlauncher.exe"].file_size < 1024 * 1024:
            raise ValueError("Update executable is unexpectedly small.")

        for key, info in selected.items():
            target_name = info.filename
            target = stage_dir / target_name
            if key == "mastersword_options.ini":
                incoming = stage_dir / "_update_mastersword_options.ini"
                with archive.open(info) as source, incoming.open("wb") as dest:
                    shutil.copyfileobj(source, dest)
                merge_update_ini(GAME_DIR / "mastersword_options.ini", incoming, stage_dir / "mastersword_options.ini")
                incoming.unlink(missing_ok=True)
            else:
                with archive.open(info) as source, target.open("wb") as dest:
                    shutil.copyfileobj(source, dest)
    return [path.name for path in stage_dir.iterdir() if path.is_file() and not path.name.startswith("_")]

def validate_update_game_executable(exe_path, game_dir, timeout=30):
    """Run an integrity probe on the staged mastersword.exe before permitting installation."""
    exe_path = Path(exe_path)
    if not exe_path.is_file() or exe_path.stat().st_size < 1024 * 1024:
        raise ValueError("The staged mastersword.exe binary is missing or invalid.")
    command = [str(exe_path), "--probe-dpi"]
    result = subprocess.run(
        command, cwd=str(game_dir), capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=timeout,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)
    )
    output = (result.stdout or "") + "\n" + (result.stderr or "")
    if result.returncode != 0 or "probe-dpi: PASS" not in output:
        excerpt = output[-1000:].strip()
        raise RuntimeError("The update executable did not pass the integrity probe. No files were changed.\n" + excerpt)
    return output

def scan_mod_folders():
    MODS_DIR.mkdir(parents=True,exist_ok=True); found=[]
    for p in MODS_DIR.iterdir():
        if not p.is_dir(): continue
        disabled=p.name.casefold().endswith(MOD_DISABLED_SUFFIX)
        name=p.name[:-len(MOD_DISABLED_SUFFIX)] if disabled else p.name
        if name: found.append({"name":name,"path":p,"enabled":not disabled})
    return sorted(found,key=lambda x:x["name"].casefold())

def set_mod_folder_enabled(folder,enabled):
    folder=Path(folder); off=folder.name.casefold().endswith(MOD_DISABLED_SUFFIX)
    if bool(enabled)==(not off): return folder
    target=folder.with_name(folder.name[:-len(MOD_DISABLED_SUFFIX)] if enabled else folder.name+MOD_DISABLED_SUFFIX)
    if target.exists(): raise FileExistsError(f"Cannot rename because {target.name} already exists.")
    return folder.rename(target)

def create_mod_folder():
    if not GAME_DIR: raise RuntimeError("Choose a MasterSword game folder first.")
    MODS_DIR.mkdir(parents=True,exist_ok=True)
    name=""; index=1
    while not name or (MODS_DIR/name).exists():
        name=f"New Mod {index}"; index+=1
    return (MODS_DIR/name).mkdir()

def rebuild_mod_overlay():
    """Compose enabled per-mod RomFS trees for the runtime's single overlay directory."""
    mods=scan_mod_folders(); enabled=[m for m in mods if m["enabled"]]
    if not INI_PATH or not INI_PATH.is_file():
        raise FileNotFoundError("The selected MasterSword game folder is missing its runtime settings file.")
    owner=GAME_DIR/".romfs-mods-active.owner.json"
    if ACTIVE_MODS_DIR.exists() and not owner.is_file():
        raise RuntimeError("The active RomFS overlay exists without this launcher's ownership marker; it was left untouched.")
    stage=GAME_DIR/f".romfs-mods-stage-{os.getpid()}-{threading.get_ident()}"
    shutil.rmtree(stage,ignore_errors=True)
    if enabled:
        stage.mkdir(parents=True)
        seen={}
        for mod in enabled:
            for source in mod["path"].rglob("*"):
                if source.is_symlink():continue
                if not source.is_file():continue
                rel=source.relative_to(mod["path"]); dest=stage/rel
                try:dest.resolve().relative_to(stage.resolve())
                except ValueError:raise ValueError(f"Mod path escapes the overlay folder: {rel}") from None
                dest.parent.mkdir(parents=True,exist_ok=True)
                # Deterministic precedence follows the alphabetical package order shown in the manager.
                shutil.copy2(source,dest); seen[rel]=mod["name"]
        if ACTIVE_MODS_DIR.exists():
            old=GAME_DIR/f".romfs-mods-old-{time.time_ns()}"; ACTIVE_MODS_DIR.rename(old)
            stage.rename(ACTIVE_MODS_DIR); shutil.rmtree(old,ignore_errors=True)
        else: stage.rename(ACTIVE_MODS_DIR)
        owner_tmp=owner.with_name(owner.name+f".{os.getpid()}.tmp")
        owner_tmp.write_text(json.dumps({"owner":"MasterSwordLauncher","format":1},indent=2),encoding="utf-8")
        owner_tmp.replace(owner)
        save_ini_settings(INI_PATH,{"romfs_mods":str(ACTIVE_MODS_DIR)})
    else:
        shutil.rmtree(stage,ignore_errors=True)
        if ACTIVE_MODS_DIR.exists():
            old=GAME_DIR/f".romfs-mods-old-{time.time_ns()}"
            ACTIVE_MODS_DIR.rename(old)
            shutil.rmtree(old,ignore_errors=True)
        owner.unlink(missing_ok=True)
        if INI_PATH and INI_PATH.exists(): save_ini_settings(INI_PATH,{"romfs_mods":""})


def create_save_snapshot(profile_dir,backups_dir=None,suffix=""):
    profile_dir=Path(profile_dir); save_dir=profile_dir/"save"
    backups_dir=Path(backups_dir) if backups_dir else profile_dir/"backups"
    if not save_dir.is_dir(): raise FileNotFoundError(f"Save data was not found: {save_dir}")
    backups_dir.mkdir(parents=True,exist_ok=True); stamp=datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    base=f"save_{stamp}"+(f"_{suffix}" if suffix else ""); dest=backups_dir/base; n=2
    while dest.exists(): dest=backups_dir/f"{base}_{n}"; n+=1
    try:shutil.copytree(save_dir,dest)
    except Exception:
        shutil.rmtree(dest,ignore_errors=True)
        raise
    return dest

def restore_save_snapshot(profile_dir,snapshot_dir):
    profile_dir=Path(profile_dir); snapshot_dir=Path(snapshot_dir); save_dir=profile_dir/"save"; backups=profile_dir/"backups"
    try:snapshot_dir.resolve().relative_to(backups.resolve())
    except ValueError: raise ValueError("Selected snapshot is outside the MasterSword backup folder.") from None
    if not snapshot_dir.is_dir() or not any(snapshot_dir.iterdir()): raise ValueError("Selected snapshot is missing or empty.")
    safety=create_save_snapshot(profile_dir,backups,"before-restore")
    staged=profile_dir/f".save-restore-{os.getpid()}-{time.time_ns()}"; old=profile_dir/f".save-previous-{os.getpid()}-{time.time_ns()}"
    try:
        shutil.copytree(snapshot_dir,staged)
        if save_dir.exists(): save_dir.rename(old)
        staged.rename(save_dir)
        if old.exists():
            try:shutil.rmtree(old)
            except OSError:pass
    except Exception:
        if not save_dir.exists() and old.exists(): old.rename(save_dir)
        shutil.rmtree(staged,ignore_errors=True); raise
    return safety

class MasterSwordSlider(tk.Canvas):
    """Gold-tracked value slider used by the source launcher."""
    def __init__(self,parent,from_,to_,initial,on_change=None,width=190,height=28):
        super().__init__(parent,width=width,height=height,bg=COLOR_BG_DARK,highlightthickness=0)
        self.from_,self.to_,self.val,self.on_change=from_,to_,int(initial),on_change
        self.w,self.h,self.pad,self.hovered=width,height,12,False
        self.bind("<Button-1>",self._click); self.bind("<B1-Motion>",self._click)
        self.bind("<Enter>",lambda _e:self._hover(True)); self.bind("<Leave>",lambda _e:self._hover(False))
        self.bind("<MouseWheel>",self._wheel); self.draw()
    def _hover(self,value): self.hovered=value; self.config(cursor="hand2" if value else ""); self.draw()
    def _value(self,x):
        span=self.w-2*self.pad; frac=max(0,min(1,(x-self.pad)/span)); return round(self.from_+frac*(self.to_-self.from_))
    def _click(self,event): self.set(self._value(event.x))
    def _wheel(self,event): self.set(self.val+(5 if event.delta>0 else -5))
    def set(self,value):
        self.val=max(self.from_,min(self.to_,int(value))); self.draw()
        if self.on_change: self.on_change(self.val)
    def draw(self):
        self.delete("all"); y=self.h//2; x=self.pad+(self.val-self.from_)/(self.to_-self.from_)*(self.w-2*self.pad)
        self.create_line(self.pad,y,self.w-self.pad,y,fill=COLOR_TRACK_BG,width=6,capstyle="round")
        if x>self.pad:self.create_line(self.pad,y,x,y,fill=COLOR_TRACK_ACTIVE,width=6,capstyle="round")
        r=10 if self.hovered else 9
        self.create_oval(x-r,y-r,x+r,y+r,fill=COLOR_GOLD_HOVER if self.hovered else COLOR_GOLD,outline=COLOR_WHITE,width=1.5)
        self.create_oval(x-3,y-3,x+3,y+3,fill=COLOR_BG_DARK,outline="")


class MasterSwordSettingsDialog(tk.Toplevel):
    def __init__(self,parent):
        super().__init__(parent); self.parent_app=parent; self.title(f"MasterSword {CURRENT_VERSION} · Display & Graphics Settings · SteelTemple")
        self.geometry("900x700"); self.minsize(820,630); self.configure(bg=COLOR_BG_DARK); self.transient(parent); self.grab_set()
        self.vars={}; self.monitors=enumerate_displays()
        saved=parent.launcher_cfg.get("target_display")
        self.target_monitor=next((m for m in self.monitors if m["device"]==saved),next((m for m in self.monitors if m["primary"]),self.monitors[0]))
        self.display_resolution=self.target_monitor["resolution"]; self.display_refresh=self.target_monitor["refresh"]
        # Always re-read game ini + durable launcher backup when Settings opens (never stale module cache).
        self.settings_path = settings_ini_path(GAME_DIR)
        self.ini_values = load_user_settings(GAME_DIR, self.display_resolution)
        self.gpu_name,self.gpu_vendor=("Detecting graphics adapter…","Other")
        self.setup_ui(); place_window_on_monitor(self,self.target_monitor)
        threading.Thread(target=self._detect_gpu_worker,daemon=True).start()
    def _detect_gpu_worker(self):
        name,vendor=detect_gpu()
        try:self.after(0,lambda:self._show_detected_gpu(name,vendor))
        except tk.TclError:pass
    def _show_detected_gpu(self,name,vendor):
        self.gpu_name,self.gpu_vendor=name,vendor
        self.gpu_header.config(text=f"Detected: {name}",fg=COLOR_ACCENT_GREEN if vendor!="Other" else COLOR_TEXT_DIM)
    def get_val(self,key,default=""): return self.ini_values.get(key.lower(),default)
    def setup_ui(self):
        header=tk.Frame(self,bg=COLOR_CARD,height=58); header.pack(fill=tk.X,side=tk.TOP); header.pack_propagate(False)
        tk.Label(header,text=f"STEELTEMPSAINT MASTERSWORD  ·  GRAPHICS & SYSTEM CONFIGURATION",font=("Segoe UI",12,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD).pack(side=tk.LEFT,padx=24,pady=17)
        self.gpu_header=tk.Label(header,text=f"Detected: {self.gpu_name}",font=("Segoe UI",9),fg=COLOR_ACCENT_GREEN if self.gpu_vendor!="Other" else COLOR_TEXT_DIM,bg=COLOR_CARD)
        self.gpu_header.pack(side=tk.RIGHT,padx=20)
        style=ttk.Style(self); style.theme_use("clam")
        style.configure("MasterSword.TNotebook",background=COLOR_BG_DARK,borderwidth=0)
        style.configure("MasterSword.TNotebook.Tab",background=COLOR_CARD,foreground=COLOR_TEXT_DIM,font=("Segoe UI",10,"bold"),padding=[18,10],borderwidth=0)
        style.map("MasterSword.TNotebook.Tab",background=[("selected",COLOR_TAB_ACTIVE)],foreground=[("selected",COLOR_GOLD)])
        style.configure("MasterSword.Horizontal.TProgressbar", troughcolor=COLOR_TRACK_BG, background=COLOR_GOLD,
                        bordercolor=COLOR_CARD_BORDER, lightcolor=COLOR_GOLD, darkcolor=COLOR_GOLD)
        self.notebook=ttk.Notebook(self,style="MasterSword.TNotebook"); self.notebook.pack(fill=tk.BOTH,expand=True,padx=20,pady=9)
        self.tabs=[]
        for label in ("DISPLAY","UPSCALING & PERFORMANCE","GRAPHICS & LIGHTING","CONTROLS & AUDIO"):
            tab=tk.Frame(self.notebook,bg=COLOR_BG_DARK); self.notebook.add(tab,text=f"  {label}  "); self.tabs.append(self._scroll_area(tab))
        self.tab_display,self.tab_upscale,self.tab_graphics,self.tab_controls=self.tabs
        self.build_display_tab(); self.build_upscale_tab(); self.build_graphics_tab(); self.build_controls_tab()
        bottom=tk.Frame(self,bg=COLOR_CARD,height=62); bottom.pack(fill=tk.X,side=tk.BOTTOM); bottom.pack_propagate(False)
        self._button(bottom,"SAVE & APPLY",self.save_and_close,True).pack(side=tk.RIGHT,padx=(8,22),pady=13)
        self._button(bottom,"CANCEL",self.destroy).pack(side=tk.RIGHT,padx=8,pady=13)
        self._button(bottom,"RESTORE DEFAULTS",self.restore_defaults).pack(side=tk.LEFT,padx=22,pady=13)
    def _button(self,parent,text,cmd,primary=False):
        return tk.Button(parent,text=text,font=("Segoe UI",9,"bold"),fg="#000000" if primary else COLOR_WHITE,
            bg=COLOR_GOLD if primary else COLOR_BUTTON_HOVER,activebackground=COLOR_GOLD_HOVER,activeforeground="#000000",
            bd=0,padx=16,pady=8,cursor="hand2",command=cmd)
    def _scroll_area(self,parent):
        holder=tk.Frame(parent,bg=COLOR_BG_DARK); holder.pack(fill=tk.BOTH,expand=True)
        canvas=tk.Canvas(holder,bg=COLOR_BG_DARK,highlightthickness=0); bar=ttk.Scrollbar(holder,orient="vertical",command=canvas.yview)
        canvas.configure(yscrollcommand=bar.set); bar.pack(side=tk.RIGHT,fill=tk.Y); canvas.pack(side=tk.LEFT,fill=tk.BOTH,expand=True)
        inner=tk.Frame(canvas,bg=COLOR_BG_DARK); win=canvas.create_window((0,0),window=inner,anchor="nw")
        inner.bind("<Configure>",lambda _e:canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind("<Configure>",lambda e:canvas.itemconfigure(win,width=e.width))
        canvas.bind("<MouseWheel>",lambda e:canvas.yview_scroll(int(-e.delta/120),"units"))
        return inner
    def _section_note(self,parent,text):
        tk.Label(parent,text=text,font=("Segoe UI",9),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,wraplength=700,justify="left").pack(fill=tk.X,padx=22,pady=(8,4),anchor="w")
    def create_row_dropdown(self,parent,label,key,options,default,desc=""):
        row=tk.Frame(parent,bg=COLOR_BG_DARK,pady=7); row.pack(fill=tk.X,padx=20)
        left=tk.Frame(row,bg=COLOR_BG_DARK); left.pack(side=tk.LEFT,fill=tk.X,expand=True)
        tk.Label(left,text=label,font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(anchor="w")
        if desc:tk.Label(left,text=desc,font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,wraplength=540,justify="left").pack(anchor="w")
        current=config_text_value(self.get_val(key,str(default))); var=tk.StringVar(value=current if current in options else str(default)); self.vars[key]=var
        cb=ttk.Combobox(row,textvariable=var,values=options,state="readonly",width=22,font=("Segoe UI",9))
        cb.pack(side=tk.RIGHT,padx=5)
        if not hasattr(self,"combos"):self.combos={}
        self.combos[key]=cb
        return row
    def create_row_toggle(self,parent,label,key,default="0",desc=""):
        row=tk.Frame(parent,bg=COLOR_BG_DARK,pady=7); row.pack(fill=tk.X,padx=20)
        left=tk.Frame(row,bg=COLOR_BG_DARK); left.pack(side=tk.LEFT,fill=tk.X,expand=True)
        tk.Label(left,text=label,font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(anchor="w")
        if desc:tk.Label(left,text=desc,font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,wraplength=540,justify="left").pack(anchor="w")
        cur=config_text_value(self.get_val(key,str(default))); var=tk.StringVar(value="1" if cur.lower() in ("1","true","on") else "0"); self.vars[key]=var
        tk.Checkbutton(row,variable=var,onvalue="1",offvalue="0",bg=COLOR_BG_DARK,activebackground=COLOR_BG_DARK,selectcolor=COLOR_CARD,bd=0).pack(side=tk.RIGHT,padx=8)
        return row
    def create_row_slider(self,parent,label,key,low,high,default,unit="",desc=""):
        row=tk.Frame(parent,bg=COLOR_BG_DARK,pady=7); row.pack(fill=tk.X,padx=20)
        left=tk.Frame(row,bg=COLOR_BG_DARK); left.pack(side=tk.LEFT,fill=tk.X,expand=True)
        tk.Label(left,text=label,font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(anchor="w")
        if desc:tk.Label(left,text=desc,font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,wraplength=470,justify="left").pack(anchor="w")
        try:value=round(float(self.get_val(key,str(default)))*10) if key=="rumble_curve" else int(float(self.get_val(key,str(default))))
        except (TypeError,ValueError):value=default
        var=tk.IntVar(value=value); self.vars[key]=var
        pill=tk.Frame(row,bg=COLOR_CARD,bd=1,relief="solid",padx=7,pady=3); pill.pack(side=tk.RIGHT,padx=(8,0))
        text=tk.Label(pill,text="",font=("Segoe UI",9,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD,width=8); text.pack()
        def changed(v):
            var.set(v); display=f"{v/10:.1f}" if key in {"rumble_curve","ao_intensity"} else str(v)
            text.config(text=f"{display} {unit}".strip())
        slider=MasterSwordSlider(row,low,high,value,changed,width=180,height=28); slider.pack(side=tk.RIGHT,padx=5); changed(value)
        return row
    def build_display_tab(self):
        f=self.tab_display
        box=tk.LabelFrame(f,text=" Presets ",font=("Segoe UI",9,"bold"),fg=COLOR_GOLD,bg=COLOR_BG_DARK,bd=1,highlightthickness=1,highlightbackground=COLOR_CARD_BORDER,padx=12,pady=8)
        box.pack(fill=tk.X,padx=20,pady=(10,5))
        self.preset_status=tk.Label(box,text="Safe 1080p default out of the box — or apply a 1-click Performance / Handheld profile.",font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,anchor="w"); self.preset_status.pack(side=tk.LEFT,fill=tk.X,expand=True)
        tk.Button(box,text="HANDHELD PRESET",command=self.apply_handheld_preset,font=("Segoe UI",8,"bold"),fg="#000",bg=COLOR_GOLD,activebackground=COLOR_GOLD_HOVER,bd=0,padx=10,pady=7,cursor="hand2").pack(side=tk.RIGHT,padx=(8,0))
        tk.Button(box,text="PERFORMANCE PRESET",command=self.apply_performance_preset,font=("Segoe UI",8,"bold"),fg="#000",bg=COLOR_ACCENT_GREEN,activebackground="#8ef0a5",bd=0,padx=10,pady=7,cursor="hand2").pack(side=tk.RIGHT,padx=(8,0))
        monitor_row=tk.Frame(f,bg=COLOR_BG_DARK,pady=8); monitor_row.pack(fill=tk.X,padx=20)
        tk.Label(monitor_row,text="Target Display Monitor",font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(side=tk.LEFT)
        self.monitor_labels={f"Display {m['index']}: {m['resolution']}"+(" (Primary)" if m['primary'] else ""):m for m in self.monitors}
        self.target_display_var=tk.StringVar(value=next((k for k,v in self.monitor_labels.items() if v['device']==self.target_monitor['device']),next(iter(self.monitor_labels))))
        self.monitor_combo=ttk.Combobox(monitor_row,textvariable=self.target_display_var,values=list(self.monitor_labels),state="readonly",width=28); self.monitor_combo.pack(side=tk.RIGHT)
        self.monitor_combo.bind("<<ComboboxSelected>>",self.on_monitor_changed)
        pathrow=tk.Frame(f,bg=COLOR_BG_DARK,pady=6); pathrow.pack(fill=tk.X,padx=20)
        tk.Label(pathrow,text="MasterSword Game Folder",font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(side=tk.LEFT)
        tk.Label(pathrow,text=str(GAME_DIR) if GAME_DIR else "Not selected",font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK).pack(side=tk.LEFT,padx=12)
        tk.Button(pathrow,text="BROWSE…",command=self.parent_app.choose_game_directory,font=("Segoe UI",8,"bold"),fg=COLOR_WHITE,bg=COLOR_BUTTON_HOVER,activebackground=COLOR_GOLD,bd=0,padx=10,pady=5).pack(side=tk.RIGHT)
        self.create_row_toggle(f,"Exclusive Fullscreen","fullscreen","0","Start in exclusive fullscreen mode.")
        resolutions=["3840x2160","2560x1440","1920x1080","1600x900","1280x800","1280x720","3440x1440","5120x1440"]
        if self.display_resolution not in resolutions:resolutions.insert(0,self.display_resolution)
        self.create_row_dropdown(f,"Window Resolution","window_size",resolutions,self.get_val("window_size",self.display_resolution),"Window size in pixels. Game render resolution is configured separately by the runtime.")
        rates=["30","60"]
        cur_fps=self.get_val("fps_limit","60")
        self.create_row_dropdown(f,"Frame Rate Limit","fps_limit",rates,"30" if cur_fps=="30" else "60","Target frame rate: 60 FPS (Native MasterSword speed) or 30 FPS (Battery Saver / Handheld).")
        self.create_row_dropdown(f,"V-Sync / Presentation Mode","vk_present",["mailbox","fifo","immediate"],"mailbox","Mailbox is low-latency; FIFO is strict V-Sync; Immediate may tear.")
        self.create_row_dropdown(f,"Performance Metrics Overlay","perf_overlay",["off","fps","full"],self.get_val("perf_overlay","off"),"Display in-game real-time FPS and frametime metrics overlay.")
        # Runtime docked modes are 1920x1080 / 2560x1440 / 3840x2160 (0/1/2). Default to safe 1920x1080 on fresh installs so 4K monitors don't tank FPS out of the gate.
        output_sizes=["1920x1080","2560x1440","3840x2160","1280x720","1600x900","3200x1800","2560x1080","3440x1440","3840x1600","5120x2160","5120x1440"]
        if self.display_resolution not in output_sizes:output_sizes.insert(0,self.display_resolution)
        self.create_row_dropdown(f,"Game Render Resolution","output_res",output_sizes,self.get_val("output_res","1920x1080"),"Docked output: defaults to safe 1920×1080 (raise to 2560×1440 or 3840×2160 on high-end GPUs). Restart the game after changing.")
        self.create_row_dropdown(f,"Aspect Ratio Behavior","aspect",["fit","stretch"],"fit","Fit preserves the image; Stretch fills the full display.")
        self.create_row_slider(f,"Field of View","fov",50,150,100,"%","Scale vertical gameplay field of view (50%–150%, synced with Insert -> Gameplay -> Field of view; cutscenes/title stay at 100%).")
        wide=tk.Frame(f,bg=COLOR_CARD,padx=13,pady=9,highlightthickness=1,highlightbackground=COLOR_CARD_BORDER); wide.pack(fill=tk.X,padx=20,pady=8)
        tk.Label(wide,text="ULTRAWIDE DISPLAY PROFILE",font=("Segoe UI",9,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD).pack(side=tk.LEFT)
        tk.Label(wide,text="Uses MasterSword's built-in aspect and FOV controls; no game files are patched.",font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_CARD).pack(side=tk.LEFT,padx=12)
        tk.Button(wide,text="APPLY",command=self.apply_ultrawide_profile,font=("Segoe UI",8,"bold"),fg="#000",bg=COLOR_GOLD,activebackground=COLOR_GOLD_HOVER,bd=0,padx=10,pady=6).pack(side=tk.RIGHT)
    def on_monitor_changed(self,_event=None):
        self.target_monitor=self.monitor_labels.get(self.target_display_var.get(),self.target_monitor)
        self.display_resolution=self.target_monitor["resolution"]; self.display_refresh=self.target_monitor["refresh"]
        if "window_size" in self.vars:self.vars["window_size"].set(self.display_resolution)
        self.parent_app.move_to_monitor(self.target_monitor)
    def apply_performance_preset(self):
        for k,v in {"device_profile":"desktop","output_res":"1920x1080","fps_limit":"60","vk_present":"mailbox","upscaler":"smrs","upscaler_preset":"performance","render_scale":"50","fg_mode":"mastersword","fg_mult":"2x","fg_type":"adaptive","sharpness":"35","reflex":"on","operation_mode":"docked"}.items():
            if k in self.vars:self.vars[k].set(v)
        self.preset_status.config(text="Performance profile selected: 1920×1080 output, Nodix SMRS Performance (50%), and MasterSword 2x FG.",fg=COLOR_ACCENT_GREEN)
    def apply_handheld_preset(self):
        for k,v in {"device_profile":"deck","window_size":"1280x800","output_res":"1280x800","fps_limit":"60","vk_present":"fifo","upscaler":"smrs","upscaler_preset":"balanced","render_scale":"58","sharpness":"50","operation_mode":"handheld"}.items():
            if k in self.vars:self.vars[k].set(v)
        self.preset_status.config(text="Handheld profile selected: 1280×800, 60 FPS, Nodix SMRS, and FIFO presentation.",fg=COLOR_ACCENT_GREEN)
    def apply_ultrawide_profile(self):
        if "aspect" in self.vars:self.vars["aspect"].set("fit")
        if "fov" in self.vars:self.vars["fov"].set(110)
        if "window_size" in self.vars:self.vars["window_size"].set(self.target_monitor["resolution"])
        if "output_res" in self.vars:self.vars["output_res"].set(self.target_monitor["resolution"])
        messagebox.showinfo("Display Profile","Applied MasterSword's fit aspect mode and a wider camera view. This setting does not rewrite RomFS or HUD files.",parent=self)
    def build_upscale_tab(self):
        f=self.tab_upscale
        self.create_row_dropdown(f,"Super Resolution Technology","upscaler",["off","smrs","smai","dlss","fsr4","fsr3","fsr","xess","lanczos"],"off","Upscaler written into the Insert menu: Off (game native TAAU), Nodix SMRS / SMAI, DLSS SR (NVIDIA), FSR 4 (AMD), FSR 3.1 / 1.0, XeSS, or Lanczos.")
        self.create_row_dropdown(f,"Upscaler Quality Preset","upscaler_preset",["native","ultra_quality","quality","balanced","performance","ultra_performance"],"performance","Maps to DLSS / FSR / Nodix / XeSS quality rows in-game (DLSS: Quality/Balanced/Perf/Ultra/DLAA).")
        self.create_row_dropdown(f,"Frame Generation Mode","fg_mode",["off","mastersword","msm_fg","dlss_fg","fsr3_fg","xefg"],"off","Writes FG engine for the Insert menu: Off (default), MasterSword, Nodix SMFG, DLSS MFG, FSR 3.1 FG, or XeSS MFG.")
        self.create_row_dropdown(f,"Frame Multiplier","fg_mult",["2x","3x","4x","5x","6x","8x"],"2x","MasterSword keeps 2x–8x (no 5x). Nodix SMFG saves ×2–×6, including 5x. DLSS MFG up to 6x, XeSS up to 4x, FSR 3.1 FG at 2x. A higher pick is clamped to that engine on save.")
        self.create_row_dropdown(f,"DLSS FG Model","fg_preset",["A","B","C"],"B","Writes fg_model A/B/C (shown with DLSS MFG in the Insert menu).")
        self.create_row_dropdown(f,"Multiplier Pacing","fg_type",["fixed","adaptive"],"fixed","Writes frame_pacing: Fixed or Adaptive (even interval).")
        self.create_row_dropdown(f,"Render Scale (Internal)","render_scale",["game","33","50","58","67","75","100","125","150","200"],"game","Defaults to Auto ('game', dynamic resolution). Writes Resolution when Auto/100/75/67/50; Upscaler quality owns scale when an upscaler is active.")
        self.create_row_slider(f,"Sharpness","sharpness",0,100,0,"%","Insert-menu Sharpness (0–100).")
        self.create_row_dropdown(f,"Low-Latency Mode","reflex",["off","on","boost"],"off","NVIDIA Reflex — same Off/On/Boost row as the Insert menu (safe 'Off' default; active with DLSS Frame Generation too).")
        self.create_row_dropdown(f,"Device Profile","device_profile",["auto","desktop","deck"],"auto","Runtime profile for thread priority and system hints.")
        self.create_row_dropdown(f,"Shader Pipeline Policy","pipe_wait",["default","draw"],"default","Default waits only when needed; Draw prioritizes avoiding pipeline stalls.")
        self.create_row_toggle(f,"Prebuild Vulkan Pipelines","pipe_prebuild","1","Build reusable Vulkan pipelines before play when supported.")
        self._section_note(f,"SAVE & APPLY writes the same flat keys the Insert menu uses (upscaling, fg_engine, frame_generation, dlss_quality…). Change here or in-game — both stay in sync.")

        def on_preset_changed(_e=None):
            p = self.vars["upscaler_preset"].get()
            scale_map = {"native": "100", "ultra_quality": "75", "quality": "67", "balanced": "58", "performance": "50", "ultra_performance": "33"}
            if p in scale_map and "render_scale" in self.vars:
                self.vars["render_scale"].set(scale_map[p])
            if "operation_mode" in self.vars:
                if p in ("native", "quality", "ultra_quality"):
                    self.vars["operation_mode"].set("docked")
                else:
                    self.vars["operation_mode"].set("handheld")

        def on_scale_changed(_e=None):
            s = self.vars["render_scale"].get()
            rev_map = {"100": "native", "75": "ultra_quality", "67": "quality", "58": "balanced", "50": "performance", "33": "ultra_performance"}
            if s in rev_map and "upscaler_preset" in self.vars:
                self.vars["upscaler_preset"].set(rev_map[s])
            if "operation_mode" in self.vars:
                if s in ("100", "125", "150", "200", "75", "67", "game"):
                    self.vars["operation_mode"].set("docked")
                else:
                    self.vars["operation_mode"].set("handheld")

        if hasattr(self, "combos"):
            if "upscaler_preset" in self.combos:
                self.combos["upscaler_preset"].bind("<<ComboboxSelected>>", on_preset_changed)
            if "render_scale" in self.combos:
                self.combos["render_scale"].bind("<<ComboboxSelected>>", on_scale_changed)
        self._build_dlss_sdk_panel(f)
    def _build_dlss_sdk_panel(self, parent):
        panel = tk.Frame(parent, bg=COLOR_CARD, padx=14, pady=12, highlightthickness=1, highlightbackground=COLOR_CARD_BORDER)
        panel.pack(fill=tk.X, padx=20, pady=(6, 12))
        tk.Label(panel, text="NVIDIA DLSS / NGX SDK", font=("Segoe UI", 9, "bold"),
                 fg=COLOR_GOLD, bg=COLOR_CARD).pack(anchor="w")
        present, total, missing = nvidia_dlss_status(GAME_DIR)
        if present >= total:
            status = f"Ready — all {total} required DLLs are beside the game exe."
            color = COLOR_ACCENT_GREEN
        elif present:
            status = (f"{present}/{total} present. Missing: {', '.join(missing[:4])}"
                      f"{'…' if len(missing) > 4 else ''}. Download installs from NVIDIA GitHub.")
            color = COLOR_GOLD
        else:
            status = (f"Not installed. Downloads NVIDIA NGX DLSS & DLSS-G runtimes {NGX_SDK_VERSION} "
                      f"(~63 MB, once) then copies nvngx_dlss.dll & nvngx_dlssg.dll into ngx\\ beside the game.")
            color = COLOR_TEXT_DIM
        self.dlss_status = tk.Label(panel, text=status, font=("Segoe UI", 8), fg=color, bg=COLOR_CARD,
                                    wraplength=520, justify="left")
        self.dlss_status.pack(anchor="w", pady=(4, 6))
        self.dlss_progress = ttk.Progressbar(panel, style="MasterSword.Horizontal.TProgressbar",
                                             mode="determinate", maximum=100, value=0)
        self.dlss_progress.pack(fill=tk.X, pady=(0, 8))
        row = tk.Frame(panel, bg=COLOR_CARD); row.pack(fill=tk.X)
        self.dlss_pct = tk.Label(row, text="0%", font=("Segoe UI", 8, "bold"), fg=COLOR_TEXT_DIM, bg=COLOR_CARD)
        self.dlss_pct.pack(side=tk.LEFT)
        self.dlss_button = tk.Button(row, text="⬇ DOWNLOAD & INSTALL DLSS SDK", command=self.install_dlss_sdk,
                                     font=("Segoe UI", 8, "bold"), fg="#000", bg=COLOR_GOLD,
                                     activebackground=COLOR_GOLD_HOVER, bd=0, padx=12, pady=8, cursor="hand2")
        self.dlss_button.pack(side=tk.RIGHT)
    def install_dlss_sdk(self):
        if not GAME_DIR:
            messagebox.showerror("NVIDIA DLSS SDK", "Select a MasterSword game folder first.", parent=self)
            return
        if getattr(self, "dlss_install_active", False):
            return
        present, total, missing = nvidia_dlss_status(GAME_DIR)
        need_download = present < total and not all(find_nvidia_dlss_file(n) for n, _ in NVIDIA_DLSS_SDK_FILES)
        if need_download:
            ok = messagebox.askyesno(
                "Download NVIDIA DLSS SDK",
                f"This will download NVIDIA NGX DLSS & DLSS-G runtimes {NGX_SDK_VERSION} from GitHub\n"
                f"(cached under %LOCALAPPDATA%\\MasterSword\\sdk_cache, NVIDIA RTX SDK license),\n"
                f"then install ngx\\nvngx_dlss.dll and ngx\\nvngx_dlssg.dll into:\n\n{GAME_DIR}\n\nContinue?",
                parent=self,
            )
            if not ok:
                return
        self.dlss_install_active = True
        self.dlss_button.config(state=tk.DISABLED, text="DOWNLOADING…")
        self.dlss_status.config(text="Preparing NVIDIA NGX DLSS SDK…", fg=COLOR_GOLD)
        self.dlss_progress["value"] = 0
        self.dlss_pct.config(text="0%")
        threading.Thread(target=self._dlss_install_worker, daemon=True).start()
    def _dlss_ui_progress(self, fraction, message):
        pct = max(0, min(100, int(round(fraction * 100))))
        def apply():
            self.dlss_progress.configure(value=pct)
            self.dlss_pct.configure(text=f"{pct}%")
            self.dlss_status.configure(text=message, fg=COLOR_GOLD)
        try:
            self.after(0, apply)
        except tk.TclError:
            pass
    def _dlss_install_worker(self):
        installed, missing = 0, []
        total = len(NVIDIA_DLSS_SDK_FILES)
        try:
            GAME_DIR.mkdir(parents=True, exist_ok=True)
            # Phase A (0–80%): ensure cache via local files or official NVIDIA download
            def cache_prog(frac, msg):
                self._dlss_ui_progress(0.80 * frac, msg)

            try:
                ensure_nvidia_dlss_sdk_cache(cache_prog, force_download=False)
            except Exception as dl_exc:
                # If local copies already cover everything, keep going; else fail clearly.
                if any(find_nvidia_dlss_file(n) is None for n, _ in NVIDIA_DLSS_SDK_FILES):
                    raise RuntimeError(f"Could not download NGX SDK: {dl_exc}") from dl_exc
                self._dlss_ui_progress(0.80, f"Download skipped — using local DLLs ({dl_exc})")

            # Phase B (80–100%): copy into the game folder. Prefer the official SDK cache copy and
            # overwrite size-mismatched files (a foreign nvngx_dlss.dll breaks slDLSSSetOptions).
            runtime_dir = nvidia_sdk_cache_dir() / "ngx-sdk" / "runtime"
            for i, (name, desc) in enumerate(NVIDIA_DLSS_SDK_FILES):
                dst = GAME_DIR / name
                dst.parent.mkdir(parents=True, exist_ok=True)
                base = 0.80 + 0.20 * (i / total)
                self._dlss_ui_progress(base, f"Installing {name} — {desc}")
                leaf = Path(name).name
                src = (runtime_dir / leaf) if (runtime_dir / leaf).is_file() else find_nvidia_dlss_file(leaf)
                if not src:
                    if dst.is_file() and dst.stat().st_size > 1024:
                        installed += 1
                        self._dlss_ui_progress(0.80 + 0.20 * ((i + 1) / total), f"Keeping existing: {name}")
                    else:
                        missing.append(name)
                        self._dlss_ui_progress(0.80 + 0.20 * ((i + 1) / total), f"Still missing: {name}")
                    continue
                try:
                    if src.resolve() == dst.resolve():
                        installed += 1
                        continue
                except OSError:
                    pass
                overwrite = dst.is_file() and dst.stat().st_size != src.stat().st_size
                if dst.is_file() and not overwrite:
                    installed += 1
                    self._dlss_ui_progress(0.80 + 0.20 * ((i + 1) / total), f"Already present: {name}")
                    continue
                shutil.copy2(src, dst)
                installed += 1
                self._dlss_ui_progress(
                    0.80 + 0.20 * ((i + 1) / total),
                    f"{'Replaced mismatched' if overwrite else 'Installed'} {name}",
                )
            if missing:
                message = (f"Installed {installed}/{total}. Still missing: {', '.join(missing)}. "
                           f"Check your network and retry the download.")
                color = "#ffb347"
            else:
                message = f"NVIDIA DLSS SDK ready — {installed}/{total} DLLs beside mastersword.exe."
                color = COLOR_ACCENT_GREEN
            self._dlss_ui_progress(1.0, message)
        except Exception as exc:
            message, color = f"DLSS SDK install failed: {exc}", "#ff7373"
        try:
            self.after(0, lambda: self._finish_dlss_install(message, color))
        except tk.TclError:
            pass
    def _finish_dlss_install(self, message, color):
        self.dlss_install_active = False
        self.dlss_button.config(state=tk.NORMAL, text="⬇ DOWNLOAD & INSTALL DLSS SDK")
        self.dlss_status.config(text=message, fg=color)
        if color == COLOR_ACCENT_GREEN:
            self.dlss_progress["value"] = 100
            self.dlss_pct.config(text="100%")
    def build_graphics_tab(self):
        f=self.tab_graphics
        self.create_row_dropdown(f,"Color Profile","color_preset",["default","vivid","clarity","custom"],"default","Choose a display color preset.")
        self.create_row_slider(f,"Color Brightness","color_brightness",-30,30,0,"","Display brightness adjustment.")
        self.create_row_slider(f,"Color Contrast","color_contrast",70,150,100,"%","Display contrast adjustment.")
        self.create_row_slider(f,"Color Saturation","color_saturation",50,200,100,"%","Display saturation adjustment.")
        self.create_row_dropdown(f,"Color Gamma","color_gamma",["0.8","0.9","1.0","1.1","1.2"],"1.0","Gamma adjustment.")
        self.create_row_toggle(f,"FXAA Anti-Aliasing","fxaa","1","Game's built-in FXAA edge smoothing.")
        self.create_row_toggle(f,"High-FPS Realtime Physics","realtime","1","Unlocks game physics calculation tickrate for smooth 60+ FPS without slow motion.")
        self.create_row_toggle(f,"Realtime Cloth Physics","cloth_realtime","1","Evaluates cloth and soft-body style effects in real time at high refresh rates.")
        self.create_row_toggle(f,"High-FPS Camera Smoothing Fix","highfps_fix","1","Fixes camera tracking jitter and maintains smooth animation pacing.")
        self.create_row_dropdown(f,"Console Emulation Profile","operation_mode",["docked","handheld"],self.get_val("operation_mode","docked"),"Docked renders internally at 2880x1620 for full fidelity; Handheld renders at 1920x1080 (56% lighter GPU workload) - pair with Super Motion SMRS for massive FPS gains!")
        self.create_row_toggle(f,"Native Vulkan Geometry Shaders","native_gs","1","Directly compiles native geometry shaders on supported Vulkan GPUs.")
        self.create_row_toggle(f,"Experimental Geometry","experimental_geometry","0","Experimental MasterSword geometry path; leave off if destructible terrain renders incorrectly.")
        self.create_row_toggle(f,"Skip Known NVIDIA Movie Fault","fix_vs_fault","1","Avoids a known NVIDIA GPU fault after the first guide movie.")
        self.create_row_toggle(f,"Zero New GPU Memory","fix_zero","1","Initializes new GPU memory to match the console's behavior.")
        self.create_row_toggle(f,"Guard Level-of-Detail Transitions","lod_guard","1","Prevents temporary terrain and object flicker during transitions.")
        self.create_row_toggle(f,"Reset Indirect Draw State","fix_indirect_reset","1")
        self.create_row_toggle(f,"Prebuild Shader Libraries","lib_prebuild","1")
        self._build_prewarm_panel(f)
    def _build_prewarm_panel(self, parent):
        panel = tk.Frame(parent, bg=COLOR_CARD, padx=14, pady=12, highlightthickness=1, highlightbackground=COLOR_CARD_BORDER)
        panel.pack(fill=tk.X, padx=20, pady=10)
        tk.Label(panel, text="VULKAN SHADER PRE-WARM", font=("Segoe UI", 9, "bold"),
                 fg=COLOR_GOLD, bg=COLOR_CARD).pack(anchor="w")
        self.warm_status = tk.Label(
            panel,
            text="Builds hookshot_programs.bin + mkw_vk_pipelines.bin before play (no game window). Can take several minutes.",
            font=("Segoe UI", 8), fg=COLOR_TEXT_DIM, bg=COLOR_CARD, wraplength=520, justify="left",
        )
        self.warm_status.pack(anchor="w", pady=(4, 6))
        self.warm_progress = ttk.Progressbar(panel, style="MasterSword.Horizontal.TProgressbar",
                                             mode="determinate", maximum=100, value=0)
        self.warm_progress.pack(fill=tk.X, pady=(0, 8))
        row = tk.Frame(panel, bg=COLOR_CARD); row.pack(fill=tk.X)
        self.warm_pct = tk.Label(row, text="0%", font=("Segoe UI", 8, "bold"), fg=COLOR_TEXT_DIM, bg=COLOR_CARD)
        self.warm_pct.pack(side=tk.LEFT)
        self.warm_button = tk.Button(
            row, text="⚡ PRE-WARM VULKAN SHADERS", command=self.prewarm_shaders,
            font=("Segoe UI", 8, "bold"), fg="#000", bg=COLOR_GOLD, activebackground=COLOR_GOLD_HOVER,
            bd=0, padx=12, pady=8, cursor="hand2",
        )
        self.warm_button.pack(side=tk.RIGHT)
    def prewarm_shaders(self):
        if not GAME_DIR or not (GAME_DIR / "mastersword.exe").is_file():
            messagebox.showerror("Shader Pre-Warm", "Select a MasterSword game folder first.", parent=self)
            return
        if getattr(self, "warm_process_active", False):
            return
        self.warm_process_active = True
        self.warm_button.config(state=tk.DISABLED, text="PRE-WARMING…")
        self.warm_status.config(text="Starting Vulkan shader preparation…", fg=COLOR_GOLD)
        self.warm_progress["value"] = 0
        self.warm_pct.config(text="0%")
        threading.Thread(target=self._prewarm_worker, daemon=True).start()
    def _prewarm_ui_progress(self, fraction, message=None):
        pct = max(0, min(100, int(round(fraction * 100))))
        def apply():
            self.warm_progress.configure(value=pct)
            self.warm_pct.configure(text=f"{pct}%")
            if message:
                self.warm_status.configure(text=message, fg=COLOR_GOLD)
        try:
            self.after(0, apply)
        except tk.TclError:
            pass
    def _prewarm_worker(self):
        logs = []
        try:
            logs_dir = GAME_DIR / "logs"
            logs_dir.mkdir(parents=True, exist_ok=True)
            env = os.environ.copy()
            env["RX_BACKEND"] = "vulkan"
            env["RX_VK_PREDICT_S"] = env.get("RX_VK_PREDICT_S", "0")
            skipped = GAME_DIR / "hookshot_prepare_skipped"
            if skipped.is_file():
                try:
                    skipped.unlink()
                except OSError:
                    pass
            # Prefer full auto_prewarm (census + warm) when available; otherwise warm-shaders only.
            auto_candidates = []
            if not getattr(sys, "frozen", False):
                auto_candidates.append(Path(__file__).resolve().parents[1] / "tools" / "auto_prewarm.py")
            auto_candidates += [
                Path(sys.executable).resolve().parent / "tools" / "auto_prewarm.py",
                GAME_DIR / "tools" / "auto_prewarm.py",
                GAME_DIR / "auto_prewarm.py",
            ]
            auto = next((p for p in auto_candidates if p.is_file()), None)
            if auto is not None and not getattr(sys, "frozen", False):
                py = sys.executable
                cmd = [py, "-u", str(auto), "--game-dir", str(GAME_DIR), "--predict-s", "0"]
                cwd = str(auto.parent.parent if auto.parent.name == "tools" else auto.parent)
            else:
                cmd = [str(GAME_DIR / "mastersword.exe"), "--warm-shaders"]
                cwd = str(GAME_DIR)
            self._prewarm_ui_progress(0.02, "Launching shader warm…")
            proc = subprocess.Popen(
                cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            stage = "Preparing shaders"
            last_frac = 0.02
            assert proc.stdout is not None
            for raw in proc.stdout:
                line = raw.rstrip()
                if line:
                    logs.append(line)
                if line.startswith("@@progress "):
                    try:
                        frac = float(line.split(None, 1)[1])
                    except (ValueError, IndexError):
                        continue
                    last_frac = max(0.02, min(0.99, frac))
                    self._prewarm_ui_progress(last_frac, f"{stage}… {int(last_frac * 100)}%")
                elif line.startswith("@@stage "):
                    parts = line.split(None, 2)
                    stage = parts[2] if len(parts) >= 3 else (parts[1] if len(parts) > 1 else stage)
                    self._prewarm_ui_progress(last_frac, f"{stage}…")
                elif "prepared " in line.lower() or "precompile" in line.lower() or "predicted " in line.lower():
                    msg = line.split("] ", 1)[-1] if "] " in line else line
                    self._prewarm_ui_progress(max(0.05, last_frac), msg[:120])
                elif line.startswith("[1/2]") or line.startswith("[2/2]"):
                    last_frac = 0.15 if line.startswith("[1/2]") else 0.35
                    self._prewarm_ui_progress(last_frac, line)
            rc = proc.wait(timeout=7200)
            (logs_dir / "prewarm.log").write_text(
                "=== MasterSword Shader Pre-Warm Log ===\n"
                f"Exit Code: {rc}\n\n" + "\n".join(logs) + "\n",
                encoding="utf-8",
            )
            if rc == 0:
                message, color = "Shader cache preparation completed.", COLOR_ACCENT_GREEN
                self._prewarm_ui_progress(1.0, message)
            else:
                message, color = f"Shader pre-warm failed (exit {rc}). Check logs/prewarm.log.", "#ff7373"
        except Exception as exc:
            message, color = f"Shader pre-warm failed: {exc}", "#ff7373"
            try:
                (GAME_DIR / "logs").mkdir(parents=True, exist_ok=True)
                (GAME_DIR / "logs" / "prewarm.log").write_text("\n".join(logs) + f"\n\nERROR: {exc}\n", encoding="utf-8")
            except OSError:
                pass
        try:
            self.after(0, lambda: self._finish_prewarm(message, color))
        except tk.TclError:
            pass
    def _finish_prewarm(self, message, color):
        self.warm_process_active = False
        self.warm_button.config(state=tk.NORMAL, text="⚡ PRE-WARM VULKAN SHADERS")
        self.warm_status.config(text=message, fg=color)
        if color == COLOR_ACCENT_GREEN:
            self.warm_progress["value"] = 100
            self.warm_pct.config(text="100%")
    def build_controls_tab(self):
        f=self.tab_controls
        self.create_row_dropdown(f,"Controller Button Prompts","pad_layout",["auto","nintendo","xbox","xbox_prompts"],"auto","Select Nintendo-style, Xbox-style, or automatic prompts.")
        self.create_row_toggle(f,"Motion Controls","motion","1","Enable motion input for supported controllers.")
        self.create_row_slider(f,"Controller Rumble Strength","rumble_strength",0,300,130,"%","Set controller vibration strength.")
        self.create_row_dropdown(f,"Rumble Response Curve","rumble_curve",["0.2","0.4","0.6","0.8","1.0","1.2","1.5"],"0.6","Shape the response to small and large rumble events.")
        self.create_row_toggle(f,"Rumble Triggers","rumble_triggers","0","Enable trigger rumble where the controller supports it.")
        self.create_row_slider(f,"Stick Deadzone","stick_deadzone",0,50,8,"%","Ignore small stick movements below this threshold.")
        self.create_row_slider(f,"Mouse Sensitivity","mouse_sens",0,10,1,"x","Adjust mouse input sensitivity.")
        self.create_row_slider(f,"Game Volume","volume",0,100,100,"%","Set the game's master volume.")
        self.create_row_dropdown(f,"Audio Output Channels","audio_channels",["stereo","5.1"],self.get_val("audio_channels","stereo"),"Choose between stereo or 5.1 surround sound audio mixing.")
        loc = self.get_val("locale") or self.get_val("lang") or "en-US"
        self.create_row_dropdown(f,"Game Language","locale",["en-US","en-GB","ja","fr","de","it","es","nl","ru","pt","pt-BR","zh-Hans","zh-Hant","ko"],loc,"English is selected by default.")
        self.create_row_dropdown(f,"Launcher Menu Language","menu_lang",["en"],"en","The game menu and overlay are kept in English.")
        self.create_row_toggle(f,"Keep Game Active When Unfocused","assume_focus","1","Keep the game running when its window is not in front.")
        row=tk.Frame(f,bg=COLOR_BG_DARK,pady=8); row.pack(fill=tk.X,padx=20)
        self.sound_effects_var=tk.BooleanVar(value=self.parent_app.launcher_cfg.get("launcher_sound_effects",True))
        tk.Checkbutton(row,text="Launcher Sound Effects",variable=self.sound_effects_var,font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK,activebackground=COLOR_BG_DARK,activeforeground=COLOR_GOLD,selectcolor=COLOR_CARD,bd=0,highlightthickness=0).pack(side=tk.LEFT)
    def save_and_close(self):
        global INI_PATH, LAUNCHER_CONFIG
        updates={k:(f"{v.get()/10:.1f}" if k in {"rumble_curve"} else str(v.get())) for k,v in self.vars.items()}
        if "locale" in updates:
            updates["lang"] = updates["locale"]
        updates["reflex"] = updates.get("reflex", "off")
        updates["low_latency"] = updates.get("reflex", "off")
        # Keep a UI-facing WxH copy in the durable backup even though the game ini stores 0/1/2.
        updates["output_res_ui"] = updates.get("output_res", "1920x1080")
        updates = game_keys_from_launcher_ui(updates)
        try:
            if not GAME_DIR:
                messagebox.showwarning(
                    "Select Game Folder",
                    "Pick the MasterSword game folder first so settings are written beside mastersword.exe.\n"
                    "Otherwise the Insert menu will not see them.",
                    parent=self,
                )
            path = settings_ini_path(GAME_DIR)
            save_ini_settings(path, updates)
            INI_PATH = path
            LAUNCHER_CONFIG = load_config()
            self.parent_app.launcher_cfg = LAUNCHER_CONFIG
            self.parent_app.launcher_cfg["target_display"]=self.target_monitor["device"]
            self.parent_app.launcher_cfg["launcher_sound_effects"]=self.sound_effects_var.get()
            self.parent_app.launcher_cfg["auto_install_updates"]=self.parent_app.launcher_cfg.get("auto_install_updates", True)
            if GAME_DIR:
                self.parent_app.launcher_cfg["game_dir"] = str(Path(GAME_DIR).resolve())
            # Re-merge after save so the in-memory backup keeps the WxH label the dropdown expects next open.
            backup = dict(self.parent_app.launcher_cfg.get("settings") or {})
            backup.update({k.lower(): str(v) for k, v in updates.items()})
            backup["output_res"] = updates.get("output_res_ui", updates.get("output_res", "1920x1080"))
            self.parent_app.launcher_cfg["settings"] = backup
            save_config(self.parent_app.launcher_cfg)
            play_ui_sound.enabled=self.sound_effects_var.get(); self.parent_app.move_to_monitor(self.target_monitor)
            where = "game folder (Insert menu reads this file)" if GAME_DIR else "launcher folder (apply again after you select a game folder)"
            messagebox.showinfo("Settings Saved", f"MasterSword settings saved to:\n{path}\n\nand remembered in mastersword_launcher.json\n\n({where})", parent=self)
            self.destroy()
        except Exception as exc:messagebox.showerror("Settings","Could not save the MasterSword settings:\n"+str(exc),parent=self)
    def restore_defaults(self):
        if not messagebox.askyesno("Restore Defaults","Reset this page to the safe MasterSword default / performance baseline?",parent=self):return
        defaults={"fullscreen":"0","window_size":self.display_resolution,"output_res":"1920x1080","fps_limit":"60","vk_present":"mailbox","aspect":"fit","fov":"100",
            "upscaler":"off","upscaler_preset":"performance","fg_mode":"off","fg_mult":"2x","fg_preset":"B","fg_type":"fixed","render_scale":"game","sharpness":"0","reflex":"off","device_profile":"auto","pipe_wait":"default","pipe_prebuild":"1",
            "color_preset":"default","color_brightness":"0","color_contrast":"100","color_saturation":"100","color_gamma":"1.0",
            "fxaa":"1","realtime":"1",
            "experimental_geometry":"0","fix_vs_fault":"1","fix_zero":"1","lod_guard":"1","fix_indirect_reset":"1","lib_prebuild":"1",
            "pad_layout":"auto","motion":"1","rumble_strength":"130","rumble_curve":"0.6","rumble_triggers":"0","stick_deadzone":"8",
            "mouse_sens":"1","volume":"100","locale":"en-US","menu_lang":"en","assume_focus":"1"}
        for k,v in defaults.items():
            if k in self.vars:self.vars[k].set(v)
        self.sound_effects_var.set(True)

_initial_game=find_game_dir(LAUNCHER_CONFIG)
if _initial_game:
    set_game_dir(_initial_game)
else:
    INI_PATH = settings_ini_path(None)


class MasterSwordModsManagerDialog(tk.Toplevel):
    def __init__(self,parent):
        super().__init__(parent); self.parent_app=parent; self.title("MasterSword Mod Manager"); self.geometry("720x500"); self.minsize(600,380)
        self.configure(bg=COLOR_BG_DARK); self.transient(parent)
        head=tk.Frame(self,bg=COLOR_CARD,padx=22,pady=14); head.pack(fill=tk.X)
        tk.Label(head,text="MOD MANAGER",font=("Segoe UI",14,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD).pack(anchor="w")
        tk.Label(head,text="Toggle MasterSword RomFS overlay packages. Put each mod's RomFS tree in its own folder.",font=("Segoe UI",9),fg=COLOR_TEXT_DIM,bg=COLOR_CARD).pack(anchor="w",pady=(3,0))
        tk.Label(head,text="Enabled folders are composed into a separate overlay. If mods replace the same file, the later name alphabetically takes precedence.",font=("Segoe UI",8),fg=COLOR_TEXT_DIM,bg=COLOR_CARD,wraplength=640,justify="left").pack(anchor="w",pady=(4,0))
        body=tk.Frame(self,bg=COLOR_BG_DARK); body.pack(fill=tk.BOTH,expand=True,padx=20,pady=14)
        self.canvas=tk.Canvas(body,bg=COLOR_BG_DARK,highlightthickness=0); bar=ttk.Scrollbar(body,orient="vertical",command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=bar.set); bar.pack(side=tk.RIGHT,fill=tk.Y); self.canvas.pack(side=tk.LEFT,fill=tk.BOTH,expand=True)
        self.rows=tk.Frame(self.canvas,bg=COLOR_BG_DARK); self.win=self.canvas.create_window((0,0),window=self.rows,anchor="nw")
        self.rows.bind("<Configure>",lambda _e:self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.bind("<Configure>",lambda e:self.canvas.itemconfigure(self.win,width=e.width))
        foot=tk.Frame(self,bg=COLOR_CARD,height=60,padx=18); foot.pack(fill=tk.X,side=tk.BOTTOM); foot.pack_propagate(False)
        self.button(foot,"OPEN MODS FOLDER",self.open_mods,True).pack(side=tk.LEFT,pady=11)
        self.button(foot,"ADD MOD FOLDER",self.add_mod).pack(side=tk.LEFT,padx=10,pady=11)
        self.button(foot,"REFRESH",self.refresh).pack(side=tk.LEFT,padx=10,pady=11)
        self.button(foot,"CLOSE",self.destroy).pack(side=tk.RIGHT,pady=11); self.refresh()
    def button(self,p,text,cmd,primary=False):
        return tk.Button(p,text=text,command=cmd,font=("Segoe UI",8,"bold"),fg="#000" if primary else COLOR_WHITE,bg=COLOR_GOLD if primary else COLOR_BUTTON_HOVER,activebackground=COLOR_GOLD_HOVER,bd=0,padx=12,pady=7,cursor="hand2")
    def refresh(self):
        for c in self.rows.winfo_children():c.destroy()
        mods=scan_mod_folders()
        if not mods:
            tk.Label(self.rows,text="No mod folders found. Open the folder below and add one folder for each RomFS overlay package.",font=("Segoe UI",10),fg=COLOR_TEXT_DIM,bg=COLOR_BG_DARK,wraplength=560,justify="left").pack(anchor="w",padx=10,pady=18); return
        for mod in mods:
            row=tk.Frame(self.rows,bg=COLOR_CARD,padx=12,pady=10,highlightthickness=1,highlightbackground=COLOR_CARD_BORDER); row.pack(fill=tk.X,pady=4)
            var=tk.BooleanVar(value=mod["enabled"])
            tk.Checkbutton(row,text=mod["name"],variable=var,command=lambda p=mod["path"],v=var:self.toggle(p,v),font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_CARD,activebackground=COLOR_CARD,activeforeground=COLOR_GOLD,selectcolor=COLOR_TAB_ACTIVE,bd=0,highlightthickness=0).pack(side=tk.LEFT)
            tk.Label(row,text="ENABLED" if mod["enabled"] else "DISABLED",font=("Segoe UI",8,"bold"),fg=COLOR_ACCENT_GREEN if mod["enabled"] else COLOR_GOLD,bg=COLOR_CARD).pack(side=tk.RIGHT,padx=8)
    def toggle(self,path,var):
        warning=game_process_warning("changing MasterSword mods")
        if warning:messagebox.showwarning("Close the Game First",warning,parent=self); self.refresh(); return
        try:set_mod_folder_enabled(path,var.get()); rebuild_mod_overlay(); self.refresh()
        except Exception as exc:messagebox.showerror("Mod Update Failed",str(exc),parent=self); self.refresh()
    def open_mods(self):
        if not GAME_DIR:messagebox.showerror("Mods","Select a MasterSword game folder first.",parent=self); return
        open_folder(MODS_DIR)
    def add_mod(self):
        if not GAME_DIR:messagebox.showerror("Mods","Select a MasterSword game folder first.",parent=self); return
        try:create_mod_folder(); self.refresh(); open_folder(MODS_DIR)
        except Exception as exc:messagebox.showerror("Mods",str(exc),parent=self)


class MasterSwordSaveBackupVaultDialog(tk.Toplevel):
    def __init__(self,parent):
        super().__init__(parent); self.title("MasterSword Save Backup Vault"); self.geometry("680x460"); self.minsize(560,360)
        self.configure(bg=COLOR_BG_DARK); self.transient(parent); self.profile_dir=PROFILE_DIR; self.backups_dir=PROFILE_DIR/"backups"; self.snapshots=[]
        head=tk.Frame(self,bg=COLOR_CARD,padx=20,pady=14); head.pack(fill=tk.X)
        tk.Label(head,text="SAVE BACKUP VAULT",font=("Segoe UI",13,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD).pack(anchor="w")
        tk.Label(head,text=f"Snapshots are stored in {self.backups_dir}.",font=("Segoe UI",9),fg=COLOR_TEXT_DIM,bg=COLOR_CARD,wraplength=630,justify="left").pack(anchor="w",pady=(3,0))
        body=tk.Frame(self,bg=COLOR_BG_DARK); body.pack(fill=tk.BOTH,expand=True,padx=20,pady=14)
        tk.Label(body,text="Available snapshots",font=("Segoe UI",10,"bold"),fg=COLOR_WHITE,bg=COLOR_BG_DARK).pack(anchor="w",pady=(0,8))
        self.list=tk.Listbox(body,bg=COLOR_CARD,fg=COLOR_WHITE,selectbackground=COLOR_TAB_ACTIVE,selectforeground=COLOR_GOLD,font=("Segoe UI",9),highlightthickness=1,highlightbackground=COLOR_CARD_BORDER,activestyle="none"); self.list.pack(fill=tk.BOTH,expand=True)
        foot=tk.Frame(self,bg=COLOR_CARD,padx=14,pady=12); foot.pack(fill=tk.X,side=tk.BOTTOM)
        self.button(foot,"CREATE BACKUP SNAPSHOT",self.create,True).pack(side=tk.LEFT)
        self.button(foot,"RESTORE BACKUP",self.restore).pack(side=tk.LEFT,padx=8)
        self.button(foot,"OPEN BACKUPS FOLDER",self.open_backups).pack(side=tk.RIGHT); self.refresh()
    def button(self,p,text,cmd,primary=False):
        return tk.Button(p,text=text,command=cmd,font=("Segoe UI",8,"bold"),fg="#000" if primary else COLOR_WHITE,bg=COLOR_GOLD if primary else COLOR_BUTTON_HOVER,activebackground=COLOR_GOLD_HOVER,bd=0,padx=11,pady=8)
    def refresh(self):
        self.backups_dir.mkdir(parents=True,exist_ok=True)
        self.snapshots=sorted((p for p in self.backups_dir.iterdir() if p.is_dir() and p.name.startswith("save_")),key=lambda p:p.name,reverse=True)
        self.list.delete(0,tk.END)
        for p in self.snapshots:self.list.insert(tk.END,p.name)
        if not self.snapshots:self.list.insert(tk.END,"No save snapshots yet")
    def create(self):
        warning=game_process_warning("creating a save snapshot")
        if warning:messagebox.showwarning("Close the Game First",warning,parent=self); return
        try:p=create_save_snapshot(self.profile_dir,self.backups_dir); self.refresh(); messagebox.showinfo("Backup Created",f"Save snapshot created:\n{p.name}",parent=self)
        except Exception as exc:messagebox.showerror("Backup Failed",str(exc),parent=self)
    def restore(self):
        sel=self.list.curselection()
        if not sel or sel[0]>=len(self.snapshots):messagebox.showinfo("Select a Backup","Select a save snapshot to restore.",parent=self); return
        warning=game_process_warning("restoring a save snapshot")
        if warning:messagebox.showwarning("Close the Game First",warning,parent=self); return
        snap=self.snapshots[sel[0]]
        if not messagebox.askyesno("Confirm Save Restore",f"Restore {snap.name}? The current save is backed up first.",parent=self):return
        try:safety=restore_save_snapshot(self.profile_dir,snap); self.refresh(); messagebox.showinfo("Backup Restored",f"Restored {snap.name}.\nSafety snapshot: {safety.name}",parent=self)
        except Exception as exc:messagebox.showerror("Restore Failed",str(exc),parent=self)
    def open_backups(self):self.backups_dir.mkdir(parents=True,exist_ok=True); open_folder(self.backups_dir)


class MasterSwordLauncherApp(tk.Tk):
    def __init__(self):
        super().__init__(); self.title(APP_TITLE); self.geometry("1120x630"); self.resizable(False,False); self.configure(bg=COLOR_BG_DARK)
        self.protocol("WM_DELETE_WINDOW",self.safe_quit)
        self.reload_user_state()
        icon=resource_dir()/"assets"/"mastersword.ico"
        if icon.is_file():
            try:self.iconbitmap(str(icon))
            except Exception:pass
        self.update_idletasks(); w,h=1120,630; self.geometry(f"{w}x{h}+{(self.winfo_screenwidth()-w)//2}+{(self.winfo_screenheight()-h)//2}")
        play_ui_sound.enabled=bool(self.launcher_cfg.get("launcher_sound_effects",True))
        self.latest_release=None; self.update_in_progress=False; self.setup_background(); self.setup_ui_overlay(); self.after(600, self.check_for_updates)
        if self.launcher_cfg.get("target_display"):self.move_to_monitor(self.target_monitor)
    def reload_user_state(self):
        """Re-check game folder + user settings on every launcher start (and before Settings / Play)."""
        global LAUNCHER_CONFIG, GAME_DIR, INI_PATH
        LAUNCHER_CONFIG = load_config()
        self.launcher_cfg = LAUNCHER_CONFIG
        game = find_game_dir(self.launcher_cfg)
        if game:
            set_game_dir(game)
        elif GAME_DIR and valid_game_dir(GAME_DIR):
            set_game_dir(GAME_DIR)
        else:
            INI_PATH = settings_ini_path(None)
        monitors = enumerate_displays()
        self.target_monitor = next(
            (m for m in monitors if m["device"] == self.launcher_cfg.get("target_display")),
            next((m for m in monitors if m["primary"]), monitors[0]),
        )
        self.user_settings = load_user_settings(GAME_DIR, self.target_monitor["resolution"])
        return self.user_settings
    def safe_quit(self):
        try:
            self.launcher_cfg["show_launcher"] = bool(self.keep_launcher_var.get()) if hasattr(self, "keep_launcher_var") else self.launcher_cfg.get("show_launcher", True)
            if GAME_DIR:
                self.launcher_cfg["game_dir"] = str(Path(GAME_DIR).resolve())
            if getattr(self, "target_monitor", None):
                self.launcher_cfg["target_display"] = self.target_monitor["device"]
            save_config(self.launcher_cfg)
        except Exception:
            pass
        self.destroy()
    def move_to_monitor(self,monitor):place_window_on_monitor(self,monitor)
    def setup_background(self):
        self.canvas=tk.Canvas(self,width=1120,height=630,highlightthickness=0,bg=COLOR_BG_DARK); self.canvas.pack(fill=tk.BOTH,expand=True)
        path = find_asset("mastersword_background.png") or find_asset("mastersword_background.jpg")
        if not path: return
        try:
            im=Image.open(path).convert("RGBA").resize((1120,630),Image.Resampling.LANCZOS)
            overlay=Image.new("RGBA",im.size,(0,0,0,0)); draw=ImageDraw.Draw(overlay)
            for x in range(570):
                alpha=int(225*(1-(x/570)**0.82)); draw.line((x,0,x,630),fill=(7,12,10,alpha))
            draw.rectangle((0,500,1120,630),fill=(7,11,9,85))
            self.bg_img=ImageTk.PhotoImage(Image.alpha_composite(im,overlay)); self.canvas.create_image(0,0,image=self.bg_img,anchor="nw")
        except Exception as exc:print("Could not load launcher artwork:",exc)
    def _menu_bind(self,item,callback,primary=False):
        base=COLOR_GOLD if primary else COLOR_WHITE
        item.bind("<Enter>",lambda _e:(play_ui_sound("hover"),item.config(fg=COLOR_GOLD_HOVER if primary else COLOR_GOLD),self.config(cursor="hand2")))
        item.bind("<Leave>",lambda _e:(item.config(fg=base),self.config(cursor="")))
        item.bind("<Button-1>",lambda _e:(play_ui_sound("click"),callback()))
    def setup_ui_overlay(self):
        self.canvas.create_text(70,62,text="STEELTEMPSAINT'S MASTERSWORD",font=("Segoe UI",11,"bold"),fill=COLOR_GOLD,anchor="w")
        self.canvas.create_text(70,104,text="OCARINA OF TIME",font=("Impact",34),fill=COLOR_WHITE,anchor="w")
        self.canvas.create_text(70,141,text="PC RECOMPILED  ·  STEEL TEMPLE MASTERSWORD EDITION",font=("Segoe UI",10,"bold"),fill=COLOR_ACCENT_GREEN,anchor="w")
        for i,(label,cb,primary) in enumerate([("PLAY",self.launch_game,True),("SETTINGS",self.open_settings,False),("DIGITAL EXTRAS",self.open_extras,False),("QUIT",self.safe_quit,False)]):
            item=tk.Label(self,text=label,font=("Segoe UI",22,"bold"),fg=COLOR_GOLD if primary else COLOR_WHITE,bg=COLOR_BG_DARK,cursor="hand2")
            self.canvas.create_window(70,230+i*62,window=item,anchor="w"); self._menu_bind(item,cb,primary)
        footer=tk.Frame(self,bg=COLOR_CARD,bd=1,relief="solid",padx=16,pady=10)
        self.keep_launcher_var=tk.BooleanVar(value=self.launcher_cfg.get("show_launcher",True))
        tk.Checkbutton(footer,text="KEEP LAUNCHER OPEN",variable=self.keep_launcher_var,command=self._save_keep_launcher,font=("Segoe UI",9,"bold"),fg=COLOR_WHITE,bg=COLOR_CARD,activebackground=COLOR_CARD,activeforeground=COLOR_GOLD,selectcolor=COLOR_CARD_BORDER,bd=0,highlightthickness=0).pack(anchor="e")
        tk.Label(footer,text=VERSION_STRING,font=("Segoe UI",9,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD).pack(anchor="e",pady=(4,0))
        self.update_status=tk.Label(footer,text="",font=("Segoe UI",8),fg=COLOR_ACCENT_GREEN,bg=COLOR_CARD,wraplength=235,justify="right"); self.update_status.pack(anchor="e",pady=(4,0))
        self.update_action_btn=tk.Button(footer,text="⚡ UPDATE NOW (1-CLICK)",command=self.apply_latest_update,font=("Segoe UI",8,"bold"),fg="#000",bg=COLOR_ACCENT_GREEN,activebackground="#8ef0a5",bd=0,padx=10,pady=6,cursor="hand2"); self.update_action_btn.pack_forget()
        tk.Button(footer,text="CHECK FOR UPDATES",command=self.check_for_updates,font=("Segoe UI",8,"bold"),fg=COLOR_WHITE,bg=COLOR_BUTTON_HOVER,activebackground=COLOR_GOLD,activeforeground="#000",bd=0,padx=10,pady=5,cursor="hand2").pack(anchor="e",pady=(6,0))
        self.release_btn=tk.Button(footer,text="VIEW RELEASE NOTES",command=self.open_latest_release,font=("Segoe UI",8,"bold"),fg="#000",bg=COLOR_GOLD,activebackground=COLOR_GOLD_HOVER,bd=0,padx=10,pady=6,cursor="hand2"); self.release_btn.pack_forget()
        self.canvas.create_window(1050,565,window=footer,anchor="se")
        repo=tk.Button(self.canvas,text="★ GitHub Repository (STS78721)",font=("Segoe UI",9,"bold"),fg=COLOR_GOLD,bg=COLOR_CARD,activebackground=COLOR_CARD_BORDER,activeforeground=COLOR_GOLD_HOVER,bd=1,relief="solid",padx=14,pady=6,cursor="hand2",command=lambda:webbrowser.open(GITHUB_URL)); self.canvas.create_window(70,570,window=repo,anchor="w")
        self.canvas.create_text(70,608,text=f"{EDITION_NAME.upper()}  ·  USE YOUR OWN GAME DATA",font=("Segoe UI",8,"bold"),fill=COLOR_TEXT_DIM,anchor="w")
    def _save_keep_launcher(self):self.launcher_cfg["show_launcher"]=self.keep_launcher_var.get(); save_config(self.launcher_cfg)
    def choose_game_directory(self):
        selected=filedialog.askdirectory(title="Select the folder containing mastersword.exe")
        if not selected:return
        if not valid_game_dir(selected):messagebox.showerror("MasterSword Game Folder","Choose the folder that contains mastersword.exe.",parent=self); return
        self.launcher_cfg["game_dir"]=str(Path(selected).resolve()); save_config(self.launcher_cfg); set_game_dir(selected)
        self.update_status.config(text="MasterSword game folder selected.",fg=COLOR_ACCENT_GREEN)
    def _ensure_game(self):
        if GAME_DIR and valid_game_dir(GAME_DIR):return True
        messagebox.showinfo("Select MasterSword Game Folder","Choose the folder containing your locally built mastersword.exe.",parent=self); self.choose_game_directory()
        return bool(GAME_DIR and valid_game_dir(GAME_DIR))
    def _pack_loose_romfs_if_needed(self, romfs_dir: Path, dst_bin: Path) -> bool:
        if not romfs_dir.is_dir():
            return False
        loose_files = [f for f in os.listdir(romfs_dir) if f not in ("romfs.bin", "index.json", ".romfs-verified-v1") and not f.startswith(".")]
        if not loose_files:
            return False
        if not messagebox.askyesno(
            "Pack Loose RomFS Data?",
            f"Loose game files were found in:\n{romfs_dir}\n\n"
            f"Ocarina of Time PC Recomp requires these files to be packed into 'romfs/romfs.bin'.\n\n"
            f"Would you like MasterSword Launcher to pack 'romfs/romfs.bin' automatically now? (Takes ~1–2 minutes)",
            parent=self,
        ):
            return False
        self.update_status.config(text="Packing romfs/romfs.bin from loose game files…", fg=COLOR_GOLD)
        self.update_idletasks()
        try:
            align = 16
            chunk_sz = 8 << 20
            files_list = []
            for root_dir, _, fnames in os.walk(romfs_dir):
                fnames.sort()
                for fname in fnames:
                    if not fname.startswith(".") and fname not in ("romfs.bin", "index.json", ".romfs-verified-v1"):
                        fp = Path(root_dir) / fname
                        files_list.append((fp.relative_to(romfs_dir).as_posix(), fp))
            files_list.sort(key=lambda e: e[0].encode())
            entries, off = [], 0
            temp_bin = str(dst_bin) + ".tmp"
            sha = hashlib.sha256()
            total_cnt = len(files_list)
            with open(temp_bin, "wb") as dst_f:
                for idx_f, (rel_p, full_p) in enumerate(files_list):
                    pad = -off % align
                    if pad:
                        dst_f.write(b"\0" * pad)
                        sha.update(b"\0" * pad)
                        off += pad
                    sz = 0
                    with open(full_p, "rb") as src_f:
                        while chunk := src_f.read(chunk_sz):
                            dst_f.write(chunk)
                            sha.update(chunk)
                            sz += len(chunk)
                    entries.append({"path": rel_p, "offset": off, "size": sz})
                    off += sz
                    if idx_f % 5000 == 0:
                        self.update_status.config(text=f"Packing romfs.bin: {idx_f}/{total_cnt} files ({off / (1024**3):.1f} GB)…", fg=COLOR_GOLD)
                        self.update_idletasks()
            if dst_bin.exists():
                dst_bin.unlink()
            os.rename(temp_bin, dst_bin)
            manifest = {"status": "pass", "source": "packed from dump", "romfs_size": off, "romfs_sha256": sha.hexdigest(), "files": entries}
            with open(romfs_dir / "index.json", "w", encoding="utf-8") as mf:
                json.dump(manifest, mf)
            self.update_status.config(text="romfs.bin packed and ready!", fg=COLOR_ACCENT_GREEN)
            return True
        except Exception as exc:
            messagebox.showerror("RomFS Pack Failed", f"Could not pack romfs/romfs.bin:\n{exc}", parent=self)
            return False

    def _game_bar_hint_once(self):
        """(0.1.9c) First PLAY only: how to tell Windows that MasterSword is a game (Game Bar's "Remember this is a game").
        Windows gives a program it knows as a game its game scheduling and Game Mode; on a 270K Plus it made play much smoother.
        Windows has no public way for a program to mark itself, and the launcher changes no Windows setting: the player ticks it."""
        if sys.platform != "win32" or not isinstance(LAUNCHER_CONFIG, dict) or LAUNCHER_CONFIG.get("game_bar_hint_shown"):
            return
        again = messagebox.askyesno(
            "Smoother play: tell Windows this is a game",
            "Windows gives programs it knows as games better CPU scheduling (Game Mode).\n"
            "MasterSword is not on its list yet. It takes ten seconds, once:\n\n"
            "  1. Start the game (it starts after this message) and click into its window.\n"
            "  2. Press Win + G to open the Game Bar.\n"
            "  3. Open Settings (the gear icon) > General and tick \"Remember this is a game\".\n\n"
            "Nothing is changed for you: it is your Windows setting, and you can untick it the same way.\n\n"
            "Show this again next time?",
            parent=self,
        )
        if not again:
            LAUNCHER_CONFIG["game_bar_hint_shown"] = True
            try:
                save_config(LAUNCHER_CONFIG)
            except Exception:
                pass

    def launch_game(self):
        self.reload_user_state()
        if not self._ensure_game():return
        # Hookshot / MasterSword needs the dump BESIDE the exe:
        #   mastersword.exe  +  image.bin  +  romfs\  (+ user\ created automatically)
        # Exit code 2 from the exe is "usage" = image.bin not found — NOT missing DLLs.
        image_bin = GAME_DIR / "image.bin"
        romfs_dir = GAME_DIR / "romfs"
        user_dir = GAME_DIR / "user"
        if not image_bin.is_file() or not romfs_dir.is_dir():
            messagebox.showerror(
                "Game Data Missing",
                f"mastersword.exe needs your dump in the same folder as the game:\n\n"
                f"  {GAME_DIR}\n\n"
                f"Required:\n"
                f"  • image.bin\n"
                f"  • romfs\\   (folder)\n\n"
                f"Copy those from your legally obtained dump next to mastersword.exe, then hit PLAY again.\n\n"
                f"(Exit code 2 / the usage line means image.bin was not found — the runtime DLLs are fine.)",
                parent=self,
            )
            return
        user_dir.mkdir(parents=True, exist_ok=True)
        env=os.environ.copy(); _gpu,vendor=detect_gpu()
        if vendor!="Other" and not env.get("MasterSword_GPU_NAME"):env["MasterSword_GPU_NAME"]="NVIDIA" if vendor=="NVIDIA" else vendor
        # Host reads HOS_WINDOW_SIZE at window create — mirror launcher Window Resolution into the env.
        ini_vals = parse_ini(settings_ini_path(GAME_DIR))
        backup = (LAUNCHER_CONFIG.get("settings") or {}) if isinstance(LAUNCHER_CONFIG, dict) else {}
        win = str(ini_vals.get("window_size") or backup.get("window_size") or "").strip().replace(" ", "")
        if "x" in win.lower() and not env.get("HOS_WINDOW_SIZE"):
            env["HOS_WINDOW_SIZE"] = win
        if str(ini_vals.get("fullscreen", backup.get("fullscreen", "0"))).strip() in {"1", "true", "on"}:
            env.setdefault("HOS_FULLSCREEN", "1")
        # Always pass explicit paths so cwd / working-directory mistakes cannot hide the dump.
        command = [
            str(GAME_DIR / "mastersword.exe"),
            str(image_bin),
            "--romfs", str(romfs_dir),
            "--user", str(user_dir),
        ]
        self._game_bar_hint_once()
        try:
            extra=[]
            if len(sys.argv)>1:extra=[a for a in sys.argv[1:] if a!="--launcher"]
            command += extra
            p=subprocess.Popen(command,cwd=str(GAME_DIR),env=env)
            time.sleep(0.8)
            code = p.poll()
            if code is not None and code != 0:
                if code == 2:
                    msg = (
                        f"mastersword.exe exited with code 2 (usage / no image).\n\n"
                        f"Put image.bin and a romfs\\ folder next to:\n  {GAME_DIR / 'mastersword.exe'}\n\n"
                        f"Then try PLAY again."
                    )
                else:
                    msg = (
                        f"mastersword.exe exited immediately (code {code}).\n\n"
                        f"If Windows also showed a missing-DLL dialog, install "
                        f"SteelTemple-MasterSword-{CURRENT_VERSION}-Update.zip.\n"
                        f"Otherwise check mastersword_log.txt / stderr in the game folder."
                    )
                messagebox.showerror("Launch Failed", msg, parent=self)
                return
            if not self.keep_launcher_var.get():
                self.withdraw()
                self.after(2500,self.safe_quit)
            if p.pid:
                threading.Thread(target=self._place_game_window,args=(p.pid,self.target_monitor),daemon=True).start()
        except Exception as exc:messagebox.showerror("Launch Error",f"Could not start mastersword.exe:\n{exc}",parent=self)
    def _place_game_window(self,pid,monitor):
        if os.name!="nt":return
        try:
            user32=ctypes.windll.user32; found=[]; callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
            def visit(hwnd,_):
                proc=wintypes.DWORD(); user32.GetWindowThreadProcessId(hwnd,ctypes.byref(proc))
                if proc.value==pid and user32.IsWindowVisible(hwnd):found.append(hwnd)
                return True
            for _ in range(80):
                found.clear(); user32.EnumWindows(callback(visit),0)
                if found:break
                time.sleep(.25)
            if found:
                hwnd = found[0]
                user32.SetForegroundWindow(hwnd)
                user32.BringWindowToTop(hwnd)
        except Exception:pass
    def check_for_updates(self):
        if getattr(self, "release_check_active", False) or getattr(self, "update_in_progress", False): return
        self.release_check_active = True
        self.update_action_btn.pack_forget()
        self.release_btn.pack_forget()
        self.update_status.config(text="Checking GitHub Releases…", fg=COLOR_TEXT_DIM)
        threading.Thread(target=self._check_release_worker, daemon=True).start()

    def _check_release_worker(self):
        try:
            req = urllib.request.Request(RELEASES_API_URL, headers={"Accept": "application/vnd.github+json", "User-Agent": "MasterSwordLauncher"})
            with urllib.request.urlopen(req, timeout=12) as r:
                data = json.loads(r.read().decode("utf-8"))
            tag = (data.get("tag_name") or "").strip()
            title = (data.get("name") or tag or "Latest MasterSword release").strip()
            page = data.get("html_url") or RELEASES_URL
            if not tag:
                raise ValueError("The latest release did not include a version tag.")
            assets = data.get("assets", [])
            tag_clean = tag.replace("-", "").replace(".", "")
            candidates = [
                f"SteelTemple-MasterSword-{tag}-Update.zip",
                f"SteelTemple-MasterSword-{tag_clean}-Update.zip",
                f"SteelTemple-MasterSword-Update-{tag}.zip",
                f"dkb-{tag}-update.zip",
            ]
            asset = next((item for item in assets for cand in candidates if item.get("name", "").casefold() == cand.casefold()), None)
            if not asset:
                asset = next((item for item in assets if "update" in item.get("name", "").lower() and item.get("name", "").lower().endswith(".zip")), None)
            asset_name = asset.get("name") if asset else None
            asset_url = asset.get("browser_download_url") if asset else None
            self.after(0, lambda: self._show_release_result(tag, title, page, asset_url, asset_name))
        except Exception as exc:
            detail = str(exc)
            if "404" in detail or "Not Found" in detail:
                self.after(0, lambda: self._show_release_result(CURRENT_VERSION, f"Your MasterSword is up to date ({CURRENT_VERSION}).", RELEASES_URL, None, None))
            else:
                self.after(0, lambda: self._show_release_error("Update check failed: " + detail))

    def _show_release_error(self, message):
        self.release_check_active = False
        self.update_status.config(text=message, fg="#ff7373")

    def _show_release_result(self, tag, title, page, asset_url=None, asset_name=None):
        self.release_check_active = False
        self.latest_release = {"tag": tag, "title": title, "page": page, "asset_url": asset_url, "asset_name": asset_name}
        clean_title = title.split(":", 1)[0].strip()
        if is_up_to_date(CURRENT_VERSION, tag) or is_same_version(CURRENT_VERSION, clean_title):
            self.update_status.config(text=f"Your MasterSword is up to date ({CURRENT_VERSION})", fg=COLOR_ACCENT_GREEN)
            self.update_action_btn.pack_forget()
            self.release_btn.pack_forget()
            return
        if not asset_url:
            self.update_status.config(text=f"Update found ({clean_title}), but update zip is missing.", fg=COLOR_GOLD)
            self.update_action_btn.pack_forget()
            self.release_btn.config(text="VIEW RELEASE NOTES")
            self.release_btn.pack(anchor="e", pady=(5, 0))
            return
        self.update_status.config(text=f"Update available: {clean_title} — installing…", fg=COLOR_GOLD)
        self.update_action_btn.config(state=tk.DISABLED, text="AUTO-INSTALLING…")
        self.update_action_btn.pack(anchor="e", pady=(5, 0))
        self.release_btn.config(text="VIEW RELEASE NOTES")
        self.release_btn.pack(anchor="e", pady=(3, 0))
        # SteelTemple style: check on launch, then pull + install automatically when an update zip exists.
        if self.launcher_cfg.get("auto_install_updates", True):
            self.after(400, lambda: self.apply_latest_update(auto=True))

    def open_latest_release(self):
        url = self.latest_release["page"] if (self.latest_release and self.latest_release.get("page")) else RELEASES_URL
        webbrowser.open(url)

    def apply_latest_update(self, auto=False):
        if getattr(self, "update_in_progress", False) or not self.latest_release:
            return
        warning = game_process_warning("applying a launcher/game update")
        if warning:
            messagebox.showwarning("Close the Game First", warning, parent=self)
            return
        title = self.latest_release.get("title", "MasterSword Update")
        if not auto and not messagebox.askyesno(
            "Install MasterSword Update",
            f"Download and install {title} automatically now?\n\nThis will update your game executable, launcher, and pre-compiled Vulkan shaders.",
            parent=self,
        ):
            return
        self.update_in_progress = True
        self._auto_installing = bool(auto)
        self.update_action_btn.config(state=tk.DISABLED, text="DOWNLOADING UPDATE…")
        self.update_status.config(text="Connecting to GitHub Releases…", fg=COLOR_TEXT_DIM)
        threading.Thread(target=self._download_update_worker, args=(self.latest_release.copy(),), daemon=True).start()

    def _download_update_worker(self, release):
        stage = None
        try:
            target = GAME_DIR if GAME_DIR else app_dir()
            if not GAME_DIR:
                self._set_update_status("No game folder yet — updating launcher files in place…", COLOR_TEXT_DIM)
            target.mkdir(parents=True, exist_ok=True)
            stage = Path(tempfile.mkdtemp(prefix=".mastersword-update-", dir=str(target)))
            archive_path = stage / release["asset_name"]
            request = urllib.request.Request(release["asset_url"], headers={"User-Agent": "MasterSwordLauncher", "Accept": "application/octet-stream"})
            with urllib.request.urlopen(request, timeout=60) as response, archive_path.open("wb") as output:
                total = int(response.headers.get("Content-Length", "0") or 0)
                downloaded = 0
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    output.write(block)
                    downloaded += len(block)
                    if total:
                        percent = min(100, downloaded * 100 // total)
                        self._set_update_status(f"Downloading update… {percent}%", COLOR_TEXT_DIM)
            self._set_update_status("Validating update archive…", COLOR_TEXT_DIM)
            files = extract_update_archive(archive_path, stage)
            archive_path.unlink(missing_ok=True)
            lower = {f.lower() for f in files}
            if "masterswordlauncher.exe" not in lower and "mastersword.exe" not in lower:
                raise ValueError("The validated update archive is missing MasterSwordLauncher.exe / mastersword.exe.")
            if (stage / "mastersword.exe").is_file() and GAME_DIR:
                self._set_update_status("Verifying updated game executable…", COLOR_TEXT_DIM)
                validate_update_game_executable(stage / "mastersword.exe", GAME_DIR)
            # Relauncher installs into GAME_DIR when set; otherwise into the launcher folder.
            if not GAME_DIR:
                set_game_dir(target)
            self.after(0, lambda: self._update_ready(release, stage, files))
        except Exception as exc:
            if stage:
                shutil.rmtree(stage, ignore_errors=True)
            detail = str(exc)
            self.after(0, lambda: self._update_failed(detail))

    def _set_update_status(self, text, color):
        try:
            self.after(0, lambda: self.update_status.config(text=text, fg=color))
        except tk.TclError:
            pass

    def _update_failed(self, detail):
        self.update_in_progress = False
        self.update_action_btn.config(state=tk.NORMAL, text="⚡ UPDATE NOW (1-CLICK)")
        self.update_status.config(text=f"Update failed:\n{detail[:100]}", fg="#ff7373")

    def _update_ready(self, release, stage, files):
        title = release.get("title", "MasterSword Update")
        auto = getattr(self, "_auto_installing", False)
        if not auto and not messagebox.askyesno(
            "Update Ready",
            f"{title} is downloaded and verified.\n\nInstall it now? The launcher will close, apply the files, and restart automatically.",
            parent=self,
        ):
            shutil.rmtree(stage, ignore_errors=True)
            self.update_in_progress = False
            self.update_action_btn.config(state=tk.NORMAL, text="⚡ UPDATE NOW (1-CLICK)")
            self.update_status.config(text="Update downloaded; installation cancelled.", fg=COLOR_TEXT_DIM)
            return
        try:
            self._start_update_relauncher(stage, title)
            os._exit(0)
        except Exception as exc:
            shutil.rmtree(stage, ignore_errors=True)
            self._update_failed(str(exc))

    def _start_update_relauncher(self, stage, title):
        def ps_quote(value):
            return "'" + str(value).replace("'", "''") + "'"
        game_path = ps_quote(GAME_DIR.resolve())
        stage_path = ps_quote(Path(stage).resolve())
        title_text = ps_quote(title)
        script = f'''
$ErrorActionPreference = 'Stop'
$game = {game_path}
$stage = {stage_path}
$rollback = Join-Path $stage '_rollback'
New-Item -ItemType Directory -Force -Path $rollback | Out-Null
$changed = [System.Collections.Generic.List[string]]::new()
try {{
    Wait-Process -Id {os.getpid()} -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    Get-ChildItem -LiteralPath $stage -File | ForEach-Object {{
        $name = $_.Name
        if ($name.StartsWith('_') -or $name.EndsWith('.tmp') -or $name.EndsWith('.zip')) {{ return }}
        $source = $_.FullName
        $target = Join-Path $game $name
        $backup = Join-Path $rollback $name
        if (Test-Path -LiteralPath $target -PathType Leaf) {{ Copy-Item -LiteralPath $target -Destination $backup -Force }}
        $changed.Add($name)
        $newFile = $target + '.update-new'
        Copy-Item -LiteralPath $source -Destination $newFile -Force
        Move-Item -LiteralPath $newFile -Destination $target -Force
    }}
}} catch {{
    try {{ Add-Type -AssemblyName System.Windows.Forms }} catch {{}}
    for ($i = $changed.Count - 1; $i -ge 0; $i--) {{
        $name = $changed[$i]; $target = Join-Path $game $name; $backup = Join-Path $rollback $name
        if (Test-Path -LiteralPath $backup -PathType Leaf) {{ Copy-Item -LiteralPath $backup -Destination $target -Force }}
        else {{ Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue }}
    }}
    [System.Windows.Forms.MessageBox]::Show(('Update failed and changes were rolled back: ' + $_.Exception.Message), 'Update Failed') | Out-Null
    exit 1
}}
try {{ Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.MessageBox]::Show(({title_text} + ' installed successfully. The launcher will restart.'), 'Update Complete') | Out-Null }} catch {{}}
$targetLauncher = Join-Path $game 'MasterSwordLauncher.exe'
if (Test-Path -LiteralPath $targetLauncher -PathType Leaf) {{
    Start-Process -FilePath $targetLauncher -WorkingDirectory $game
}}
Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
'''
        encoded = base64.b64encode(script.encode("utf-16le")).decode("ascii")
        flags = getattr(subprocess, "DETACHED_PROCESS", 0)
        subprocess.Popen(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden",
                          "-EncodedCommand", encoded], cwd=str(GAME_DIR), creationflags=flags,
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    def open_settings(self):
        # Re-check game folder + saved settings every time Settings opens (after exit / Insert-menu edits).
        self.reload_user_state()
        if INI_PATH is None:
            globals()["INI_PATH"] = settings_ini_path(None)
        MasterSwordSettingsDialog(self)
    def open_extras(self):
        menu=tk.Menu(self,tearoff=0,bg=COLOR_CARD,fg=COLOR_WHITE,activebackground=COLOR_GOLD,activeforeground="#000",font=("Segoe UI",10,"bold"),bd=1)
        menu.add_command(label="🌐  Visit SteelTempSaint MasterSword (GitHub)",command=lambda:webbrowser.open(GITHUB_URL))
        menu.add_command(label="⬆  Check for SteelTemple Releases",command=self.check_for_updates)
        menu.add_command(label="🧩  Open Mod Manager",command=lambda:MasterSwordModsManagerDialog(self))
        menu.add_command(label="🛡  Save Backup Vault",command=self.open_save_vault); menu.add_separator()
        for label,path in (("📁  Open Mods Folder","mods"),("💾  Open Save Data Folder","save"),("🔮  Open Shader Cache","shader-cache"),("📄  Open MasterSword Logs","logs")):
            menu.add_command(label=label,command=lambda p=path:self.open_game_folder(p))
        menu.add_separator(); menu.add_command(label="🎮  View Controller Guide",command=self.open_controller_guide)
        menu.tk_popup(self.winfo_pointerx(),self.winfo_pointery())
    def open_game_folder(self,relative):
        if not self._ensure_game():return
        path=PROFILE_DIR/"save" if relative=="save" else GAME_DIR/relative; open_folder(path)
    def open_save_vault(self):
        if not self._ensure_game():return
        MasterSwordSaveBackupVaultDialog(self)
    def open_controller_guide(self):
        messagebox.showinfo("Controller Guide","MasterSword PC Recomp controller notes\n\nConnect a controller before launching. SDL gamepads are supported; button prompts can be changed in Settings → Controls & Audio.\n\nMotion input, rumble, trigger rumble, and button prompts depend on the controller and MasterSword build.",parent=self)


def main():
    app=MasterSwordLauncherApp(); app.mainloop()

if __name__=="__main__":main()
