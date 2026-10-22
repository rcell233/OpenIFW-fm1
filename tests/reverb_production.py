#!/usr/bin/env python3
"""Check the shipped half-rate path against untouched core PCM/state fixtures."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root/'build/fixed-engine';out=root/'build/reverb-production';out.mkdir(exist_ok=True)
cases=json.loads((source/'cases.json').read_text())
assert len(cases)==166
core=pcm=0
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for case in cases:
    name=case['id'];old=source/name;new=out/name
    subprocess.run([str(root/'build/ifw-fixed'),str(old)+'.fixed',str(new)],check=True,capture_output=True)
    assert sha(Path(str(new)+'.state.f64'))==sha(Path(str(old)+'.state.f64')),name+': core state changed'
    core+=1
    if case.get('dry'):
        assert sha(Path(str(new)+'.f32'))==sha(Path(str(old)+'.f32')),name+': dry audio changed'
        pcm+=1
assert pcm>100
result=dict(reverbRate=22050,coreStateCases=core,exactDryPcmCases=pcm,oldGoldensUpdated=False)
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'PASS production reverb: {core} exact musical state traces; {pcm} unchanged dry PCM scenes; old full-rate wet goldens retained separately.')
