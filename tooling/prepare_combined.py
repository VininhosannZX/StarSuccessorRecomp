"""Prepare a fixed-address, relocated analysis image from user-supplied Wii files.
This is an offline recompilation input. Runtime RSO loading is a separate integration step.
"""
import struct,json,hashlib,csv
from pathlib import Path
root=Path('.').resolve()
source=root/'game/extracted/DATA'
output=root/'combined-build'
output.mkdir(exist_ok=True)
main=(source/'sys/main.dol').read_bytes()
header=struct.unpack_from('>57I',main)
def u32(d,o):return struct.unpack_from('>I',d,o)[0]
def cstring(d,o):
 end=d.find(b'\0',o)
 if o>=len(d) or end<0:raise ValueError('invalid symbol string')
 return d[o:end].decode('ascii')
def load_rso(path):
 d=path.read_bytes();h=struct.unpack_from('>22I',d)
 sections=[struct.unpack_from('>II',d,h[3]+8*i) for i in range(h[2])]
 exports=[]
 for i in range(h[16],h[16]+h[17],16):
  name,value,section,hashv=struct.unpack_from('>4I',d,i)
  exports.append((cstring(d,h[18]+name),value,section))
 imports=[]
 for i in range(h[19],h[19]+h[20],12):
  name,value,reloc=struct.unpack_from('>3I',d,i)
  imports.append(cstring(d,h[21]+name))
 return d,h,sections,exports,imports

# Section identities are independently checked against DOL section ranges.
# _text export "abs" at 0x80145A54 was verified against its four PPC instructions.
main_sections={1:(0x80004000,0x26c8),2:(0x8001f240,0x2b5eb8),
 3:(0x802d5100,0x124),4:(0x802d5240,0xc),5:(0x802d5280,0x1ffd0),
 6:(0x802f5260,0x3f3e4),7:(0x80334680,0x17b070),
 8:(0x804af700,0x2244),9:(0x804b2920,0x3eb0),
 10:(0x804b67e0,0xe0),11:(0x804b1960,0xfac)}
_,_,_,main_exports,_=load_rso(source/'files/gs2.sel')
symbols={}
seeds={header[56]}
labels=[]
for name,value,section in main_exports:
 if section==0xfff1:address=value
 else:
  base,size=main_sections[section]
  if value>=size:raise ValueError(f'main symbol out of range: {name}')
  address=base+value
 symbols[name]=address
 labels.append((hex(address),name,'main'))
 if section in (1,2) and address%4==0:seeds.add(address)
seed_path=root/'config/function-seeds.map'
seeds.update(int(line.split()[0],16) for line in seed_path.read_text().splitlines() if line.strip())
cursor=0x81800000
modules=[]
for path in sorted((source/'files').glob('*.rso')):
 d,h,sections,exports,imports=load_rso(path)
 load_end=max(off+size for off,size in sections if off)
 bss_off=(load_end+31)&~31
 blob=bytearray(bss_off+h[7])
 for off,size in sections:
  if off:
   if off+size>len(d):raise ValueError('section bounds')
   blob[off:off+size]=d[off:off+size]
 bases={i:cursor+(off if off else bss_off) for i,(off,size) in enumerate(sections) if size}
 missing=[n for n in imports if n not in symbols]
 if missing:raise ValueError(f'{path.name}: missing imports {missing}')
 count=0
 empty_targets=[]
 for start,size,external in [(h[12],h[13],False),(h[14],h[15],True)]:
  if size%12 or start+size>len(d):raise ValueError('relocation table bounds')
  for off in range(start,start+size,12):
   target,info,addend=struct.unpack_from('>3I',d,off)
   index,kind=info>>8,info&255
   if external:
    symbol=symbols[imports[index]]
   else:
    symbol=bases.get(index,0)
    if index not in bases:
     empty_targets.append({'offset':hex(target),'section':index,'addend':hex(addend)})
   value=(symbol+addend)&0xffffffff
   width=2 if kind in (4,6) else 4
   if target+width>load_end:raise ValueError('relocation target outside sections')
   if kind==1:struct.pack_into('>I',blob,target,value)
   elif kind==4:struct.pack_into('>H',blob,target,value&0xffff)
   elif kind==6:struct.pack_into('>H',blob,target,((value+0x8000)>>16)&0xffff)
   elif kind==10:
    delta=value-(cursor+target)
    if not -(1<<25)<=delta<(1<<25) or delta%4:raise ValueError('REL24 out of range')
    old=u32(blob,target)
    if old>>26!=18:raise ValueError('REL24 instruction is not b/bl')
    struct.pack_into('>I',blob,target,(old&0xfc000003)|(delta&0x03fffffc))
   else:raise ValueError(f'unsupported relocation {kind}')
   count+=1
 prolog_section=d[0x20]
 prolog=bases[prolog_section]+h[9]
 text_start,text_size=sections[prolog_section]
 for name,value,section in exports:
  if section in bases and value<sections[section][1]:
   addr=bases[section]+value
   labels.append((hex(addr),name,path.stem))
   if section==prolog_section and addr%4==0:seeds.add(addr)
 seeds.add(prolog)
 modules.append({'name':path.name,'sha256':hashlib.sha256(d).hexdigest(),
   'base':hex(cursor),'size':len(blob),'prolog':hex(prolog),'relocations':count,
   'imports':len(imports),'unresolved_imports':0,'empty_section_references':empty_targets,'image':blob})
 cursor=(cursor+len(blob)+0xffff)&~0xffff
if cursor>0x82000000:raise ValueError('modules exceed near-code overlay')
start=0x81800000
module_blob=bytearray(cursor-start)
for mod in modules:
 offset=int(mod['base'],16)-start
 module_blob[offset:offset+mod['size']]=mod.pop('image')
# Copy the DOL into a new file, adding the relocated module image as a third text section.
combined=bytearray(main)
pad=(-len(combined))%32;combined.extend(bytes(pad));file_offset=len(combined)
combined.extend(module_blob)
struct.pack_into('>I',combined,2*4,file_offset)
struct.pack_into('>I',combined,(18+2)*4,start)
struct.pack_into('>I',combined,(36+2)*4,len(module_blob))
(output/'combined-analysis.dol').write_bytes(combined)
# Conservative callback seeds from actual pointer values in loaded data.
# Accept stack-frame + mflr prologues only; arbitrary aligned code is not an oracle.
code_regions=[(header[18+i],header[36+i],header[i]) for i in range(2)]
for mod in modules:
 raw,h,secs,exps,imps=load_rso(source/'files'/mod['name'])
 off,size=secs[raw[0x20]]
 code_regions.append((int(mod['base'],16)+off,size,file_offset+int(mod['base'],16)-start+off))
data_regions=[(header[i],header[36+i]) for i in range(7,18) if header[36+i]]
data_regions.append((file_offset,len(module_blob)))
callback_seeds=set()
for off,size in data_regions:
 for (target,) in struct.iter_unpack('>I',combined[off:off+size]):
  if target&3:continue
  for base,length,codeoff in code_regions:
   if base<=target<base+length-8:
    pos=codeoff+target-base
    first,second=struct.unpack_from('>II',combined,pos)
    if first == 0x4e800020 or (first&0xffff0000==0x94210000 and first&0x8000 and second==0x7c0802a6):
     callback_seeds.add(target)
    break
seeds.update(callback_seeds)
print('Conservative callback pointer seeds',len(callback_seeds),flush=True)
(output/'functions.map').write_text('\n'.join(f'{a:08X} func_{a:08X}' for a in sorted(seeds) if a!=0x801a6a10)+'\n')
with (output/'symbols.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['address','name','module']);w.writerows(labels)
(output/'modules.json').write_text(json.dumps({'purpose':'offline analysis only; fixed addresses need runtime loader integration and reserved memory','module_memory_start':hex(start),'module_memory_end':hex(cursor),'modules':modules},indent=2))
(root/'combined-build/project.yml').write_text(f'''schema_version: 1
workspace_root: "{root}"
project:
  id: star-successor
  display_name: Star Successor combined analysis
memory:
  base: 0x80000000
  size: 0x02000000
  sda_base: 0x804B7700
  sda2_base: 0x804BA920
inputs:
  dol:
    path: "{output}/combined-analysis.dol"
translation:
  allow_unsupported_instructions: false
  function_map:
    path: "{output}/functions.map"
runtime:
  native_registration_root: port/native
  native_index: port/native-index.json
  native_abi_directories: [port/native]
output:
  root: combined-build/generated
  functions: functions
  runtime_config: RuntimeConfig.h
  data_initializer: data_sections_init.cpp
  base_manifest: base.json
''')
print(json.dumps({'modules':len(modules),'relocations':sum(m['relocations'] for m in modules),'import_entries':sum(m['imports'] for m in modules),'seeds':len(seeds),'image_end':hex(cursor),'combined_bytes':len(combined)},indent=2))
