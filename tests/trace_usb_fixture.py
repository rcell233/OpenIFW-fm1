"""Build a host test include from the board's EP1 receive, reply and transmit paths."""
from pathlib import Path
usb=Path('firmware/src/usb.c').read_text()
app=Path('firmware/src/app.c').read_text()
def between(source,start,end):
    a=source.index(start)
    return source[a:source.index(end,a)]
parts=['#define FELUCCA_OTA 1',
    between(usb,'static struct {\n    uint8_t up, config','/* ------------------------------------------------------- descriptors'),
    between(usb,'static void sysex_byte','static void ep1_rx'),
    between(app,'static void trace_reply(void){','static void diagnostics_service(void) {'),
    between(usb,'static void ep1_tx(void)','static void ep3_rx(void)').rsplit('#if FELUCCA_CDC',1)[0]]
Path('build/trs/trace_usb_fixture.inc').write_text('\n'.join(parts))
