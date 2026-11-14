#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Load the packaged audio library and verify nonzero rendered PCM, without hardware."""
import argparse
import ctypes as c
import json
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library',type=Path,required=True)
    args=parser.parse_args()
    audio=c.CDLL(str(args.library.resolve()))
    def bind(name,result,*params):
        function=getattr(audio,name); function.restype=result; function.argtypes=list(params); return function
    get_proc=bind('alcGetProcAddress',c.c_void_p,c.c_void_p,c.c_char_p)
    loopback=get_proc(None,b'alcLoopbackOpenDeviceSOFT')
    render=get_proc(None,b'alcRenderSamplesSOFT')
    if not loopback or not render:
        raise SystemExit('Missing OpenAL Soft loopback extension')
    open_loopback=c.CFUNCTYPE(c.c_void_p,c.c_char_p)(loopback)
    render_samples=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_int)(render)
    create=bind('alcCreateContext',c.c_void_p,c.c_void_p,c.POINTER(c.c_int))
    current=bind('alcMakeContextCurrent',c.c_ubyte,c.c_void_p)
    destroy=bind('alcDestroyContext',None,c.c_void_p)
    close=bind('alcCloseDevice',c.c_ubyte,c.c_void_p)
    generate_buffers=bind('alGenBuffers',None,c.c_int,c.POINTER(c.c_uint))
    fill=bind('alBufferData',None,c.c_uint,c.c_int,c.c_void_p,c.c_int,c.c_int)
    generate_sources=bind('alGenSources',None,c.c_int,c.POINTER(c.c_uint))
    attach=bind('alSourcei',None,c.c_uint,c.c_int,c.c_int)
    play=bind('alSourcePlay',None,c.c_uint)
    error=bind('alGetError',c.c_int)
    device=open_loopback(None)
    if not device: raise SystemExit('Cannot open audio loopback device')
    context=None
    try:
        attributes=(c.c_int*7)(0x1990,0x1500,0x1991,0x1402,0x1007,22050,0)
        context=create(device,attributes)
        if not context or not current(context): raise SystemExit('Cannot create audio context')
        buffer,source=c.c_uint(),c.c_uint()
        generate_buffers(1,c.byref(buffer)); generate_sources(1,c.byref(source))
        samples=(c.c_short*11025)(*(int(12000*math.sin(2*math.pi*440*i/22050)) for i in range(11025)))
        fill(buffer,0x1101,samples,c.sizeof(samples),22050)
        attach(source,0x1009,buffer.value); play(source)
        rendered=(c.c_short*4096)()
        render_samples(device,rendered,4096)
        peak=max(abs(v) for v in rendered)
        rms=math.sqrt(sum(v*v for v in rendered)/len(rendered))
        if error()!=0 or peak<1000 or rms<500:
            raise SystemExit(f'Audio runtime produced invalid PCM: peak={peak}, rms={rms}')
        report=dict(samples=len(rendered),rate=22050,peak=peak,rms=round(rms,2),hardware=False)
        (ROOT/'build/audio-runtime.json').write_text(json.dumps(report)+'\n')
        print(f'PASS: packaged audio runtime rendered an actual tone to PCM: {report}')
        bind('alDeleteSources',None,c.c_int,c.POINTER(c.c_uint))(1,c.byref(source))
        bind('alDeleteBuffers',None,c.c_int,c.POINTER(c.c_uint))(1,c.byref(buffer))
    finally:
        current(None)
        if context: destroy(context)
        close(device)

if __name__=='__main__': main()
