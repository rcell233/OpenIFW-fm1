#!/usr/bin/env python3
"""Package source, shared C firmware host, original programs and evidence."""
from pathlib import Path
import json
import zipfile
import hashlib
root=Path(__file__).resolve().parents[1]
dest=root.parent/'deliverables'; dest.mkdir(exist_ok=True)
out=dest/'OpenIFW-FM1-0.4.3.zip'
inputs=[]
for name in ['README.md','LICENSE','THIRD_PARTY_NOTICES.md','Makefile','build.sh','package.json','.gitignore','sources.lock.json']:
    inputs.append((root/name,Path('OpenIFW-FM1')/name))
for directory in ['src','firmware','tools','tests','docs','vendor','web','licenses','assets','.github']:
    for p in sorted((root/directory).rglob('*')):
        rel=p.relative_to(root)
        if p.is_file() and not any(x in {'build','.git','__pycache__'} for x in rel.parts):
            inputs.append((p,Path('OpenIFW-FM1')/rel))
for p in sorted((root/'build/comparison').glob('*.wav')):
    inputs.append((p,Path('OpenIFW-FM1/examples/audio')/p.name))
for name in ['legato-c.wav','legato-js.wav','scenario.json','result.json','legato.ifwtest']:
    inputs.append((root/'build/demo'/name,Path('OpenIFW-FM1/examples/audio')/name))
for name in ['OpenIFW-FM1-0.4.3.fwsc','build.json']:
    inputs.append((root/'build/firmware'/name,Path('OpenIFW-FM1/firmware-bin')/name))
for p in sorted((root/'build/web').rglob('*')):
    if p.is_file():inputs.append((p,Path('OpenIFW-FM1/web-demo')/p.relative_to(root/'build/web')))
inputs.append((root/'build/panel/openifw.wav',Path('OpenIFW-FM1/examples/audio/fixed-panel.wav')))
for p in sorted((root/'build/factory-regression').glob('*.wav')):
    inputs.append((p,Path('OpenIFW-FM1/examples/factory-audio')/p.name))
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for source,name in inputs:
        info=zipfile.ZipInfo(str(name),(2026,10,18,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
        info.external_attr=(0o100644<<16)
        z.writestr(info,source.read_bytes())
with zipfile.ZipFile(out) as z:
    bad=z.testzip()
    if bad: raise SystemExit(f'corrupt archive member: {bad}')
    if any('/assets/logo.' in n for n in z.namelist()):
        raise SystemExit('excluded third-party logo in archive')
    assert 'OpenIFW-FM1/web-demo/openifw.wasm' in z.namelist()
    assert 'OpenIFW-FM1/vendor/openifw/presets/primer.json' in z.namelist()
print(json.dumps({'file':str(out),'files':len(inputs),'bytes':out.stat().st_size,
                  'sha256':hashlib.sha256(out.read_bytes()).hexdigest()},indent=2))
