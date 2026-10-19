#!/usr/bin/env python3
"""Compare optimized C's complete 166-case PCM and musical state to accepted 0.4.0."""
from pathlib import Path
import hashlib,json,subprocess,sys
r=Path(__file__).resolve().parents[1]
baseline=json.loads((r/'docs/evidence/dsp-optimization-baseline.json').read_text())
for name in ['dsp-regression','dsp-regression-unity']:
    actual=json.loads(subprocess.check_output([r/'build'/name],text=True))
    if actual!=baseline['liveEditHashes']:raise SystemExit(name+': live patch/FX history differs from 0.4.0')
print('PASS live edits: 32,768 frames, 1,024 patch changes; separate modules and -O3 unity match 0.4.0.')
if '--live-only' in sys.argv:raise SystemExit(0)
expected=baseline['frozenPcmAndState']
bad=[name for name,sha in expected.items()
     if not (r/'build/fixed-engine'/name).is_file()
     or hashlib.sha256((r/'build/fixed-engine'/name).read_bytes()).hexdigest()!=sha]
if bad:raise SystemExit('PCM/state changed from 0.4.0: '+', '.join(bad))
print(f'PASS exact optimization regression: {len(expected)} PCM/state files, 166 scenarios match 0.4.0 byte for byte.')
