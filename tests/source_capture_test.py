#!/usr/bin/env python3
"""Unaligned live edits must preserve the old sample's waveform CVs and PCM."""
import hashlib,json,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
gold=json.loads((r/'docs/evidence/source-capture-rc1.json').read_text())
assert hashlib.sha256((r/'tests/source_capture.c').read_bytes()).hexdigest()==gold['fixtureSha256']
actual=json.loads(subprocess.check_output([r/'build/source-capture'],text=True))
assert actual==gold['hashes'],'PCM, exposed raw CVs or retained effect buffers differ from 0.4.6-rc1'
print('PASS deferred waves: 65,536 frames, 1,776 unaligned patch edits, all raw CVs and FX history match 0.4.6-rc1')
