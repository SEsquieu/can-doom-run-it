#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Observe native events; request a complete snapshot on connection or gaps."""
import argparse
import json
import socket
import time
from control_protocol import ControlState, MAX_DATAGRAM

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--count', type=int, default=0, help='Exit after N events; 0 listens until Ctrl-C')
    parser.add_argument('--timeout', type=float, default=None)
    parser.add_argument('--all', action='store_true', help='Also display definitions and state snapshots')
    args = parser.parse_args()
    if args.count < 0 or (args.timeout is not None and args.timeout <= 0):
        parser.error('count must be nonnegative and timeout must be positive')
    tracker = ControlState()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1', 19793))
        receiver.settimeout(args.timeout)
        received, last_request = 0, 0.0
        try:
            while not args.count or received < args.count:
                if not tracker.synchronized and time.monotonic() - last_request > 0.2:
                    receiver.sendto(b'DIO/2 SNAPSHOT', ('127.0.0.1', 19794))
                    last_request = time.monotonic()
                data, _ = receiver.recvfrom(MAX_DATAGRAM + 1)
                if len(data) > MAX_DATAGRAM:
                    raise SystemExit('Oversized control datagram')
                event = json.loads(data)
                if event.get('schema') == 2:
                    tracker.accept(event)
                elif event.get('schema') != 1:
                    raise SystemExit('Unsupported control schema')
                if args.all or event.get('kind', 'event') == 'event':
                    print(json.dumps(event, separators=(',', ':')), flush=True)
                    received += 1
        except socket.timeout as exc:
            raise SystemExit('Timed out waiting for native control events') from exc
        except KeyboardInterrupt:
            pass

if __name__ == '__main__':
    main()
