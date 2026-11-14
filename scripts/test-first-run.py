#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux graphical check: zero arguments, fresh state, actual F6/F7 and Use keys."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import time
from control_protocol import ControlState,MAX_DATAGRAM

ROOT=Path(__file__).resolve().parents[1]

class Keyboard:
    def __init__(self):
        self.x11=c.CDLL('libX11.so.6'); self.xtest=c.CDLL('libXtst.so.6')
        self.x11.XOpenDisplay.argtypes=[c.c_char_p]; self.x11.XOpenDisplay.restype=c.c_void_p
        self.x11.XKeysymToKeycode.argtypes=[c.c_void_p,c.c_ulong]; self.x11.XKeysymToKeycode.restype=c.c_ubyte
        self.x11.XSync.argtypes=[c.c_void_p,c.c_int]; self.x11.XCloseDisplay.argtypes=[c.c_void_p]
        self.xtest.XTestFakeKeyEvent.argtypes=[c.c_void_p,c.c_uint,c.c_int,c.c_ulong]
        self.display=self.x11.XOpenDisplay(None)
        if not self.display: raise RuntimeError('Cannot open X11 display')
    def key(self,symbol):
        code=self.x11.XKeysymToKeycode(self.display,symbol)
        if not code: raise RuntimeError(f'Cannot map key {symbol}')
        self.xtest.XTestFakeKeyEvent(self.display,code,1,0)
        self.x11.XSync(self.display,0); time.sleep(0.1)
        self.xtest.XTestFakeKeyEvent(self.display,code,0,0)
        self.x11.XSync(self.display,0); time.sleep(0.05)
    def close(self): self.x11.XCloseDisplay(self.display)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine',type=Path,default=ROOT/'build/linux/can-doom-run-it')
    args=parser.parse_args()
    work=ROOT/'build/smoke/default-first-run'; work.mkdir(parents=True,exist_ok=True)
    for name in ('config','cache'):
        shutil.rmtree(work/name,ignore_errors=True)
    report=work/'input-lab.json'; report.unlink(missing_ok=True)
    environment=os.environ.copy()
    environment.update(XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'),
                       CDR_LAB_REPORT=str(report),CDR_LAB_EXPORT=str(work/'sketch.svg'))
    tracker=ControlState(); messages=[]
    process=None; keyboard=None
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver, (work/'engine.log').open('w') as log:
        receiver.bind(('127.0.0.1',19793)); receiver.settimeout(0.1)
        def until(predicate,description,seconds=10):
            deadline=time.monotonic()+seconds
            while time.monotonic()<deadline:
                if process.poll() is not None: raise RuntimeError(f'Engine exited before {description}: {process.returncode}')
                try:
                    data,_=receiver.recvfrom(MAX_DATAGRAM+1)
                    if len(data)>MAX_DATAGRAM: raise RuntimeError('Oversized datagram')
                    message=json.loads(data); tracker.accept(message); messages.append(message)
                except socket.timeout: pass
                if predicate(): return
            raise RuntimeError(f'Timed out waiting for {description}; inspect {work}')
        try:
            # No renderer, config, sound, IWAD, map, exec or other arguments.
            process=subprocess.Popen([str(args.engine.resolve())],cwd=work,env=environment,
                                     stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            until(lambda:tracker.synchronized and len(tracker.definitions)==8 and any(m['tick']>=35 for m in messages),'fresh default room with advancing simulation',60)
            keyboard=Keyboard(); keyboard.key(0xffc3)  # F6, native launch path.
            until(lambda:report.exists(),'native endpoint launch')
            time.sleep(0.5)  # Local discovery polls at 250 ms; no script config.
            keyboard.key(0xffc4)  # F7, explicit native user gesture.
            until(lambda:any(m.get('reason')=='rearm' for m in messages),'native authorization')
            keyboard.key(ord('e'))
            until(lambda:any(m.get('kind')=='event' and m.get('control')=='player.use' for m in messages),'actual Use binding')
            keyboard.key(0xffc4)  # Disarm and inspect independent held-state cleanup.
            time.sleep(0.2)
            observed=json.loads(report.read_text())
            if observed['held']!=0 or observed['mode']!=0: raise RuntimeError(f'Native disarm left state held: {observed}')
            os.kill(observed['pid'],signal.SIGTERM)
            time.sleep(0.1)  # Let native worker reap its child while the engine lives.
            keyboard.key(ord('`'))
            for char in 'quit': keyboard.key(ord(char))
            keyboard.key(0xff0d)
            process.wait(timeout=10)
            if process.returncode!=0: raise RuntimeError(f'Engine exit failed: {process.returncode}')
            log.flush()
            startup_log=(work/'engine.log').read_text(errors='replace')
            if 'Could not find patch set can-doom-run-it' in startup_log or 'Unable to start D_runnin' in startup_log:
                raise RuntimeError('First-run bundled music resources failed to load')
            print('PASS: zero-argument fresh launch, native F6/F7, actual Use key, disarm and normal quit')
        finally:
            (work/'messages.jsonl').write_text(''.join(json.dumps(m)+'\n' for m in messages))
            if keyboard: keyboard.close()
            if process is not None:
                try: os.killpg(process.pid,signal.SIGTERM)
                except ProcessLookupError: pass
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired: os.killpg(process.pid,signal.SIGKILL); process.wait()

if __name__=='__main__': main()
