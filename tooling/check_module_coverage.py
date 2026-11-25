"""Check independently identified RSO callback targets against compiler output."""
import json
from pathlib import Path
root=Path(__file__).resolve().parent.parent
base=root/'combined-build'
metadata=json.loads((base/'base_translation_output.json').read_text())
translated={f['entryPoint'] for f in metadata['functions']}
required=json.loads((base/'required-module-functions.json').read_text())
modules={name:{'required':len(addresses),'translated':sum(int(a,16) in translated for a in addresses),
               'missing':[a for a in addresses if int(a,16) not in translated]}
         for name,addresses in required.items()}
report={'translated_function_count':len(translated),'quality':metadata['quality'],'modules':modules,
        'missing_count':sum(len(m['missing']) for m in modules.values()),
        'scope':'RSO constructor, destructor and vtable targets; does not prove complete gameplay'}
(base/'module-coverage-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if report['missing_count'] or any(metadata['quality'].values()):raise SystemExit(1)
