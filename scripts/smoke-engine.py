#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Launch the real engine, enter a free game map, and exit on simulation ticks.

Linux CI runs this under xvfb-run with LIBGL_ALWAYS_SOFTWARE=1.
"""
import argparse
import json
import os
import socket
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "build/linux/can-doom-run-it")
    parser.add_argument("--iwad", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--controls", action="store_true", help="Drive two Use presses and check native events")
    parser.add_argument("--disable-output", action="store_true", help="Expect no events with output disabled")
    parser.add_argument("--no-listener", action="store_true", help="Verify gameplay still exits with no receiver")
    demos = parser.add_mutually_exclusive_group()
    demos.add_argument("--record-demo", type=Path, help="Record this live test as a demo")
    demos.add_argument("--play-demo", type=Path, help="Replay a recorded test; expect no external events")
    args = parser.parse_args()
    if (args.disable_output or args.no_listener or args.record_demo or args.play_demo) and not args.controls:
        parser.error("Output and demo options require --controls")
    engine, iwad = args.engine.resolve(), args.iwad.resolve()
    if not engine.is_file() or not iwad.is_file():
        raise SystemExit("Both the built engine and a free IWAD are required")
    mode = "startup"
    if args.controls:
        mode = "replay" if args.play_demo else "disabled" if args.disable_output else "unobserved" if args.no_listener else "live"
    work = ROOT / "build/smoke" / mode
    work.mkdir(parents=True, exist_ok=True)
    config = work / "startup.cfg"
    # Startup defers console commands individually. Restore the alias from INI
    # before exec parsing, then invoke it as one deferred command. Defining and
    # invoking a new alias in the same startup cfg would report "unknown command".
    chain = "wait 105; quit"
    if args.controls:
        chain = ("wait 70; +use; wait 12; -use; wait 10; +use; wait 4; -use; "
                 "wait 10; cdr_io_status; quit")
    if args.play_demo:
        chain = "wait 100; cdr_io_status; wait 10; quit"
    (work / "engine.ini").write_text(
        "[LastRun]\nVersion=225\n[GlobalSettings]\nvid_preferbackend=2\n"
        "vid_rendermode=0\nvid_activeinbackground=true\ni_pauseinbackground=false\n"
        "vid_lowerinbackground=false\nr_multithreaded=0\ncl_capfps=true\n"
        f"[Doom.ConsoleAliases]\nName=cdr_smoke\nCommand={chain}\n",
        encoding="utf-8",
    )
    config.write_text("cdr_smoke\n", encoding="utf-8")
    log = work / "engine.log"
    command = [str(engine), "-iwad", str(iwad), "-config", str(work / "engine.ini"),
               "-noautoload", "-noautoexec", "-nosound", "-nojoy", "-warp", "1",
               "-nomonsters", "-window", "-width", "640",
               "-height", "480", "+vid_preferbackend", "2", "+vid_rendermode", "0",
               "+vid_activeinbackground", "1", "+i_pauseinbackground", "0",
               "+cl_capfps", "1", "+r_multithreaded", "0",
               "+logfile", str(work / "console.log"),
               "-exec", str(config)]
    if args.controls:
        command.extend(["+cdr_io_enabled", "0" if args.disable_output else "1"])
    if args.record_demo:
        command.extend(["-record", str(args.record_demo.resolve())])
    if args.play_demo:
        command.extend(["-playdemo", str(args.play_demo.resolve())])
    environment = os.environ.copy()
    if os.name == "posix":
        environment.update(XDG_CONFIG_HOME=str(work / "config"),
                           XDG_CACHE_HOME=str(work / "cache"))
    events = []
    listener = None
    try:
        if args.controls and not args.no_listener:
            listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            listener.bind(("127.0.0.1", 19793))
            listener.settimeout(0.2)
        with log.open("w", encoding="utf-8") as output:
            try:
                result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                        timeout=args.timeout, cwd=work, env=environment)
            except subprocess.TimeoutExpired as exc:
                raise SystemExit(f"Engine smoke test timed out; inspect {log} and {work / 'console.log'}") from exc
        if listener is not None:
            while True:
                try:
                    data, _ = listener.recvfrom(257)
                except socket.timeout:
                    break
                if len(data) > 256:
                    raise SystemExit("Oversized event received")
                events.append(json.loads(data))
    finally:
        if listener is not None:
            listener.close()
    text = log.read_text(encoding="utf-8", errors="replace")
    if result.returncode != 0 or "MAP01 - " not in text.upper():
        raise SystemExit(f"Engine failed to enter MAP01 and exit cleanly; inspect {log}\n{text[-4000:]}")
    if args.controls:
        expected = 0 if args.disable_output or args.play_demo else 2
        if f"emitted={expected} " not in text:
            raise SystemExit(f"Unexpected native I/O counters; inspect {log}")
        if not args.no_listener:
            (work / "events.jsonl").write_text("".join(json.dumps(event) + "\n" for event in events), encoding="utf-8")
            if len(events) != expected:
                raise SystemExit(f"Expected {expected} events, got {len(events)}: {events}")
            for index, event in enumerate(events, start=1):
                if (event.get("schema") != 1 or event.get("type") != "trigger" or
                    event.get("control") != "player.use" or event.get("player") != 0 or
                    event.get("sequence") != index or type(event.get("tick")) is not int):
                    raise SystemExit(f"Unexpected control event: {event}")
            if events and not (events[0]["tick"] >= 0 and events[1]["tick"] > events[0]["tick"] + 10):
                raise SystemExit(f"Use hold/release timing did not match: {events}")
        print(f"PASS: {mode}, native emitted={expected}, observed={len(events)}")
    print(f"PASS: real engine entered MAP01 and exited cleanly. Log: {log}")


if __name__ == "__main__":
    main()
