#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the pinned dynamic audio runtime for self-contained Windows bundles."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
REVISION='dc7d7054a5b4f3bec1dc23a42fd616a0847af948'  # OpenAL Soft 1.24.3
URL='https://github.com/kcat/openal-soft.git'

def run(*args): subprocess.run([str(a) for a in args],check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs',type=int,default=3)
    args=parser.parse_args()
    source=ROOT/'build/deps/OpenAL'
    if not (source/'.git').exists():
        source.mkdir(parents=True,exist_ok=True)
        run('git','init',source)
        run('git','-C',source,'remote','add','origin',URL)
        run('git','-C',source,'fetch','--depth=1','origin',REVISION)
        run('git','-C',source,'checkout','--detach',REVISION)
    actual=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    dirty=subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True)
    if actual!=REVISION or dirty:
        raise SystemExit('OpenAL checkout differs from its pinned clean revision')
    build=ROOT/'build/deps/openal-build'
    generator=['-G','Visual Studio 17 2022','-A','x64'] if os.name=='nt' else []
    run('cmake','-S',source,'-B',build,*generator,'-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_INSTALL_PREFIX={ROOT / "build/deps/openal"}','-DCMAKE_INSTALL_LIBDIR=lib',
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',
        '-DFORCE_STATIC_VCRT=ON','-DLIBTYPE=SHARED','-DALSOFT_UTILS=OFF',
        '-DALSOFT_EXAMPLES=OFF','-DALSOFT_TESTS=OFF','-DALSOFT_INSTALL_CONFIG=OFF',
        '-DALSOFT_INSTALL_HRTF_DATA=OFF','-DALSOFT_INSTALL_AMBDEC_PRESETS=OFF',
        '-DALSOFT_INSTALL_EXAMPLES=OFF','-DALSOFT_INSTALL_UTILS=OFF',
        '-DALSOFT_BUILD_ROUTER=OFF','-DALSOFT_EMBED_HRTF_DATA=ON')
    run('cmake','--build',build,'--config','Release','--parallel',args.jobs)
    run('cmake','--install',build,'--config','Release')

if __name__=='__main__': main()
