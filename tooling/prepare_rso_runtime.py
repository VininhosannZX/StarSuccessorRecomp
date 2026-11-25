"""Derive runtime module identities and instruction relocation sites from RSO records."""
import json,struct
from pathlib import Path
r=Path.cwd();mods=json.loads((r/'combined-build/modules.json').read_text())['modules'];sites=set();rows=[]
u=lambda b,o:struct.unpack_from('>I',b,o)[0]
for m in mods:
 b=(r/'game/extracted/DATA/files'/m['name']).read_bytes();h=struct.unpack_from('>22I',b);sec=b[32];off,size=struct.unpack_from('>II',b,h[3]+sec*8);base=int(m['base'],16)
 name=b[h[4]:].split(b'\0',1)[0].decode('ascii')
 for start,length in [(h[12],h[13]),(h[14],h[15])]:
  for p in range(start,start+length,12):
   target,info,addend=struct.unpack_from('>III',b,p)
   if info&255 in (4,6) and off<=target<off+size:
    assert target%4==2,(m['name'],hex(target));sites.add(base+target-2)
 rows.append({'name':m['name'],'path':name,'base':base,'code_offset':off,'code_size':size,'prolog':int(m['prolog'],16),'bss_size':h[7],'section_count':h[2],'section_table':h[3]})
(r/'combined-build/runtime-immediates.txt').write_text(''.join(f'{p:08X}\n' for p in sorted(sites)))
(r/'combined-build/runtime-modules.json').write_text(json.dumps(rows,indent=2))
(r/'port/rso_modules.inc').write_text('\n'.join('{'+','.join([json.dumps(m['name']),json.dumps(m['path']),*[f'0x{m[k]:X}u' for k in ['base','code_offset','code_size','prolog','bss_size','section_count','section_table']]])+'},' for m in rows)+'\n')
print(len(rows),'modules;',len(sites),'runtime immediate sites')
