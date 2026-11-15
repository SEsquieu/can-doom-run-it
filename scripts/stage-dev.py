#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a first-run development bundle with verified Freedoom and all notices."""
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
preset = sys.argv[1]
if preset not in ("linux", "windows"):
    raise SystemExit("Expected linux or windows")
build = root / "build" / preset
binary_dir = build / "Release" if preset == "windows" else build
stage = root / "build/stage" / preset
if stage.exists():
    shutil.rmtree(stage)
stage.mkdir(parents=True, exist_ok=True)
name = "can-doom-run-it.exe" if preset == "windows" else "can-doom-run-it"
shutil.copy2(binary_dir / name, stage / name)
lab = "doom-io-input-lab.exe" if preset == "windows" else "doom-io-input-lab"
shutil.copy2(binary_dir / lab, stage / lab)
if not (binary_dir / "doom-io.pk3").is_file() or not (binary_dir / "freedoom2.wad").is_file():
    raise SystemExit("Missing first-run content: fetch free assets and rebuild before staging")
shutil.copy2(binary_dir / "freedoom2.wad", stage / "freedoom2.wad")
shutil.copytree(binary_dir / "licenses/Freedoom", stage / "licenses/Freedoom", dirs_exist_ok=True)
pk3s = list(binary_dir.glob("*.pk3"))
if not pk3s:
    raise SystemExit("Missing engine PK3s")
for path in pk3s:
    shutil.copy2(path, stage / path.name)
for directory in ("soundfonts", "fm_banks"):
    source = binary_dir / directory
    shutil.copytree(source, stage / directory, dirs_exist_ok=True)
for notice in (root / "fm_banks").glob("*.txt"):
    shutil.copy2(notice, stage / "fm_banks" / notice.name)
if preset == "windows":
    audio = root / "build/deps/openal/bin/OpenAL32.dll"
    if not audio.is_file():
        raise SystemExit("Missing OpenAL runtime: run python scripts/build-openal.py")
    shutil.copy2(audio, stage / audio.name)
    openal_source = root / "build/deps/OpenAL"
    audio_notices = stage / "licenses/OpenAL"
    audio_notices.mkdir(parents=True, exist_ok=True)
    for name in ("COPYING", "BSD-3Clause", "LICENSE-pffft", "README.md"):
        shutil.copy2(openal_source / name, audio_notices / name)
    shutil.copy2(openal_source / "fmt-11.1.1/LICENSE", audio_notices / "fmt-LICENSE")
    dlls = list((root / "build/deps/zmusic").rglob("*.dll"))
    if not dlls:
        raise SystemExit("Missing ZMusic runtime DLL")
    for path in dlls:
        shutil.copy2(path, stage / path.name)
else:
    for path in (root / "build/deps/zmusic/lib").glob("libzmusic.so*"):
        shutil.copy2(path, stage / path.name)
shutil.copytree(root / "docs", stage / "docs", dirs_exist_ok=True)
shutil.copy2(root / "LICENSE", stage / "LICENSE")
shutil.copy2(root / "README.md", stage / "README.md")
shutil.copytree(root / "build/deps/ZMusic/licenses", stage / "licenses/ZMusic", dirs_exist_ok=True)
revision = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
(stage / "SOURCE.txt").write_text(
    f"Engine source: https://github.com/SEsquieu/can-doom-run-it/tree/{revision}\n"
    "ZMusic source: https://github.com/ZDoom/ZMusic/tree/89f3d65734470fb7ec0c1e69f73a0cfcc88ed557\n"
    "Windows OpenAL Soft source: https://github.com/kcat/openal-soft/tree/dc7d7054a5b4f3bec1dc23a42fd616a0847af948\n"
    "Development artifact; includes Freedoom 0.13.0 and original HELIX content.\n"
    "Extract the entire archive, then launch can-doom-run-it. See docs/PLAYING.md.\n",
    encoding="utf-8",
)
if preset == "windows":
    update = root / "build/stage/sketchpad-windows"
    if update.exists():
        shutil.rmtree(update)
    update.mkdir(parents=True)
    for file in (lab, "LICENSE", "SOURCE.txt"):
        shutil.copy2(stage / file, update / file)
    (update / "UPDATE.txt").write_text(
        "Close the Input Lab / Sketchpad window.\n"
        "Replace doom-io-input-lab.exe in your existing DOOM I/O folder with this version.\n"
        "Return to DOOM: F6 reopens it, F7 rearms output, then enable LATCH to draw.\n"
        "The XY pad is a velocity joystick: center or leave it to stop.\n",
        encoding="utf-8",
    )
