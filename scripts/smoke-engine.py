#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Launch the real engine, enter a free game map, and exit on simulation ticks.

Linux CI runs this under xvfb-run with LIBGL_ALWAYS_SOFTWARE=1.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "build/linux/can-doom-run-it")
    parser.add_argument("--iwad", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    engine, iwad = args.engine.resolve(), args.iwad.resolve()
    if not engine.is_file() or not iwad.is_file():
        raise SystemExit("Both the built engine and a free IWAD are required")
    work = ROOT / "build/smoke"
    work.mkdir(parents=True, exist_ok=True)
    config = work / "startup.cfg"
    # Exec files split semicolons before execution. An alias preserves the wait
    # chain so quit runs in the game loop, rather than during startup parsing.
    config.write_text('alias cdr_smoke "wait 105; quit"\ncdr_smoke\n', encoding="utf-8")
    log = work / "engine.log"
    command = [str(engine), "-iwad", str(iwad), "-config", str(work / "engine.ini"),
               "-noautoload", "-noautoexec", "-nosound", "-nojoy", "-warp", "1",
               "-nomonsters", "-window", "-width", "640",
               "-height", "480", "+vid_preferbackend", "2", "+vid_rendermode", "0",
               "+vid_activeinbackground", "1", "+i_pauseinbackground", "0",
               "+cl_capfps", "1", "+r_multithreaded", "0",
               "+logfile", str(work / "console.log"),
               "-exec", str(config)]
    environment = os.environ.copy()
    if os.name == "posix":
        environment.update(XDG_CONFIG_HOME=str(work / "config"),
                           XDG_CACHE_HOME=str(work / "cache"))
    with log.open("w", encoding="utf-8") as output:
        try:
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                    timeout=args.timeout, cwd=work, env=environment)
        except subprocess.TimeoutExpired as exc:
            raise SystemExit(f"Engine smoke test timed out; inspect {log}") from exc
    text = log.read_text(encoding="utf-8", errors="replace")
    if result.returncode != 0 or "MAP01 - " not in text.upper():
        raise SystemExit(f"Engine failed to enter MAP01 and exit cleanly; inspect {log}\n{text[-4000:]}")
    print(f"PASS: real engine entered MAP01 and exited cleanly. Log: {log}")


if __name__ == "__main__":
    main()
