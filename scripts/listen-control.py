#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Observe native control events. Start this, launch the engine, and press Use."""
import argparse
import json
import socket


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=0, help="Exit after N events; 0 listens until Ctrl-C")
    parser.add_argument("--timeout", type=float, default=None, help="Seconds without an event before exiting")
    args = parser.parse_args()
    if args.count < 0 or (args.timeout is not None and args.timeout <= 0):
        parser.error("count must be nonnegative and timeout must be positive")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(("127.0.0.1", 19793))
        receiver.settimeout(args.timeout)
        received = 0
        try:
            while not args.count or received < args.count:
                data, _ = receiver.recvfrom(257)
                if len(data) > 256:
                    raise SystemExit("Unexpected oversized control datagram")
                event = json.loads(data)
                if event.get("schema") != 1 or event.get("type") != "trigger":
                    raise SystemExit("Unknown control schema or primitive")
                print(json.dumps(event, separators=(",", ":")), flush=True)
                received += 1
        except socket.timeout as exc:
            raise SystemExit("Timed out waiting for native control events") from exc
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
