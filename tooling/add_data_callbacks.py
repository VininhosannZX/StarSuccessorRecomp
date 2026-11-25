from pathlib import Path
import struct,json
p=Path('combined-build/functions.map');s=p.read_text();known={int(l.split()[0],16) for l in s.splitlines() if l.strip()}
r=Path('game/extracted/DATA/sys/main.dol').read_bytes();h=struct.unpack_from('>57I',r);targets=set()
for n in range(7,18):
 for (a,) in struct.iter_unpack('>I',r[h[n]:h[n]+h[36+n]]):
  if a%4:continue
  if any(h[18+k]<=a<h[18+k]+h[36+k] for k in range(7) if h[36+k]):targets.add(a)
for a in sorted(targets-known):s+=f'{a:08X} func_{a:08X}\n'
p.write_text(s);print('actual main data code pointers',len(targets),'added',len(targets-known))
