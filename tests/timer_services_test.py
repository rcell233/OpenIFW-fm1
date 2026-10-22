#!/usr/bin/env python3
"""Run actual TIMER5 and SysEx dispatch against deterministic device services."""
import os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];out=r/'build/target-audit';out.mkdir(parents=True,exist_ok=True)
s=(r/'firmware/src/platform.c').read_text()
body=s[s.index('static volatile uint8_t in_audio;'):s.index('extern void isr_timer5(void);')]
usb=(r/'firmware/src/usb.c').read_text();sysex=usb[usb.index('static void sysex_byte(uint8_t b)'):usb.index('/* one USB-MIDI event packet:')]
source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "diagnostics_protocol.h"
#define FM1_TICKS_PER_US 24u
#define FELUCCA_OTA 1
#define RING_PUBLISH() do{}while(0)
static uint32_t ticks,fm1_ms,scans,polls,uac_calls;
static uint32_t fm1_ticks(void){return ticks;}
static void fm1_timer5_ack(void){}
static void fm1_input_tick(void){scans++;ticks+=72;}
static void usb_poll(void){polls++;ticks+=480;}
static void uac_service(void){uac_calls++;ticks+=168;}
static struct {uint8_t sx_on,sx_len,sysex[16],uboot_req,ota_req,diag_req,diag_sequence;} usb;
static uint8_t sx_collect,sx_ready,sx_frame[640];static uint32_t sx_pos,sx_frame_len;
'''+body+sysex+r'''
int main(void){
 in_audio=1;
 for(unsigned i=0;i<20;i++){ticks=i*2400u;fm1_timer5_irq();}
 assert(scans==20 && polls==0 && uac_calls==4);
 in_audio=0;ticks=48000u;fm1_timer5_irq();
 assert(polls==1 && platform_perf.usb_polls==1 && platform_perf.usb_ticks==480);
 assert(platform_perf.scan_max_ticks==552 && platform_perf.scans==21 && fm1_ms==2);
 const uint8_t request[]={0xf0,0x7d,'I','F','W',1,0,127,0xf7};
 for(unsigned i=0;i<9;i++){sysex_byte(request[i]);sysex_byte(0xf8);}
 assert(usb.diag_req && usb.diag_sequence==127 && !sx_ready && !usb.ota_req && !usb.uboot_req);
 usb.diag_req=0;
 const uint8_t loader[]={0xf0,0x22,0x24,0x35,0x7d,0xf7};
 for(unsigned i=0;i<6;i++)sysex_byte(loader[i]);
 assert(usb.uboot_req && !usb.diag_req);
 const uint8_t ota[]={0xf0,0x22,0x24,0x35,0x7f,0xf7};
 for(unsigned i=0;i<6;i++)sysex_byte(ota[i]);assert(usb.ota_req);
 for(unsigned i=0;i<8;i++)sysex_byte(request[i]);sysex_byte(0x90);sysex_byte(0xf7);
 assert(!usb.diag_req);
 puts("PASS production TIMER5: scan/UAC during DSP, deferred USB resumes, actual service timing; SysEx diagnostics/OTA/UBOOT isolation");
 return 0;
}
'''
p=out/'timer_services.c';p.write_text(source)
exe=out/'timer_services'
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Ifirmware/src',str(p),'-o',str(exe)],cwd=r,check=True)
subprocess.run([str(exe)],check=True)
