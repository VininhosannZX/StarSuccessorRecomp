"""Prepare pinned Windows cross-build dependencies on Linux x86_64/WSL2."""
from pathlib import Path
import argparse,hashlib,os,shutil,subprocess,tarfile,urllib.request
ROOT=Path(__file__).resolve().parent.parent
PINS=[('llvm-mingw.tar.xz','https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-ubuntu-22.04-x86_64.tar.xz','e7a15a49a2ea95e9f3d8a9d0cfabe5052a4b10fb4777cc54de5a8c27b64bb701',ROOT/'tooling'),('SDL3-src.tar.gz','https://github.com/libsdl-org/SDL/releases/download/release-3.2.10/SDL3-3.2.10.tar.gz','f87be7b4dec66db4098e9c167b2aa34e2ca10aeb5443bdde95ae03185ed513e0',ROOT/'tooling/sdl3-source')]
def command(args):subprocess.run(list(map(str,args)),cwd=ROOT,check=True)
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--jobs',type=int,default=2);parser.add_argument('--check',action='store_true');args=parser.parse_args()
 if args.check:
  needed=['tooling/sdl3-static-build/libSDL3.a','tooling/zstd-static-windows/lib/libzstd.a','tooling/sdl3-windows/SDL3-3.2.10/x86_64-w64-mingw32/include/SDL3/SDL.h']
  missing=[str(p) for p in needed if not (ROOT/p).is_file()]
  if not list((ROOT/'tooling').glob('llvm-mingw-*/bin/x86_64-w64-mingw32-clang++')):missing.append('LLVM-MinGW cross compiler')
  if missing:raise SystemExit('Missing dependencies:\n'+'\n'.join(missing))
  print('Cross-build dependencies found');return
 if os.name=='nt':raise SystemExit('Run this build helper in Linux x86_64 or WSL2; the output is Windows x64.')
 if args.jobs<1:raise SystemExit('--jobs must be positive')
 for filename,url,digest,destination in PINS:
  archive=ROOT/'tooling'/filename
  if not archive.exists():
   print('Downloading',filename,flush=True);temporary=archive.with_suffix(archive.suffix+'.part')
   with urllib.request.urlopen(url,timeout=120) as source,temporary.open('wb') as out:shutil.copyfileobj(source,out)
   temporary.replace(archive)
  if hashlib.sha256(archive.read_bytes()).hexdigest()!=digest:raise SystemExit('Archive checksum mismatch: '+filename)
  destination.mkdir(parents=True,exist_ok=True)
  with tarfile.open(archive) as tar:
   for member in tar.getmembers():
    if not (destination/member.name).resolve().is_relative_to(destination.resolve()):raise SystemExit('Unsafe archive path')
   tar.extractall(destination,filter='data')
 sdl=ROOT/'tooling/sdl3-source/SDL3-3.2.10';headers=ROOT/'tooling/sdl3-windows/SDL3-3.2.10/x86_64-w64-mingw32/include';shutil.copytree(sdl/'include',headers,dirs_exist_ok=True)
 toolchain=ROOT/'tooling/windows-clang.cmake'
 command(['cmake','-S',sdl,'-B','tooling/sdl3-static-build',f'-DCMAKE_TOOLCHAIN_FILE={toolchain}','-DCMAKE_BUILD_TYPE=Release','-DSDL_SHARED=OFF','-DSDL_STATIC=ON','-DSDL_TESTS=OFF','-DSDL_TEST_LIBRARY=OFF'])
 command(['cmake','--build','tooling/sdl3-static-build','--parallel',args.jobs])
 command(['cmake','-S','tooling/zstd-source/zstd-1.5.7/build/cmake','-B','tooling/zstd-static-windows',f'-DCMAKE_TOOLCHAIN_FILE={toolchain}','-DCMAKE_BUILD_TYPE=Release','-DZSTD_BUILD_PROGRAMS=OFF','-DZSTD_BUILD_TESTS=OFF','-DZSTD_BUILD_SHARED=OFF','-DZSTD_BUILD_STATIC=ON'])
 command(['cmake','--build','tooling/zstd-static-windows','--parallel',args.jobs])
if __name__=='__main__':main()
