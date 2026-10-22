#!/usr/bin/env python3
"""Check actual production output and preserve the measured pre-gain PCM."""
import hashlib
import json
import math
from pathlib import Path

root=Path(__file__).resolve().parents[1]
out=root/'build/output-level'
rows=json.loads((out/'factory.json').read_text())
before=json.loads((root/'docs/evidence/output-level-0.4.7-rc3.json').read_text())
long=json.loads((out/'long-scenes.json').read_text())
assert len(rows)==len(before['openifw'])==288
assert long['arithmeticChecks']==524288 and len(long['longScenes'])==6
fields=['preset','name','note','velocity','breath','peak','rms','steadyRms','unclippedPeak',
        'clippedChannelSamples','gain2ClippedChannelSamples','gain4ClippedChannelSamples']
for row,old in zip(rows,before['openifw']):
    assert all(row[f]==old[f] for f in fields),row
    assert row['outputGain']==2 and row['outputBoundaryChannelSamples']==0,row
    assert abs(row['outputPeak']-2*row['peak'])<2e-9,row
    assert abs(row['outputRms']-2*row['rms'])<2e-9,row
for row in long['longScenes']:
    assert row['seconds']==100 and row['boundaryChannelSamples']==0,row
unchanged=['firmware/src/fixed/synth.c','firmware/src/fixed/effects.c',
           'firmware/src/fixed/reverb_half.h','firmware/src/presets.h','firmware/src/dsp.c']
for name in unchanged:
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==before['sourceSha256'][name],name
summary=dict(kind='native-output-gain-validation-not-hardware',outputGain=2,gainDb=20*math.log10(2),
    factoryCases=288,preGainMeasurementsExact=True,outputBoundaryChannelSamples=0,
    factoryMaxOutputPeak=max(r['outputPeak'] for r in rows),
    longScenes=long,unchangedSourceSha256={name:before['sourceSha256'][name] for name in unchanged},
    sourceSha256={name:hashlib.sha256((root/name).read_bytes()).hexdigest()
                  for name in ['firmware/src/output_level.h','firmware/src/audio_output.h','web/emu/openifw_web.c']},
    hardwareMeasured=False)
(out/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS output level: 524,288 arithmetic boundaries; 288 unchanged pre-gain factory scenes, exact x2 output and no output boundary samples; 600 s held/legato/repeated-note/breath/release playback with no output boundary samples')
