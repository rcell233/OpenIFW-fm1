#!/usr/bin/env python3
"""Preserve rc9 audio/platform state; allow only the requested UI/navigation edits."""
import hashlib,json,re
from upstream_131_lock import audit,historical_hash
from pathlib import Path
root=Path(__file__).resolve().parents[1]
lock=json.loads((root/'docs/evidence/ui-rc9-baseline.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
# The user authorized the pixel font, theme preferences and encoder remap.
# All remaining files, the audio bridge and unrelated scheduler code stay frozen.
authorized_ui={"firmware/src/ui/controls.c","firmware/src/ui/state.h","firmware/src/ui/font.h"}
for path,expected in lock['sha256'].items():
    if path in authorized_ui:continue
    data=(root/path).read_bytes()
    if path=='firmware/src/platform.c':
        # rc15 overlays the pinned UART HAL; only its init, include and count
        # poll are permitted. Original GPIO, USB, boot and audio code stay pinned.
        data=data.replace(b'static uint32_t trs_clock(void){return fm1_ticks();}\n',b'')
        data=data.replace(b'#define FELUCCA_UART 1',b'#define FELUCCA_UART 0')
        data=data.replace(b'#include "fm1_uart.h"\n#include "trs_midi.h"\n',b'')
        data=data.replace(b'    if (sub % 5u == 2u) trs_tick(); /* 2 kHz DMA count only; off the USB poll tick */\n',b'')
        data=data.replace(b'    trs_init(); /* normal boot only; configure PH8 before TIMER5 scans GPIO */\n',b'')
    if path=='firmware/src/panel_params.h':
        data=data.replace(b'{"BREATH CC",0,119,1,0}',b'{"WIND / CC7",0,1,1,1}')
    if path=='firmware/src/ui/pages.h':
        data=re.sub(rb'^\{UG_GLOBAL,UK_BRCURVE,[^\n]*\n',b'',data,flags=re.M)
        data=data.replace(b'UK_BRCURVE,',b'').replace(b'UP_BRCURVE,',b'').replace(b',UC_BRPOINT0=317,UC_BRPOINT1=318,UC_BRPOINT2=319,UC_BRPOINT3=320',b'')
        data=data.replace(b'{"BR CC","MIDI","OCT","SLOT"}',b'{"WIND","MIDI","OCT","SLOT"}')
    if path=='firmware/src/output_bus.h':
        # rc12 explicitly restores NORMAL; no DSP implementation changes.
        data=data.replace(b'#define IFW_OUTPUT_BUS 0 /* Historical LOUD path is opt-in for regression tests only. */',b'#define IFW_OUTPUT_BUS 1')
        data=data.replace(b'b->mode=IFW_OUTPUT_BUS && mode==IFW_OUTPUT_LOUD;',b'b->mode=mode==IFW_OUTPUT_LOUD;')
    assert historical_hash(path,data)==expected,'UI-only release changed: '+path
app=re.sub(rb'\b407[0-9]{2}\b',b'<release>',(root/'firmware/src/app.c').read_bytes())
# rc16 input routing and diagnostics changed; the shared audio bridge and DSP are tested separately.
assert sha(app)=='19675dfd7ff046ad3b3ef23add6c0728627bdba067da557cb73ebc668c4624df','rc16 app bridge changed'
scheduler=(root/'firmware/src/ui/draw.c').read_bytes().split(b'static void draw_panel(uint32_t full) {',1)[1]
# The rc14 live readout and marker add two bounded DMA widgets. Keep the
# exact scheduler body pinned; the actual damage behavior is replayed too.
scheduler=scheduler.replace(b'\n    static uint32_t old_trace_mode;if(old_trace_mode!=mt.mode){old_trace_mode=mt.mode;ui_widgets|=8u;}',b'')
assert sha(scheduler)==audit['drawPanelSha256'],'Display scheduler changed'
print("PASS historical source freezes, reviewed 1.3.1 note handoff and rc14 display scheduler")
