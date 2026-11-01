#!/usr/bin/env python3
"""This UI release must preserve the rc9 audio, controls and display scheduler."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
lock=json.loads((root/'docs/evidence/ui-rc9-baseline.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
for path,expected in lock['sha256'].items():
    assert sha((root/path).read_bytes())==expected,'UI-only release changed: '+path
app=re.sub(rb'\b407[0-9]{2}\b',b'<release>',(root/'firmware/src/app.c').read_bytes())
assert sha(app)==lock['appIgnoringDiagnosticReleaseSha256'],'Audio/app bridge changed'
scheduler=(root/'firmware/src/ui/draw.c').read_bytes().split(b'static void draw_panel(uint32_t full) {',1)[1]
assert sha(scheduler)==lock['displaySchedulerSha256'],'Display scheduling/damage geometry changed'
print(f"PASS UI-only scope: {len(lock['sha256'])} exact rc9 audio/control/platform/font files, unchanged app bridge and display scheduler")
