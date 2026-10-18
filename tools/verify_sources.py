#!/usr/bin/env python3
"""Verify that the JS oracle and hardware platform remain unchanged."""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
lock=json.loads((root/'sources.lock.json').read_text())
for name,sha in lock['sha256'].items():
    p=root/name
    if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=sha:
        raise SystemExit(f'upstream source changed or missing: {name}')
print(f"PASS {len(lock['sha256'])} pinned source checksums")
