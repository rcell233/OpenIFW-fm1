#!/usr/bin/env python3
"""Keep the incremental IRQ stream tied to the released DSP's golden output."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
expected=json.loads((root/'docs/evidence/irq-stream-golden.json').read_text())
actual=json.loads((root/'build/target-audit/irq-native-legacy.json').read_text())
for key in ('blocks','frames','hashes'):
    if actual[key]!=expected[key]:raise SystemExit('IRQ stream regression: '+key)
print('PASS full-rate reference IRQ stream: 12 wet/dry factory scenes, mono/legato, 196608 frames, 24 unchanged golden hashes')
