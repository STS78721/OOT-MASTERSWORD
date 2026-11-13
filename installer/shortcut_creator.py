"""Desktop shortcut creator for Ocarina of Time · MasterSword PC Recompiled."""

import os
import subprocess

def create_desktop_shortcut(game_dir: str, shortcut_name: str = "Ocarina of Time · MasterSword", target_exe: str = None) -> bool:
    """Creates a Windows desktop shortcut pointing to MasterSwordLauncher.exe (or mastersword.exe) in game_dir."""
    try:
        desktop_dir = os.path.join(os.environ.get("USERPROFILE", ""), "Desktop")
        if not os.path.exists(desktop_dir):
            desktop_dir = os.path.join(os.path.expanduser("~"), "Desktop")

        shortcut_path = os.path.join(desktop_dir, f"{shortcut_name}.lnk")
        
        # Prefer MasterSwordLauncher.exe if present
        if target_exe:
            chosen_exe = os.path.join(game_dir, target_exe)
        elif os.path.isfile(os.path.join(game_dir, "MasterSwordLauncher.exe")):
            chosen_exe = os.path.join(game_dir, "MasterSwordLauncher.exe")
        elif os.path.isfile(os.path.join(game_dir, "..", "MasterSwordLauncher.exe")):
            chosen_exe = os.path.abspath(os.path.join(game_dir, "..", "MasterSwordLauncher.exe"))
        else:
            chosen_exe = os.path.join(game_dir, "mastersword.exe")

        icon_path = os.path.join(game_dir, "mastersword.ico")
        if not os.path.exists(icon_path):
            icon_path = chosen_exe

        ps_script = f"""
        $ws = New-Object -ComObject WScript.Shell
        $s = $ws.CreateShortcut('{shortcut_path}')
        $s.TargetPath = '{chosen_exe}'
        $s.WorkingDirectory = '{game_dir}'
        $s.IconLocation = '{icon_path},0'
        $s.Description = "Play SteelTempSaint MasterSword"
        $s.Save()
        """

        res = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", ps_script],
                             capture_output=True, text=True, timeout=10)
        return res.returncode == 0 and os.path.exists(shortcut_path)
    except Exception as e:
        print(f"Error creating shortcut: {e}")
        return False
