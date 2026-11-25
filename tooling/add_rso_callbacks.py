"""Seed initializer lists and relocated data function pointers for every RSO."""
from pathlib import Path
import struct,json
r=Path.cwd();p=r/'combined-build/functions.map';s=p.read_text();known={int(l.split()[0],16)for l in s.splitlines()if l.strip()};alltargets=set();audit=[]
for m in json.loads((r/'combined-build/modules.json').read_text())['modules']:
 b=(r/'game/extracted/DATA/files'/m['name']).read_bytes();h=struct.unpack_from('>22I',b);secs=[struct.unpack_from('>II',b,h[3]+i*8)for i in range(h[2])];base=int(m['base'],16);targets=set();counts={}
 for pos in range(h[12],h[12]+h[13],12):
  target,info,addend=struct.unpack_from('>III',b,pos);section,kind=info>>8,info&255
  container=next((i for i,(o,n)in enumerate(secs)if o and o<=target<o+n),None)
  # Section 6 includes switch labels, which are not independent functions.
  if kind==1 and section==3 and container in (4,5,7):
   assert addend%4==0 and addend<secs[3][1]
   targets.add(base+secs[3][0]+addend);counts[container]=counts.get(container,0)+1
 alltargets.update(targets);audit.append({'module':m['name'],'section_reference_counts':counts,'function_targets':len(targets),'added':len(targets-known)})
for a in sorted(alltargets-known):s+=f'{a:08X} func_{a:08X}\n'
p.write_text(s);(r/'combined-build/rso-callback-audit.json').write_text(json.dumps(audit,indent=2));print(json.dumps(audit));print('added',len(alltargets-known),'relocated function pointers; initializer at 8181FBCC:',0x8181fbcc in alltargets)
