#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch the pinned, freely redistributable IWAD and its unmodified notices."""
import hashlib
from pathlib import Path
import shutil
import urllib.request
import zipfile

ROOT=Path(__file__).resolve().parents[1]
URL='https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip'
SHA256='3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59'

def main():
    work=ROOT/'build/assets'
    work.mkdir(parents=True,exist_ok=True)
    archive=work/'freedoom-0.13.0.zip'
    if not archive.exists():
        temporary=archive.with_suffix('.download')
        with urllib.request.urlopen(URL,timeout=60) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response,output)
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA256:
        raise SystemExit('Freedoom archive checksum mismatch; no assets were staged')
    destination=work/'freedoom'
    destination.mkdir(exist_ok=True)
    wanted=['freedoom2.wad','COPYING.txt','CREDITS.txt','CREDITS-MUSIC.txt','README.html','NEWS.html']
    with zipfile.ZipFile(archive) as source:
        for name in wanted:
            (destination/name).write_bytes(source.read('freedoom-0.13.0/'+name))
    (destination/'SOURCE.txt').write_text(f'Freedoom 0.13.0\n{URL}\nArchive SHA-256: {SHA256}\n',encoding='utf-8')
    print(f'Freedoom Phase 2 and notices verified: {destination}')

if __name__=='__main__': main()
