#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the original HELIX control room and its original procedural artwork.

Only Python's standard library is required. WAD/PK3/raster outputs stay in build/.
"""
import argparse
from pathlib import Path
import shutil
import struct
import zlib
import zipfile
ROOT=Path(__file__).resolve().parents[1]
FONT={
'A':['01110','10001','10001','11111','10001','10001','10001'],
'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01111','10000','10000','10111','10001','10001','01110'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['11111','00100','00100','00100','00100','00100','11111'],
'J':['00111','00010','00010','00010','10010','10010','01100'],
'K':['10001','10010','10100','11000','10100','10010','10001'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','10101','10011','10001','10001','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'Q':['01110','10001','10001','10001','10101','10010','01101'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','10101','01010'],
'X':['10001','10001','01010','00100','01010','10001','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'Z':['11111','00001','00010','00100','01000','10000','11111'],
'0':['01110','10011','10101','10101','11001','10001','01110'],
'1':['00100','01100','00100','00100','00100','00100','01110'],
'2':['01110','10001','00001','00010','00100','01000','11111'],
'3':['11110','00001','00001','01110','00001','00001','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010'],
'5':['11111','10000','10000','11110','00001','00001','11110'],
'6':['01110','10000','10000','11110','10001','10001','01110'],
'7':['11111','00001','00010','00100','01000','01000','01000'],
'8':['01110','10001','10001','01110','10001','10001','01110'],
'9':['01110','10001','10001','01111','00001','00001','01110'],
'/':['00001','00001','00010','00100','01000','10000','10000'],
'-':['00000','00000','00000','11111','00000','00000','00000'],
'.':['00000','00000','00000','00000','00000','00110','00110'],
':':['00000','00110','00110','00000','00110','00110','00000'],
'+':['00000','00100','00100','11111','00100','00100','00000'],
' ':['00000']*7,
}
TEAL=(60,225,195,255); AMBER=(247,182,62,255); WHITE=(209,226,224,255)
class Image:
    def __init__(self,w,h,color=(15,25,32,255)):
        self.w,self.h=w,h; self.p=bytearray(color*(w*h))
    def rect(self,x,y,w,h,c):
        for py in range(max(0,y),min(self.h,y+h)):
            for px in range(max(0,x),min(self.w,x+w)):
                self.p[(py*self.w+px)*4:(py*self.w+px)*4+4]=bytes(c)
    def text(self,y,text,scale=1,color=WHITE):
        x=(self.w-len(text)*6*scale)//2
        for char in text.upper():
            for row,bits in enumerate(FONT.get(char,FONT[' '])):
                for col,bit in enumerate(bits):
                    if bit=='1': self.rect(x+col*scale,y+row*scale,scale,scale,color)
            x+=6*scale
    def png(self,sprite=False):
        def chunk(kind,data): return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
        data=b''.join(b'\0'+self.p[y*self.w*4:(y+1)*self.w*4] for y in range(self.h))
        header=struct.pack('>IIBBBBB',self.w,self.h,8,6,0,0,0)
        origin=chunk(b'grAb',struct.pack('>ii',self.w//2,self.h)) if sprite else b''
        return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',header)+origin+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')

def artwork():
    files={}
    for name,kind in [('DIOWALL','wall'),('DIOBASE','base'),('DIOFLOOR','floor'),('DIOGRID','grid'),('DIOCEIL','ceiling')]:
        image=Image(64,64)
        for y in range(64):
            for x in range(64):
                noise=((x*17+y*31)%9)-4
                c=(37+noise,46+noise,51+noise,255)
                if kind in ('wall','base'):
                    c=(32+noise,43+noise,48+noise,255)
                    if y in (2,3,60,61) or x in (0,63): c=(15,24,29,255)
                    if kind=='wall' and y in (14,15): c=TEAL
                    if kind=='base' and y>45 and (x+y)%24<10: c=AMBER
                    if (x%56<3 and y%56<3): c=(97,108,108,255)
                elif kind=='floor':
                    if x%32==0 or y%32==0: c=(15,24,28,255)
                    if 3<x<8 and 3<y<8: c=(69,86,88,255)
                elif kind=='grid':
                    c=(13+noise,40+noise,49+noise,255)
                    if x%16==0 or y%16==0: c=(32,117,129,255)
                    if x%64==0 or y%64==0: c=TEAL
                elif kind=='ceiling':
                    c=(18+noise,24+noise,29+noise,255)
                    if y%8 in (0,1): c=(45,57,61,255)
                image.rect(x,y,1,1,c)
        files['textures/'+name+'.png']=image.png()
    signs={
      'DIOLOGO':('DOOM I/O','HELIX CONTROL','NATIVE SIGNAL LAB'),
      'DIOTRG':('01 / PULSE','SHOOT TO FIRE','ONE SHOT'),
      'DIOBTN':('02 / BUTTONS','SHOOT OR USE','MOMENTARY'),
      'DIOTOG':('03 / TOGGLE','PRESS USE','LATCHED STATE'),
      'DIOANA':('04 / ANALOG','HOLD USE','LOOK UP / DOWN'),
      'DIOPAD':('05 / XY','WALK THE GRID','TWO AXES'),
      'DIOSEL':('06 / SELECT','PRESS USE','THREE MODES'),
      'DIODIAG':('NATIVE BUS','LIVE DIAGNOSTIC','APPROACH TO READ'),
      'DIOEP':('ENDPOINTS','USE TO SELECT','F6 OPEN / F7 ARM'),
    }
    for name,lines in signs.items():
        image=Image(128,192,(22,34,42,255)); image.rect(5,20,118,154,(9,19,27,255))
        image.rect(5,20,118,3,TEAL); image.rect(5,171,118,3,TEAL)
        for i,line in enumerate(lines): image.text(85+i*23,line,1,TEAL if i==0 else WHITE)
        image.text(43,'HELIX',2,WHITE); image.text(153,'DOOM I/O',1,AMBER)
        files['textures/'+name+'.png']=image.png()
    for prefix,label,levels in [('DIBT','PULSE',2),('DIBB','HOLD',2),('DITG','LATCH',2),('DIAN','ANALOG',9),('DISE','MODE',3),('DIDG','BUS',9),('DIEP','PROFILE',2)]:
        for level in range(levels):
            image=Image(128,128,(0,0,0,0))
            image.rect(10,12,108,116,(31,43,50,255)); image.rect(16,18,96,104,(12,23,30,255))
            color=TEAL if prefix not in ('DIBT','DIBB') else AMBER
            image.rect(18,20,92,3,color); image.text(30,label,2,WHITE)
            amount=level/(levels-1)
            image.rect(27,54,74,38,(31,58,65,255)); image.rect(31,58,int(66*amount),30,color)
            if levels==2: image.text(67,'ON' if level else 'OFF',1,WHITE)
            elif prefix=='DISE': image.text(67,str(level+1),2,WHITE)
            else: image.text(98,str(round(amount*100)),1,color)
            image.rect(21,114,86,3,color)
            files['sprites/'+prefix+chr(65+level)+'0.png']=image.png(True)
    return files

def make_map():
    vertices=[]; lookup={}; sectors=[]; sides=[]; lines=[]; edges={}; cells={}
    pillars={(-4,-1),(-4,1),(3,-1),(3,1)}
    def vertex(point):
        if point not in lookup: lookup[point]=len(vertices); vertices.append(point)
        return lookup[point]
    for gy in range(-4,4):
        for gx in range(-6,6):
            if (gx,gy) in pillars: continue
            pit=-2<=gx<2 and -1<=gy<2
            bench=gy==3 or (gx in (-6,5) and -1<=gy<2) or (gy==-4 and gx>=3)
            sector=dict(heightfloor=-8 if pit else 24 if bench else 0,heightceiling=192,
                        texturefloor='DIOGRID' if pit else 'DIOFLOOR',textureceiling='DIOCEIL',lightlevel=184 if pit else 216 if bench else 168)
            index=len(sectors); sectors.append(sector); cells[(gx,gy)]=index
            x,y=gx*128,gy*128
            boundary=[((x,y),(x,y+128),'west'),((x,y+128),(x+128,y+128),'north'),((x+128,y+128),(x+128,y),'east'),((x+128,y),(x,y),'south')]
            for a,b,face in boundary:
                va,vb=vertex(a),vertex(b); key=tuple(sorted((va,vb)))
                texture='DIOWALL'
                if gy==3 and face=='north':
                    texture={-5:'DIOTRG',-4:'DIOTRG',-3:'DIOBTN',-2:'DIOBTN',-1:'DIOLOGO',0:'DIOLOGO',1:'DIOTOG',2:'DIOTOG',3:'DIOSEL',4:'DIOSEL'}.get(gx,texture)
                if gx==-6 and face=='west' and -1<=gy<2: texture='DIOANA'
                if gx==5 and face=='east' and -1<=gy<2: texture='DIODIAG'
                if gy==-4 and face=='south': texture='DIOEP' if gx>=3 else 'DIOPAD' if -2<=gx<2 else 'DIOWALL'
                side=len(sides); sides.append(dict(sector=index,texturetop='DIOWALL',texturebottom='DIOBASE',texturemiddle=texture))
                if key in edges:
                    line=lines[edges[key]]; line['sideback']=side; line['twosided']=True; line['blocking']=False
                    sides[line['sidefront']]['texturemiddle']='-'; sides[side]['texturemiddle']='-'
                else:
                    edges[key]=len(lines); lines.append(dict(v1=va,v2=vb,sidefront=side,blocking=True))
    things=[]
    def thing(x,y,z,t,angle=0,role=0,tid=0):
        item=dict(x=x,y=y,height=z,type=t,angle=angle,arg0=role,id=tid,skill1=True,skill2=True,skill3=True,skill4=True,skill5=True,single=True,coop=True,dm=True)
        ids={1:'trigger.pulse',2:'button.a',3:'button.b',4:'toggle.power',5:'scalar.throttle',6:'vector.pad',7:'selector.mode'}
        if role in ids: item['user_control']=ids[role]
        things.append(item)
    thing(0,-352,0,1,90)
    # UDMF thing height is relative to its sector floor, including benches.
    thing(-512,400,0,11010,270,1,501)
    thing(-384,400,0,11011,270,2,502); thing(-256,400,0,11011,270,3,503)
    thing(256,400,0,11012,270,4,504); thing(-680,64,0,11013,0,5,505)
    thing(0,64,0,11014,0,6,506); thing(512,400,0,11015,270,7,507)
    for y,role in [(-96,2),(-32,3),(32,4),(96,5),(160,6),(224,7)]: thing(696,y,0,11016,180,role)
    thing(480,-400,0,11017,90,0,508)
    for x,y in [(-576,-256),(576,-256),(-576,256),(576,256)]: thing(x,y,0,2028)
    def value(v): return 'true' if v is True else 'false' if v is False else '"'+v+'"' if isinstance(v,str) else str(v)
    def block(kind,items): return kind+'\n{\n'+''.join(f'    {k} = {value(v)};\n' for k,v in items.items())+'}\n'
    text='// Original HELIX control room. SPDX-License-Identifier: GPL-3.0-or-later\nnamespace = "ZDoom";\n'
    for x,y in vertices: text+=block('vertex',dict(x=x,y=y))
    for s in sectors: text+=block('sector',s)
    for s in sides: text+=block('sidedef',s)
    for line in lines: text+=block('linedef',line)
    for item in things: text+=block('thing',item)
    return text

def wad(text):
    lumps=[('DIO01',b''),('TEXTMAP',text.encode()),('ENDMAP',b'')]
    data=bytearray(12); directory=[]
    for name,value in lumps:
        directory.append(struct.pack('<II8s',len(data),len(value),name.encode().ljust(8,b'\0'))); data.extend(value)
    offset=len(data)
    data.extend(b''.join(directory)); data[:12]=struct.pack('<4sII',b'PWAD',len(lumps),offset)
    return bytes(data)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--write-map-source',action='store_true')
    args=parser.parse_args()
    source=make_map()
    if args.write_map_source: (ROOT/'content/maps/DIO01.udmf').write_text(source,encoding='utf-8')
    else: source=(ROOT/'content/maps/DIO01.udmf').read_text(encoding='utf-8')
    files=artwork(); files['maps/DIO01.wad']=wad(source)
    for path in (ROOT/'content').rglob('*'):
        if path.is_file() and path.suffix!='.udmf': files[path.relative_to(ROOT/'content').as_posix()]=path.read_bytes()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    temporary=args.output.with_suffix('.temporary')
    with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(2026,1,1,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
            archive.writestr(info,data)
    temporary.replace(args.output)
    assets=ROOT/'build/assets/freedoom'
    if (assets/'freedoom2.wad').exists():
        shutil.copy2(assets/'freedoom2.wad',args.output.parent/'freedoom2.wad')
        shutil.copytree(assets,args.output.parent/'licenses/Freedoom',dirs_exist_ok=True,ignore=shutil.ignore_patterns('*.wad'))
    print(f'Original HELIX room and {len(files)} resources built: {args.output}')

if __name__=='__main__': main()
