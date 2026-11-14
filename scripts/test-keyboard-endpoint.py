#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove OS-routed key delivery in a separate desktop process, not API success."""
import argparse
import json
from pathlib import Path
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset',choices=['linux','windows'],required=True)
    args=parser.parse_args()
    build=ROOT/'build'/args.preset
    suffix='.exe' if args.preset=='windows' else ''
    lab=(build/'Release' if args.preset=='windows' else build)/('doom-io-input-lab'+suffix)
    probe=build/'src/cdr'/('Release' if args.preset=='windows' else '')/('cdr_keyboard_probe'+suffix)
    report=ROOT/'build/keyboard-endpoint.json'
    report.unlink(missing_ok=True)
    process=subprocess.Popen([str(lab),'--report',str(report)])
    try:
        subprocess.run([str(probe)],check=True,timeout=12)
        deadline=time.monotonic()+3
        observed=None
        while time.monotonic()<deadline:
            try:
                observed=json.loads(report.read_text())
                if observed['held']==0 and observed['fires']==1 and observed['pulses']==1 and observed['x']>0 and observed['y']>0 and observed['mode']==0:
                    break
            except (FileNotFoundError,json.JSONDecodeError):
                pass
            time.sleep(0.02)
        else:
            raise SystemExit(f'OS endpoint did not receive the expected actions and cleanup: {observed}')
        print('PASS: independent native window received keyboard actions, moved its rover, and returned to neutral')
        print(json.dumps(observed))
    finally:
        process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait()

if __name__=='__main__': main()
