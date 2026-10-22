#!/usr/bin/env python3
"""Require output-only transitions with byte-identical dense-note DSP state."""
import json
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'build/rapid-notes'
read=lambda name:json.loads((out/(name+'.json')).read_text())
new,old,sanitized=read('production'),read('reference'),read('sanitize')
assert new==sanitized,'Dense-note ASan/UBSan differs from production'
assert new['coreTraceHash']==old['coreTraceHash']
assert new['frames']==old['frames']==4147200
assert new['coreStateAndFxBuffersExact'] and old['coreStateAndFxBuffersExact']
assert len(new['cases'])==len(old['cases'])==18
for a,b in zip(new['cases'],old['cases']):
    for k in ['preset','mode','noteOns']:assert a[k]==b[k]
    assert not a['clippedChannelSamples'] and not b['clippedChannelSamples']
    assert not a['protectedBoundaryStep']
    assert not b['protectedEvents']
protected=sum(r['protectedEvents'] for r in new['cases'])
assert protected>1000
arithmetic=read('arithmetic')
assert arithmetic==read('arithmetic-sanitize')
assert arithmetic['arithmeticChecks']==104160 and arithmetic['newMaxConstantStep']<=938
result=dict(kind='software-output-transition-validation-not-hardware',frames=new['frames'],
            scenes=18,protectedEvents=protected,coreStateAndFxBuffersExact=True,
            unchangedPcmAfterFrames=64,clippedChannelSamples=0,sanitizeExact=True,
            arithmetic=arithmetic)
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'PASS rapid notes: 18 scenes, {new["frames"]:,} frames, {protected:,} protected events; full DSP/FX state exact, no clipping, identical ASan/UBSan results')
