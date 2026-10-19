#!/usr/bin/env python3
"""Keep the original UI freeze and explicitly audit authorized DSP optimizations."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
baseline=json.loads((root/'docs/evidence/ui-engine-baseline.json').read_text())
optimized=json.loads((root/'docs/evidence/dsp-optimization-baseline.json').read_text())['optimizedSha256']
extra=json.loads((root/'docs/evidence/dsp-optimization-baseline.json').read_text()).get('targetSourceSha256',{})
for p,h in extra.items():
    if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h:raise SystemExit('Target DSP helper changed: '+p)
allowed={'firmware/src/fixed/synth.c','firmware/src/fixed/synth.h',
         'firmware/src/fixed/effects.c','firmware/src/fixed/math.h','firmware/src/fixed/math.c'}
if set(optimized)!=allowed:raise SystemExit('Unexpected DSP optimization exception')
changed=[p for p,h in baseline['sha256'].items()
         if hashlib.sha256((root/p).read_bytes()).hexdigest()!=optimized.get(p,h)]
if changed:raise SystemExit('Frozen engine/preset changed: '+', '.join(changed))
print(f"PASS source audit: {len(baseline['sha256'])-len(optimized)} frozen files; "
      f"{len(optimized)} explicitly recorded performance changes, with separate exact PCM/state regressions.")
