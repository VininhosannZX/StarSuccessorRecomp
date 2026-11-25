"""Audit absolute internal code references in all original stage RSO sections."""
from pathlib import Path
import collections,json,struct
root=Path(__file__).resolve().parent.parent
metadata=json.loads((root/'combined-build/base_translation_output.json').read_text())
entries={f['entryPoint'] for f in metadata['functions']}
labels={pc for f in metadata['functions'] for pc in f.get('localLabelAddresses',[])}
counts=collections.Counter();missing={}
for module in json.loads((root/'combined-build/modules.json').read_text())['modules']:
 raw=(root/'game/extracted/DATA/files'/module['name']).read_bytes()
 header=struct.unpack_from('>22I',raw)
 sections=[struct.unpack_from('>II',raw,header[3]+i*8) for i in range(header[2])]
 base=int(module['base'],16);unknown=set()
 for offset in range(header[12],header[12]+header[13],12):
  destination,info,addend=struct.unpack_from('>III',raw,offset)
  section,kind=info>>8,info&255
  if kind!=1 or section!=3:continue
  assert addend%4==0 and addend<sections[3][1]
  container=next((i for i,(start,length) in enumerate(sections) if start and start<=destination<start+length),None)
  address=base+sections[3][0]+addend
  category='entry' if address in entries else 'local_label' if address in labels else 'uncovered'
  counts[(container,category)]+=1
  if category=='uncovered':unknown.add(address)
 missing[module['name']]=[f'{address:08X}' for address in sorted(unknown)]
report={'scope':'absolute internal code references in all RSO source sections; does not prove full gameplay','reference_counts':{str(k):v for k,v in counts.items()},'uncovered':missing}
(root/'combined-build/all-rso-code-reference-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if any(missing.values()):raise SystemExit(1)
