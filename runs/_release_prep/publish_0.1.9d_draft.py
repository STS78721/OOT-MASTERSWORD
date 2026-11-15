"""0.1.9d: attach the staged files to the existing DRAFT release (made in the browser), check them, then publish it as Latest.
Uses the GitHub sign-in git keeps on this PC, as stage_and_upload_0.1.9c.py did. Nothing else on GitHub is changed."""
import json, sys, urllib.error, urllib.request
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import importlib.util
spec = importlib.util.spec_from_file_location("st", str(Path(__file__).parent / "stage_and_upload_0.1.9d.py"))
st = importlib.util.module_from_spec(spec); spec.loader.exec_module(st)
REPO, TAG = st.REPO, st.TAG
files = [st.DIST / f"SteelTemple-MasterSword-{TAG}-Portable-StandAlone.zip", st.DIST / f"SteelTemple-MasterSword-{TAG}-Update.zip",
         st.DIST / "MasterSwordLauncher.exe"]
token = st.git_token()
rels = st.api(token, "GET", f"https://api.github.com/repos/{REPO}/releases?per_page=20")
drafts = [r for r in rels if r.get("draft") and r.get("tag_name") == TAG]
if len(drafts) != 1:
    raise SystemExit(f"expected one draft for {TAG}, found {len(drafts)}")
rel = drafts[0]
print("draft:", rel["name"], rel["id"])
upload_url = rel["upload_url"].split("{")[0]
have = {a["name"]: a for a in rel.get("assets", [])}
for f in files:
    if f.name in have and have[f.name]["size"] == f.stat().st_size:
        print("already attached:", f.name); continue
    st.upload_asset(token, upload_url, f, "application/zip" if f.suffix == ".zip" else "application/octet-stream")
rel = st.api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/{rel['id']}")
got = {a["name"]: a["size"] for a in rel.get("assets", [])}
for f in files:
    if got.get(f.name) != f.stat().st_size:
        raise SystemExit(f"asset check failed for {f.name}: {got.get(f.name)} vs {f.stat().st_size}")
print("all three assets attached with the right sizes")
if "--publish" in sys.argv:
    st.api(token, "PATCH", f"https://api.github.com/repos/{REPO}/releases/{rel['id']}",
           data=json.dumps({"draft": False, "make_latest": "true", "tag_name": TAG}).encode(), content_type="application/json")
    latest = st.api(token, "GET", f"https://api.github.com/repos/{REPO}/releases/latest")
    print("published; latest release is now:", latest["tag_name"], latest["html_url"])
