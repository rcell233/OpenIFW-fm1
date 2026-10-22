#!/usr/bin/env python3
"""Run the production boot-progress and rescue snapshot code with mocked USB."""
import os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];out=r/'build/target-audit';out.mkdir(parents=True,exist_ok=True)
app=(r/'firmware/src/app.c').read_text();start=app.index('static void diagnostics_service(void) {')
body=app[start:app.index('\n#endif',start)]
source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IFW_HOST
#include "audio_perf.h"
#include "boot_diagnostics.h"
#include "diagnostics_protocol.h"
#define UI_WIDGET_COUNT 6u
#define MQ 64u
#define FS 44100u
#define HALF_FRAMES 256u
#define FM1_CRASH_MAGIC 0x43525348u
static uint32_t ui_widget_max_ticks[6],mo_w,mo_r,fm1_ms,preset,recovery_active;
static uint32_t ui_poll_max_gap,ui_frame_count,ui_lcd_pixels;
static struct{uint32_t scans,scan_ticks,scan_max_ticks,usb_polls,usb_ticks,usb_max_ticks;}platform_perf;
static struct{uint32_t underruns,overruns,missed,stalls,r48;}uac;
static struct{uint32_t config,diag_req,diag_sequence,diag_command;}usb;
static struct{uint32_t p3_rst,rst_src;}fm1_boot;
static struct{uint32_t failed,pending;}bootguard;
static struct{uint32_t magic,count,vec,pc,rets,emu,dbg,uptime_ms,early;}fm1_crash;
static uint32_t packets[64],sent;
static uint32_t irq_save(void){return 0;}
static void irq_restore(uint32_t f){(void)f;}
static void midi_out_event(uint32_t p){assert(sent<64);packets[sent++]=p;mo_w++;}
'''+body+r'''
static uint32_t reply_word(unsigned i){
 uint8_t b[IFW_BOOT_BYTES];unsigned n=0;
 for(unsigned j=0;j<sent;j++)for(unsigned k=0;k<((packets[j]&15)==4?3:(packets[j]&15)-4);k++)b[n++]=(uint8_t)(packets[j]>>(8+8*k));
 assert(n==IFW_BOOT_BYTES && b[6]==3 && b[7]==127 && b[8]==20 && b[n-1]==247);
 uint32_t v=0;for(unsigned k=0;k<5;k++)v|=(uint32_t)b[9+i*5+k]<<(7*k);return v;
}
int main(void){
 (void)perf_phase;
 ifw_boot_progress=(IFWBootProgress){0x42574649,40701,11};ifw_boot_begin();
 assert(ifw_previous_boot.version==40701 && ifw_previous_boot.stage==11);
 ifw_boot_mark(3);assert(ifw_boot_progress.stage==3 && ifw_boot_progress.version==40705);
 usb.config=1;usb.diag_req=1;usb.diag_sequence=127;usb.diag_command=2;
 recovery_active=1;ifw_boot_reason=1;fm1_boot.p3_rst=4;bootguard.failed=2;bootguard.pending=2;
 fm1_crash.magic=FM1_CRASH_MAGIC;fm1_crash.pc=0x02012344;fm1_crash.count=3;
 mo_w=29;diagnostics_service();assert(!sent && usb.diag_req); /* 37 packets need room */
 mo_r=2;diagnostics_service();assert(sent==37 && !usb.diag_req);
 assert(reply_word(1)==40705 && reply_word(2)==1 && reply_word(3)==1 && reply_word(4)==4);
 assert(reply_word(8)==1 && reply_word(11)==0x02012344 && reply_word(17)==3 && reply_word(18)==11 && reply_word(19)==40701);
 assert(bootguard.failed==2 && bootguard.pending==2 && recovery_active==1);
 sent=mo_w=mo_r=0;fm1_crash.magic=0;usb.diag_req=1;diagnostics_service();
 assert(reply_word(8)==0 && reply_word(11)==0); /* never expose random .noinit as a fault */
 puts("PASS production boot report: retained stages, rescue flags, reset/fault fields, bounded MIDI backpressure, read-only guard state");
}
'''
p=out/'boot_diagnostics.c';p.write_text(source)
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Ifirmware/src',str(p),'-o',str(out/'boot-diagnostics')],cwd=r,check=True)
subprocess.run([str(out/'boot-diagnostics')],check=True)
