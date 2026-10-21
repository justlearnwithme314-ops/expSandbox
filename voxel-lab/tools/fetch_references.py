"""Restore the exact dependency checkouts listed in references/manifest.json."""
import json
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
for entry in json.loads((root/"references/manifest.json").read_text(encoding="utf-8-sig")):
    path=root/"references"/entry["name"]
    def git(*args):
        return subprocess.run(["git","-c",f"safe.directory={path.as_posix()}","-C",str(path),*args],check=True,text=True,capture_output=True).stdout.strip()
    if not (path/".git").exists():
        if path.exists() and any(path.iterdir()):
            raise RuntimeError(f"Refusing to overwrite non-Git directory: {path}")
        path.mkdir(parents=True,exist_ok=True)
        git("init")
        git("remote","add","origin",entry["url"])
    if git("status","--porcelain"):
        raise RuntimeError(f"Reference contains local changes: {path}")
    try:
        actual=git("rev-parse","HEAD")
    except subprocess.CalledProcessError:
        actual=""
    if actual != entry["revision"]:
        git("fetch","--depth","1","origin",entry["revision"])
        git("checkout","--detach",entry["revision"])
    assert git("rev-parse","HEAD")==entry["revision"]
    print(entry["name"],entry["revision"])
