#!/usr/bin/env python3
"""Preserve rc9 audio/platform state; allow only the requested UI/navigation edits."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
lock=json.loads((root/'docs/evidence/ui-rc9-baseline.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
# The user authorized the pixel font, theme preferences and encoder remap.
# All remaining files, the audio bridge and unrelated scheduler code stay frozen.
authorized_ui={"firmware/src/ui/controls.c","firmware/src/ui/state.h","firmware/src/ui/font.h"}
for path,expected in lock['sha256'].items():
    if path in authorized_ui:continue
    assert sha((root/path).read_bytes())==expected,'UI-only release changed: '+path
app=re.sub(rb'\b407[0-9]{2}\b',b'<release>',(root/'firmware/src/app.c').read_bytes())
app=app.replace(b"SELECT: PAGES / ALG: COLOR",b"SELECT: PAGES / ALG: MODULE")
assert sha(app)==lock['appIgnoringDiagnosticReleaseSha256'],'Audio/app bridge changed'
scheduler=(root/'firmware/src/ui/draw.c').read_bytes().split(b'static void draw_panel(uint32_t full) {',1)[1]
scheduler=scheduler.replace(b'  uint32_t layout_changed=!initialized || ui_force_redraw || memcmp(layout,old_layout,sizeof layout);\n  if(layout_changed || ui_prefs.palette!=old_prefs.palette || ui_prefs.style!=old_prefs.style) {\n    ui_widgets=15u;\n    /* Color changes update the pending bands in place: rapid turns must not\n     * keep restarting at the header and starve the rest of the display. */\n    if(layout_changed)ui_widget_cursor=0;',b'  if(!initialized || ui_force_redraw || memcmp(layout,old_layout,sizeof layout) ||\n     ui_prefs.palette!=old_prefs.palette || ui_prefs.style!=old_prefs.style) {\n    ui_widgets=15u;ui_widget_cursor=0;')
assert sha(scheduler)==lock['displaySchedulerSha256'],'Display scheduling/damage geometry changed'
print(f"PASS UI-only scope: {len(lock['sha256'])-len(authorized_ui)} exact rc9 audio/platform/page-map files, unchanged audio bridge; only explicit hint/palette-cursor edits in the UI scheduler")
