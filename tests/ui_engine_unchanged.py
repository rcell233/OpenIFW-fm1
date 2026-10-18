#!/usr/bin/env python3
"""The UI revision must retain the accepted DSP, mono controls and programs."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
baseline=json.loads((root/'docs/evidence/ui-engine-baseline.json').read_text())
changed=[p for p,h in baseline['sha256'].items()
         if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]
if changed:raise SystemExit('Frozen engine/preset changed: '+', '.join(changed))
print(f"PASS frozen engine: {len(baseline['sha256'])} DSP / mono / preset files unchanged.")
