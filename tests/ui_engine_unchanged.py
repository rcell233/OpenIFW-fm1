#!/usr/bin/env python3
"""Keep the original UI freeze and explicitly audit authorized DSP optimizations."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[1]
baseline=json.loads((root/'docs/evidence/ui-engine-baseline.json').read_text())
optimized=json.loads((root/'docs/evidence/dsp-optimization-baseline.json').read_text())['optimizedSha256']
extra=json.loads((root/'docs/evidence/dsp-optimization-baseline.json').read_text()).get('targetSourceSha256',{})
fx=json.loads((root/'docs/evidence/fx-half-baseline.json').read_text())
precision=json.loads((root/'docs/evidence/fx-precision-baseline.json').read_text())
cpu=json.loads((root/'docs/evidence/cpu-optimization-baseline.json').read_text())
kernels=json.loads((root/'docs/evidence/dsp-kernels-baseline.json').read_text())
assert set(fx['sha256'])=={'firmware/src/fixed/effects.c','firmware/src/fixed/synth.h','firmware/src/fixed/reverb_half.h'}
assert fx['previousSha256']=={p:optimized[p] for p in ['firmware/src/fixed/effects.c','firmware/src/fixed/synth.h']}
assert precision['previousSha256']==fx['sha256']
assert set(precision['sha256'])==set(fx['sha256'])
assert set(cpu['sha256'])=={'firmware/src/fixed/synth.c','firmware/src/fixed/synth.h'}
assert cpu['previousSha256']=={p:precision['sha256'].get(p,optimized[p]) for p in cpu['sha256']}
assert set(kernels['sha256'])=={'firmware/src/fixed/synth.c','firmware/src/fixed/math.c','firmware/src/fixed/math.h'}
assert kernels['previousSha256']=={p:cpu['sha256'].get(p,optimized[p]) for p in kernels['sha256']}
assert set(kernels['addedSourceSha256'])=={'firmware/src/fixed/soft_division.h'}
for p,h in precision['sha256'].items():
    assert hashlib.sha256((root/p).read_bytes()).hexdigest()==kernels['sha256'].get(p,cpu['sha256'].get(p,h)),'Authorized FX source changed: '+p
for p,h in cpu['sha256'].items():
    assert hashlib.sha256((root/p).read_bytes()).hexdigest()==kernels['sha256'].get(p,h),'Authorized CPU source changed: '+p
for p,h in {**kernels['sha256'],**kernels['addedSourceSha256']}.items():
    assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,'Authorized kernel source changed: '+p
for p,h in extra.items():
    if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h:raise SystemExit('Target DSP helper changed: '+p)
allowed={'firmware/src/fixed/synth.c','firmware/src/fixed/synth.h',
         'firmware/src/fixed/effects.c','firmware/src/fixed/math.h','firmware/src/fixed/math.c'}
if set(optimized)!=allowed:raise SystemExit('Unexpected DSP optimization exception')
changed=[p for p,h in baseline['sha256'].items()
         if hashlib.sha256((root/p).read_bytes()).hexdigest()!=kernels['sha256'].get(p,cpu['sha256'].get(p,precision['sha256'].get(p,optimized.get(p,h))))]
if changed:raise SystemExit('Frozen engine/preset changed: '+', '.join(changed))
print(f"PASS source audit: {len(baseline['sha256'])-len(optimized)} frozen files; "
      f"{len(optimized)} explicitly recorded performance changes, with separate exact PCM/state regressions.")
for name in ['fm1_input.h','fm1_time.h','fm1_isr.S']:
    assert not (root/'firmware/hal'/name).exists(),name+' must use the original vendor HAL'
print('PASS HAL: input/time/ISR use the original vendor files and Flash placement')
print('PASS authorized FX scope: half-rate high-precision reverb; original core, presets, full-rate goldens and rc4 precision reference retained')
print('PASS exact CPU scope: filter3 loop unrolling and input-keyed rate memoization; no output transitions')
print('PASS exact kernels: original soft-saturator integer results and requested-output wave formulas; old fixtures unchanged')
