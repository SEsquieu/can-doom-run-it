#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove OS-routed key delivery in a separate desktop process, not API success."""
import argparse
import json
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET
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
    export=report.with_suffix('.svg')
    export.unlink(missing_ok=True)
    process=subprocess.Popen([str(lab),'--report',str(report),'--export',str(export)])
    driver=None
    try:
        driver=subprocess.Popen([str(probe)],stdin=subprocess.PIPE)
        deadline=time.monotonic()+8
        observed=None
        while time.monotonic()<deadline:
            try:
                observed=json.loads(report.read_text())
                if observed['fires']==1 and observed['pulses']==1 and observed['strokes']>0 and observed['x']>0 and observed['y']>0:
                    drawing=ET.parse(export).getroot()
                    if drawing.findall('{http://www.w3.org/2000/svg}path') and drawing.findall('{http://www.w3.org/2000/svg}circle'):
                        break
            except (FileNotFoundError,json.JSONDecodeError,ET.ParseError):
                pass
            if driver.poll() is not None:
                raise SystemExit(f'Native probe exited before the endpoint acknowledged output: {driver.returncode}')
            time.sleep(0.02)
        else:
            raise SystemExit(f'External SVG export was not observed before shutdown: {observed}')
        driver.communicate(input=b'\n',timeout=3)
        if driver.returncode!=0:
            raise SystemExit(f'Native probe failed: {driver.returncode}')
        deadline=time.monotonic()+3
        observed=None
        while time.monotonic()<deadline:
            try:
                observed=json.loads(report.read_text())
                if observed['held']==0 and observed['fires']==1 and observed['pulses']==1 and observed['x']>0 and observed['y']>0 and observed['mode']==0 and observed['strokes']>0:
                    break
            except (FileNotFoundError,json.JSONDecodeError):
                pass
            time.sleep(0.02)
        else:
            raise SystemExit(f'OS endpoint did not receive the expected actions and cleanup: {observed}')
        print('PASS: independent native window received keyboard actions, moved its rover, and returned to neutral')
        print(json.dumps(observed))
        drawing=ET.parse(export).getroot()
        if not drawing.findall('{http://www.w3.org/2000/svg}path') or not drawing.findall('{http://www.w3.org/2000/svg}circle'):
            raise SystemExit('External application did not produce a usable SVG drawing')
        print(f'PASS: independent application exported strokes and markers to {export}')
    finally:
        if driver is not None and driver.poll() is None:
            if driver.stdin and not driver.stdin.closed:
                driver.stdin.write(b'\n'); driver.stdin.close()
            try: driver.wait(timeout=3)
            except subprocess.TimeoutExpired: driver.kill(); driver.wait()
        process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait()

if __name__=='__main__': main()
