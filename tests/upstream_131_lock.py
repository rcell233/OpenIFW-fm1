"""Verify rc14 handoff edits without retiring the older scope freezes."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
audit = json.loads((root / 'docs/evidence/upstream-1.3.1.json').read_text())
def historical_hash(path, data):
    actual = hashlib.sha256(data).hexdigest()
    if path in audit['sources']:
        pin = audit['sources'][path]
        assert actual == pin['rc14'], 'Unreviewed change to the 1.3.1 port: ' + path
        return pin['rc13']
    return actual
