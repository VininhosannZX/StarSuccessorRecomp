"""Catch accidental private game/build payloads in the public source tree."""
from pathlib import Path
import hashlib,json,re,sys
ROOT=Path(__file__).resolve().parent.parent
BLOCKED={'.iso','.rvz','.gcm','.wbfs','.dol','.rso','.rel','.sel','.bin','.exe','.dll','.o','.obj','.a','.lib','.pdb','.wav','.zip','.7z','.log','.pyc'}
PRIVATE={'game','combined-build','dist','build','bin','obj','__pycache__','CMakeFiles'}
def inspect(root):
 failures=[];total=0;largest=0
 for path in root.rglob('*'):
  if not path.is_file() or '.git' in path.relative_to(root).parts:continue
  relative=path.relative_to(root);total+=1;largest=max(largest,path.stat().st_size)
  if path.is_symlink():failures.append(f'{relative}: symlink must not be packaged')
  if path.suffix.lower() in BLOCKED:failures.append(f'{relative}: binary/private extension')
  if any(part in PRIVATE for part in relative.parts):
   # Zstandard's source CMake files live in a legitimate upstream build/ directory.
   if not str(relative).startswith(('tooling/zstd-source/zstd-1.5.7/build/cmake/','project/Wiicompiled/runtime/game/')):
    failures.append(f'{relative}: generated/private directory')
  if path.name=='dsp_boot_program.h':failures.append(f'{relative}: game-derived header')
  if path.stat().st_size>95*1024*1024:failures.append(f'{relative}: too large for the public source package')
  if re.fullmatch(r'(?:base_common_\d+|func_[0-9A-Fa-f]{8})\.cpp',path.name):failures.append(f'{relative}: likely generated guest code')
 return failures,total,largest
if __name__=='__main__':
 failures,total,largest=inspect(ROOT)
 milestone=json.loads((ROOT/'MILESTONE_V20.json').read_text())
 for name,digest in milestone['renderer_sha256'].items():
  if hashlib.sha256((ROOT/'port/ios'/name).read_bytes()).hexdigest()!=digest:failures.append(f'port/ios/{name}: differs from archived v20 milestone')
 for item in failures:print('FAIL',item)
 print(f'Checked {total} files; largest file {largest:,} bytes; {len(failures)} omission/identity errors.')
 raise SystemExit(bool(failures))
