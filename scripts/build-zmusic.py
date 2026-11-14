#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the pinned upstream music dependency, entirely under ignored build/."""
import argparse
from pathlib import Path
import subprocess

REVISION = "89f3d65734470fb7ec0c1e69f73a0cfcc88ed557"  # ZMusic 1.1.14
ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    source = ROOT / "build/deps/ZMusic"
    if not (source / ".git").exists():
        source.mkdir(parents=True, exist_ok=True)
        run("git", "init", source)
        run("git", "-C", source, "remote", "add", "origin", "https://github.com/ZDoom/ZMusic.git")
        run("git", "-C", source, "fetch", "--depth=1", "origin", REVISION)
        run("git", "-C", source, "checkout", "--detach", REVISION)
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != REVISION or subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True):
        raise SystemExit("ZMusic checkout differs from the pinned clean revision; inspect build/deps/ZMusic")
    run("cmake", "-S", source, "-B", ROOT / "build/deps/zmusic-build",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        f"-DCMAKE_INSTALL_PREFIX={ROOT / 'build/deps/zmusic'}", "-DCMAKE_INSTALL_LIBDIR=lib")
    run("cmake", "--build", ROOT / "build/deps/zmusic-build", "--config", "Release", "--parallel", args.jobs)
    run("cmake", "--install", ROOT / "build/deps/zmusic-build", "--config", "Release")


if __name__ == "__main__":
    main()
