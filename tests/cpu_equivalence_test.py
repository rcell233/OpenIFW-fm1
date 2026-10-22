#!/usr/bin/env python3
"""Compare CPU changes to the locked, pre-change rc5 production traces."""
import hashlib,json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
fixture=json.loads((root/'docs/evidence/cpu-equivalence-rc5.json').read_text())
assert hashlib.sha256((root/'tests/cpu_equivalence.c').read_bytes()).hexdigest()==fixture['harnessSha256']
expected=fixture['trace']
assert len(expected['cases'])==24 and expected['frames']==2359296
for name in ['production','sanitize']:
    actual=json.loads((root/f'build/cpu-equivalence/{name}.json').read_text())
    assert actual==expected,name+' changed rc5 PCM, state or FX delay memory'
print('PASS exact rc5 production: 24 dense-note/live-rate-edit scenes, 2,359,296 frames; '
      'PCM, complete pre-existing state and FX memory hashes match; ASan/UBSan identical')
