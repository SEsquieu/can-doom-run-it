#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Launch the real engine, enter a free game map, and exit on simulation ticks.

Linux CI runs this under xvfb-run with LIBGL_ALWAYS_SOFTWARE=1.
"""
import argparse
import json
import os
import socket
import signal
import time
import xml.etree.ElementTree as ET
from control_protocol import ControlState, MAX_DATAGRAM
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "build/linux/can-doom-run-it")
    parser.add_argument("--iwad", type=Path)
    parser.add_argument("--room", action="store_true", help="Test bundled first-run content without IWAD/map arguments")
    parser.add_argument("--external", action="store_true", help="With --room --controls, verify actions in the independent native input lab")
    parser.add_argument("--screenshot", type=Path)
    parser.add_argument("--view", choices=["entry","monitor","buttons"], default="entry")
    parser.add_argument("--multiplayer", action="store_true", help="Check authority suppression in a one-player multiplayer host")
    parser.add_argument("--renderer", choices=[0,4], type=int, default=0)
    parser.add_argument("--backend", choices=[0,2], type=int, default=2)
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--controls", action="store_true", help="Exercise native controls through actual gameplay")
    parser.add_argument("--disable-output", action="store_true", help="Expect no events with output disabled")
    parser.add_argument("--no-listener", action="store_true", help="Verify gameplay still exits with no receiver")
    demos = parser.add_mutually_exclusive_group()
    demos.add_argument("--record-demo", type=Path, help="Record this live test as a demo")
    demos.add_argument("--play-demo", type=Path, help="Replay a recorded test; expect no external events")
    args = parser.parse_args()
    if (args.disable_output or args.no_listener or args.record_demo or args.play_demo) and not args.controls:
        parser.error("Output and demo options require --controls")
    if not args.room and args.iwad is None:
        parser.error("--iwad is required outside the bundled room")
    if args.external and (not args.room or not args.controls or args.disable_output or args.no_listener or args.play_demo):
        parser.error("--external requires live --room --controls")
    if args.view!="entry" and (not args.room or args.controls):
        parser.error("--view is for room startup screenshots")
    engine = args.engine.resolve()
    iwad = args.iwad.resolve() if args.iwad else None
    if not engine.is_file() or (iwad is not None and not iwad.is_file()):
        raise SystemExit("Both the built engine and a free IWAD are required")
    mode = "startup"
    if args.controls:
        mode = "replay" if args.play_demo else "disabled" if args.disable_output else "unobserved" if args.no_listener else "live"
    if args.room:
        mode = "room-" + mode
        if args.external:
            mode += "-external"
        if args.view!="entry":
            mode += "-" + args.view
    if args.multiplayer:
        mode += "-multiplayer"
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
    if args.room and args.controls:
        # Position with the engine's existing cheat command, then operate real
        # actors through hitscan, Use and movement. Never call Publish in a test.
        chain = ("wait 70; " + ("cdr_open_lab; wait 35; cdr_arm; wait 7; " if args.external else "") +
                 "warp -512 348 0; wait 3; +attack; wait 6; -attack; wait 24; "
                 "warp -384 348 0; wait 3; +attack; wait 6; -attack; wait 24; "
                 "warp -256 348 0; wait 3; +attack; wait 6; -attack; wait 24; "
                 "warp 256 348 0; wait 3; +use; wait 2; -use; wait 6; "
                 "warp 512 348 0; wait 3; +use; wait 2; -use; wait 6; "
                 "warp -628 64 0; +left; wait 28; -left; wait 3; "
                 "+use; +lookup; wait 16; -lookup; wait 3; -use; wait 5; centerview; "
                 "warp 80 144 -8; wait 16; warp 0 -352 0; wait 7; " +
                 ("warp -512 348 0; +right; wait 28; -right; wait 3; +attack; wait 6; -attack; wait 24; " if args.external else "") +
                 "cdr_io_status; quit")
    if args.play_demo:
        chain = "wait 100; cdr_io_status; quit"
    if args.room and not args.controls:
        chain = chain.replace("quit", "cdr_io_status; quit")
        if args.view=="monitor":
            chain=chain.replace("cdr_io_status; quit","warp 620 64 0; +right; wait 28; -right; wait 5; cdr_io_status; quit")
        elif args.view=="buttons":
            chain=chain.replace("cdr_io_status; quit","warp -384 292 0; wait 5; cdr_io_status; quit")
    if args.screenshot:
        args.screenshot.resolve().parent.mkdir(parents=True, exist_ok=True)
        chain = chain.replace("cdr_io_status; ", "")
        chain = chain.replace("quit", f'screenshot "{args.screenshot.resolve().as_posix()}"; wait 2; cdr_io_status; quit')
    if args.external:
        blur="echo CDR_BLUR_REQUEST; wait 21; " if os.name=="posix" else ""
        chain=chain.replace("quit", blur+"menu_main; wait 10; echo CDR_MENU_CLEANUP; cdr_io_status; quit")
    chain = "echo CDR_SMOKE_BEGIN; " + chain.replace("quit", "echo CDR_SMOKE_DONE; quit")
    (work / "engine.ini").write_text(
        f"[LastRun]\nVersion=225\n[GlobalSettings]\nvid_preferbackend={args.backend}\n"
        f"vid_rendermode={args.renderer}\nvid_fullscreen=false\nwin_w={1280 if args.room else 640}\nwin_h={720 if args.room else 480}\n"
        "vid_activeinbackground=true\ni_pauseinbackground=false\n"
        "vid_lowerinbackground=false\nr_multithreaded=0\ncl_capfps=true\n"
        f"[Doom.ConsoleAliases]\nName=cdr_smoke\nCommand={chain}\n",
        encoding="utf-8",
    )
    config.write_text("cdr_smoke\n", encoding="utf-8")
    log = work / "engine.log"
    (work / "console.log").write_text("", encoding="utf-8")
    command = [str(engine), "-config", str(work / "engine.ini"),
               "-noautoload", "-noautoexec", "-nosound", "-nojoy",
               "-nomonsters", "-window", "-width", str(1280 if args.room else 640),
               "-height", str(720 if args.room else 480), "+vid_preferbackend", str(args.backend), "+vid_rendermode", str(args.renderer),
               "+vid_activeinbackground", "1", "+i_pauseinbackground", "0",
               "+cl_capfps", "1", "+r_multithreaded", "0",
               "+logfile", str(work / "console.log"),
               "-exec", str(config)]
    if args.room and args.play_demo:
        command.extend(["-iwad", str(engine.parent / "freedoom2.wad"), "-file", str(engine.parent / "doom-io.pk3")])
    elif not args.room:
        command.extend(["-iwad", str(iwad), "-warp", "1"])
    else:
        command.extend(["+screenblocks", "12", "+crosshair", "1"])
    if args.controls:
        command.extend(["+cdr_io_enabled", "0" if args.disable_output else "1"])
    if args.multiplayer:
        command.extend(["-host","1"])
    if args.record_demo:
        command.extend(["-record", str(args.record_demo.resolve())])
    if args.play_demo:
        command.extend(["-playdemo", str(args.play_demo.resolve())])
    environment = os.environ.copy()
    if os.name == "posix":
        environment.update(XDG_CONFIG_HOME=str(work / "config"),
                           XDG_CACHE_HOME=str(work / "cache"))
    events, messages = [], []
    def drain():
        if listener is None:
            return
        for _ in range(256):
            try:
                data, _ = listener.recvfrom(MAX_DATAGRAM + 1)
            except BlockingIOError:
                break
            if len(data) > MAX_DATAGRAM:
                raise SystemExit("Oversized event received")
            record = json.loads(data)
            messages.append(record)
            if record.get("kind", "event") == "event" and record.get("control") == "player.use":
                events.append(record)
    listener = None
    process = None
    try:
        if args.external:
            report = work / "input-lab.json"
            report.unlink(missing_ok=True)
            export = work / "sketch.svg"
            export.unlink(missing_ok=True)
            # Exercise the application's native launch path rather than starting
            # its endpoint for it. Only the trusted environment selects test files.
            environment.update(CDR_LAB_REPORT=str(report),CDR_LAB_EXPORT=str(export))
        if args.controls and not args.no_listener:
            listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            listener.bind(("127.0.0.1", 19793))
            listener.setblocking(False)
            listener.sendto(b"DIO/2 SNAPSHOT", ("127.0.0.1", 19794))
        with log.open("w", encoding="utf-8") as output:
            try:
                process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                           cwd=work, env=environment, start_new_session=os.name == "posix")
                deadline = time.monotonic() + args.timeout
                blurred=False
                while process.poll() is None:
                    console = work / "console.log"
                    diagnostic = console.read_text(errors="replace") if console.exists() else ""
                    if 'Unknown command "cdr_smoke"' in diagnostic:
                        raise RuntimeError("Startup alias was not registered before exec; this is a fixture failure")
                    if "Script error," in diagnostic or "Execution could not continue" in diagnostic:
                        raise RuntimeError("Engine script initialization failed")
                    if args.external and os.name=="posix" and not blurred and "CDR_BLUR_REQUEST" in diagnostic:
                        blur_x11_window()
                        blurred=True
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Engine smoke test timed out")
                    drain()
                    time.sleep(0.05)
                result = process
            except (RuntimeError, KeyboardInterrupt) as exc:
                if process.poll() is None:
                    if os.name == "posix":
                        os.killpg(process.pid, signal.SIGKILL)
                    else:
                        process.kill()
                    process.wait()
                raise SystemExit(f"{exc}; inspect {log} and {work / 'console.log'}") from exc
        drain()
        if args.external:
            deadline=time.monotonic()+3
            observed=None
            while time.monotonic()<deadline:
                try:
                    observed=json.loads(report.read_text())
                    if observed["held"]==0 and observed["fires"]>=1 and observed["pulses"]>=2 and observed["brakes"]>=1 and observed["x"]>0 and observed["y"]>0 and observed["mode"]==0:
                        break
                except (FileNotFoundError,json.JSONDecodeError):
                    pass
                time.sleep(0.02)
            else:
                raise SystemExit(f"Gameplay did not drive and neutralize the independent OS endpoint: {observed}")
            print(f"PASS: gameplay drove external OS window: {observed}")
            drawing=ET.parse(export).getroot()
            if not drawing.findall('{http://www.w3.org/2000/svg}path') or not drawing.findall('{http://www.w3.org/2000/svg}circle'):
                raise SystemExit("Gameplay did not produce strokes and markers in the external application's SVG export")
            print(f"PASS: gameplay produced a usable external SVG drawing: {export}")
    finally:
        if listener is not None:
            listener.close()
        if args.external and process is not None:
            if os.name=="posix":
                # Native child shares the isolated engine process group.
                try: os.killpg(process.pid,signal.SIGTERM)
                except ProcessLookupError: pass
            else:
                try: os.kill(json.loads(report.read_text())["pid"],signal.SIGTERM)
                except (OSError,KeyError,ValueError): pass
    text = log.read_text(encoding="utf-8", errors="replace")
    if result.returncode != 0 or ("DIO01 - " if args.room else "MAP01 - ") not in text.upper():
        raise SystemExit(f"Engine failed to enter MAP01 and exit cleanly; inspect {log}\n{text[-4000:]}")
    if "CDR_SMOKE_BEGIN" not in text or "CDR_SMOKE_DONE" not in text:
        raise SystemExit(f"Timed gameplay commands did not complete; inspect {log}")
    if args.controls:
        (work / "messages.jsonl").write_text("".join(json.dumps(m) + "\n" for m in messages), encoding="utf-8")
        if args.room:
            validate_room(args, messages, text)
            print(f"PASS: {mode}, actual native instruments verified")
            return
        expected = 0 if args.disable_output or args.play_demo or args.multiplayer else 2
        if f"emitted={expected} " not in text:
            raise SystemExit(f"Unexpected native I/O counters; inspect {log}")
        if not args.no_listener:
            (work / "events.jsonl").write_text("".join(json.dumps(event) + "\n" for event in events), encoding="utf-8")
            if len(events) != expected:
                raise SystemExit(f"Expected {expected} events, got {len(events)}: {events}")
            for index, event in enumerate(events, start=1):
                if (event.get("schema") != 2 or not event.get("session") or event.get("type") != "trigger" or
                    event.get("control") != "player.use" or event.get("player") != 0 or
                    (index > 1 and event.get("sequence", 0) <= events[index-2]["sequence"]) or type(event.get("tick")) is not int):
                    raise SystemExit(f"Unexpected control event: {event}")
            tracker = ControlState()
            for record in messages:
                tracker.accept(record)
            if expected and not tracker.definitions:
                raise SystemExit("No initial state snapshot received")
            (work / "messages.jsonl").write_text("".join(json.dumps(m) + "\n" for m in messages), encoding="utf-8")
            if events and not (events[0]["tick"] >= 0 and events[1]["tick"] > events[0]["tick"] + 10):
                raise SystemExit(f"Use hold/release timing did not match: {events}")
        print(f"PASS: {mode}, native emitted={expected}, observed={len(events)}")
    print(f"PASS: real engine entered {'DIO01' if args.room else 'MAP01'} and exited cleanly. Log: {log}")


def validate_room(args, messages, text):
    if "controls=8" not in text:
        raise SystemExit("Room did not register its complete control set")
    if args.no_listener:
        return
    events=[m for m in messages if m.get("kind")=="event"]
    if args.disable_output or args.play_demo or args.multiplayer:
        if events:
            raise SystemExit(f"Unauthorized room events: {events}")
        return
    by_id={}
    for m in events:
        by_id.setdefault(m["control"],[]).append(m)
    required={"trigger.pulse","button.a","button.b","toggle.power","scalar.throttle","vector.pad","selector.mode"}
    missing=required-set(by_id)
    if missing:
        raise SystemExit(f"Gameplay did not reach instruments {missing}; observed {by_id}")
    sessions={m.get("session") for m in messages}
    if len(sessions)!=1 or not next(iter(sessions)):
        raise SystemExit("Invalid process session boundary")
    if any(m.get("schema")!=2 for m in messages) or any(a["sequence"]>=b["sequence"] for a,b in zip(messages,messages[1:])):
        raise SystemExit("Invalid message schema or sequence ordering")
    for identifier in ("button.a","button.b"):
        phases=[m["phase"] for m in by_id[identifier]]
        if phases!=["press","release"]:
            raise SystemExit(f"Invalid momentary button transition: {identifier}: {phases}")
    for identifier,neutral in (("scalar.throttle",0.5),("vector.pad",0)):
        values=[m["value"][0] for m in by_id[identifier]]
        if max(values)<=neutral+0.1 or values[-1]!=neutral:
            raise SystemExit(f"Continuous control failed range/neutral check: {identifier}: {values}")
    if not any(m["value"][1]>0.1 for m in by_id["vector.pad"]):
        raise SystemExit("XY surface did not publish its second axis")
    if by_id["toggle.power"][0]["value"]!=[1,0] or by_id["selector.mode"][0]["value"]!=[1,0]:
        raise SystemExit("Use instruments did not change their persistent state")
    if args.external:
        focus=[m for m in messages if m.get("kind")=="lifecycle" and m.get("reason")=="focus_lost"]
        shutdown=[m for m in messages if m.get("kind")=="lifecycle" and m.get("reason")=="shutdown"]
        if not focus or not shutdown or focus[-1]["sequence"]>=shutdown[0]["sequence"]:
            raise SystemExit("Engine menu/focus cleanup did not precede shutdown")
        if not any(m.get("reason")=="focus_lost" and m["value"]==[0,0] for m in by_id["toggle.power"]):
            raise SystemExit("Native menu/focus loss did not neutralize the held toggle")
    tracker=ControlState()
    for message in messages:
        tracker.accept(message)
    if len(tracker.definitions)!=8:
        raise SystemExit("Missing complete native state snapshot")


def blur_x11_window():
    """Change actual OS focus; background rendering flags must not confer authority."""
    import ctypes
    x11=ctypes.CDLL("libX11.so.6")
    x11.XOpenDisplay.argtypes=[ctypes.c_char_p]
    x11.XOpenDisplay.restype=ctypes.c_void_p
    x11.XDefaultRootWindow.argtypes=[ctypes.c_void_p]
    x11.XDefaultRootWindow.restype=ctypes.c_ulong
    x11.XSetInputFocus.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong]
    x11.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
    x11.XCloseDisplay.argtypes=[ctypes.c_void_p]
    display=x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("Cannot open display for actual focus-loss check")
    try:
        x11.XSetInputFocus(display,x11.XDefaultRootWindow(display),2,0)
        x11.XSync(display,0)
    finally:
        x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()
