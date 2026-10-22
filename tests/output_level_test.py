#!/usr/bin/env python3
"""Check production output, preset identity and headroom after the FX precision fix."""
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
fields=['preset','name','note','velocity','breath']
loudness=[]
for row,old in zip(rows,before['openifw']):
    assert all(row[f]==old[f] for f in fields),row
    assert row['outputGain']==2 and row['outputBoundaryChannelSamples']==0,row
    assert abs(row['outputPeak']-2*row['peak'])<2e-9,row
    assert abs(row['outputRms']-2*row['rms'])<2e-9,row
    assert row['clippedChannelSamples']==0,row
    delta=20*math.log10(row['rms']/old['rms'])
    loudness.append(dict(preset=row['preset'],note=row['note'],velocity=row['velocity'],breath=row['breath'],rmsDeltaDb=delta))
    # At full breath quantization was already small: preserve useful wet gain.
    # Quiet Q15 wet signals were partly erased; retaining them changes their
    # level, so record that difference instead of freezing the precision bug.
    if row['breath']==1:assert abs(delta)<0.25,'Full-breath wet gain changed: '+str(row)
for row in long['longScenes']:
    assert row['seconds']==100 and row['boundaryChannelSamples']==0,row
unchanged=['firmware/src/fixed/synth.c','firmware/src/presets.h','firmware/src/dsp.c']
for name in unchanged:
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==before['sourceSha256'][name],name
summary=dict(kind='native-output-gain-validation-not-hardware',outputGain=2,gainDb=20*math.log10(2),
    factoryCases=288,presetIdentityExact=True,fxPrecisionChange=True,outputBoundaryChannelSamples=0,
    fullBreathMaxAbsRmsDeltaDb=max(abs(r['rmsDeltaDb']) for r in loudness if r['breath']==1),
    weakBreathMaxRmsDeltaDb=max(r['rmsDeltaDb'] for r in loudness if r['breath']<1),
    weakBreathCasesBeyond3Db=[r for r in loudness if abs(r['rmsDeltaDb'])>=3],
    factoryMaxOutputPeak=max(r['outputPeak'] for r in rows),
    longScenes=long,unchangedSourceSha256={name:before['sourceSha256'][name] for name in unchanged},
    sourceSha256={name:hashlib.sha256((root/name).read_bytes()).hexdigest()
                  for name in ['firmware/src/output_level.h','firmware/src/audio_output.h','web/emu/openifw_web.c']},
    hardwareMeasured=False)
(out/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS output level: 524,288 arithmetic boundaries; 288 factory scenes, full-breath RMS within 0.25 dB of rc4, exact x2 output and no output boundary samples; 600 s held/legato/repeated-note/breath/release playback with no output boundary samples')
