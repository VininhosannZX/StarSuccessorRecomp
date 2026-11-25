assert not __import__("pathlib").Path("combined-build/retranslation-required").exists(), "Translation refresh required before native compilation"
from pathlib import Path
import json
import subprocess,concurrent.futures,hashlib,sys,re
from poll_shards import instrument
root=Path('.').resolve();repo=root/'project/Wiicompiled';windows='windows' in sys.argv
required_path=root/'combined-build/required-module-functions.json'
if required_path.exists():
 metadata=json.loads((root/'combined-build/base_translation_output.json').read_text())
 translated={f['entryPoint'] for f in metadata['functions']}
 missing={name:[address for address in addresses if int(address,16) not in translated]
          for name,addresses in json.loads(required_path.read_text()).items()}
 missing={name:addresses for name,addresses in missing.items() if addresses}
 if missing:raise RuntimeError('Required RSO callbacks are not translated: '+json.dumps(missing))
 if any(metadata.get('quality',{}).get(name,0) for name in ('unsupportedInstructionCount','invalidSsaFunctionCount')):
  raise RuntimeError('Translated functions failed compiler quality checks')
optimized='o1' in sys.argv
out=root/'combined-build'/('windows-o0' if windows and 'o0' in sys.argv else 'windows' if windows else 'native-o1' if optimized else 'native');out.mkdir(exist_ok=True);obj=out/'obj';obj.mkdir(exist_ok=True)
clang=next((root/'tooling').glob('llvm-mingw-*/bin/'+('x86_64-w64-mingw32-clang++' if windows else 'clang++')))
shards=root/'combined-build/shards';boot=root/'deliverables/StarSuccessorBoot9'
sources=[]
# Use the compiler's graph, so an extracted checkpoint cannot pick up stale
# shards left over from a previous build.
graph=(shards/'shards.cmake').read_text()
for group in ['MKW_BASE_COMMON_SHARDS','MKW_BASE_REGISTRATION_SOURCES']:
 match=re.search(r'set\('+group+r'\s+(.*?)\)',graph,re.S)
 if match:
  for value in re.findall(r'"([^"\n]+\.cpp)"',match.group(1)):
   parts=value.replace('\\','/').split('/')
   sources.append(shards/parts[-2]/parts[-1])
assert len(sources)>=80, 'Compiler shard graph is missing'
sources+=[repo/'runtime/src'/n for n in ['abi_bridge.cpp','fpu_helpers.cpp','memory.cpp','ppc_helpers.cpp','ppc_quantized.cpp','guest_flat_memory.cpp','hle/hollywood_gpio.cpp']]
sources += list((root/'port/native').glob('*.cpp')) + list((root/'port').glob('*.cpp')) + list((root/'port/ios').glob('*.cpp'))
sources += [repo/'runtime/game/guest_region.cpp',root/'combined-build/generated/data_sections_init.cpp',root/'combined-build/generated/data_sections_init_blobs.S',boot/'boot_main.cpp']
flags=[] if windows else ['--target=x86_64-linux-gnu','-stdlib=libstdc++','-fno-pie']
flags += ['-std=c++17','-march=x86-64-v2','-DSTAR_RSO_RUNTIME','-DTARGET_PC','-DNOMINMAX','-DMKW_TRANSLATED_GUEST_REGION=0','-fno-fast-math','-ffp-contract=off','-w']
sdlwin=root/'tooling/sdl3-windows/SDL3-3.2.10/x86_64-w64-mingw32'
sdl_static=root/'tooling/sdl3-static-build/libSDL3.a'
zstd_static=root/'tooling/zstd-static-windows/lib/libzstd.a'
include_paths=[root/'port/include',root/'port/ios',repo/'runtime/include',repo/'runtime/src',repo,root/'combined-build/generated']
include_paths+=([sdlwin/'include',root/'tooling/graphics-headers'] if windows else [root/'tooling/prefix/usr/include'])
for p in include_paths:flags+=['-I',str(p)]
def build(src):
 target=obj/(src.stem+'.o');stamp=target.with_suffix('.hash');extra=b''.join(h.read_bytes() for h in sorted((root/'port').glob('*.hpp'))) if src.stem in ('star_disc','star_launcher','boot_main') else b''
 extra+=(repo/'runtime/include/guest_abi/memory_access.h').read_bytes()+(repo/'runtime/include/guest_abi/dispatch.h').read_bytes()+b''.join(h.read_bytes() for h in sorted((root/'port').rglob('*.h')))
 if src.stem=='boot_main':extra+=b''.join(f.read_bytes() for f in sorted(boot.glob('*.h')))
 body=src.read_bytes()
 if src.parent.name=='base_common':
  traits=src.with_name(src.stem+'_traits.h')
  if traits.exists():extra+=traits.read_bytes()
  patched,count=instrument(body.decode());body=patched.encode()
 local_flags=flags+(['-O2'] if src.parent.name!='base_common' and src.suffix!='.S' else ['-O1'] if optimized and src.suffix!='.S' else [])
 digest=hashlib.sha256(body+repr(local_flags).encode()+extra).hexdigest()
 if target.exists() and stamp.exists() and stamp.read_text()==digest:return target
 compile_source=src
 if src.parent.name=='base_common':
  staged=out/'polled-shards';staged.mkdir(exist_ok=True)
  compile_source=staged/src.name
  # The translator's traits header remains next to its canonical shard.
  body=body.replace(('#include "'+src.stem+'_traits.h"').encode(),('#include "'+str(src.with_name(src.stem+'_traits.h'))+'"').encode())
  compile_source.write_bytes(body)
 if windows and src.suffix=='.S':
  compile_source=out/src.name
  compile_source.write_text(src.read_text().replace('.section .rodata,"a",@progbits','.section .rdata,"dr"').replace('.section .note.GNU-stack,"",@progbits',''))
 r=subprocess.run([str(clang),*local_flags,'-c',str(compile_source),'-o',str(target)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode:
  (out/(src.stem+'.error.log')).write_text(r.stdout);raise RuntimeError(src.name+'\n'+r.stdout[:1500])
 stamp.write_text(digest);return target
objects=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=2 if optimized else 4) as pool:
 for i,o in enumerate(pool.map(build,sources),1):
  objects.append(o)
  if i%10==0:print('compiled',i,'/',len(sources),flush=True)
 (out/'build-inputs.json').write_text(__import__('json').dumps({'optimized':optimized,'windows':windows,'sources':[str(p.relative_to(root)) for p in sources]},indent=2))
wrap=['PPC_WriteSpr','PPC_ReadSpr','_ZN12MemoryInline10Read16SlowEj','_ZN12MemoryInline11Write16SlowEjt','_ZN12MemoryInline10Read32SlowEj','_ZN12MemoryInline11Write32SlowEjj','_Z31FilterScnRendererPathMaskOptionj']
exe=out/('StarSuccessorBoot.exe' if windows else 'StarSuccessorBoot');linkflags=['-static','-mwindows','-Wl,--stack,67108864'] if windows else ['--target=x86_64-linux-gnu','-stdlib=libstdc++','-no-pie']
cmd=[str(clang),*linkflags,*map(str,objects),*[f'-Wl,--wrap={s}' for s in wrap],*(['-ldbghelp',str(sdl_static if sdl_static.exists() else sdlwin/'lib/libSDL3.dll.a'),str(zstd_static),*['-l'+n for n in ['m','kernel32','user32','gdi32','winmm','imm32','ole32','oleaut32','version','uuid','advapi32','setupapi','shell32','dinput8']]] if windows else []),*(['-L'+str(root/'tooling/prefix/usr/lib/x86_64-linux-gnu'),'-Wl,-rpath,'+str(root/'tooling/prefix/usr/lib/x86_64-linux-gnu'),'-lSDL3',str(root/'tooling/prefix/usr/lib/x86_64-linux-gnu/libzstd.so.1')] if not windows else []),'-o',str(exe)]
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(out/'link.log').write_text(r.stdout);print('link',r.returncode,r.stdout[:1500],flush=True);raise SystemExit(r.returncode)
