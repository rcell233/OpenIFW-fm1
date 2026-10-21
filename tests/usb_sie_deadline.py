#!/usr/bin/env python3
"""Fault-inject the actual USB SIE accessors used by the high-priority ISR."""
import os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'firmware/src/usb.c').read_text()
a=s.index('static int sie_wait(void)')
b=s.index('\n}',s.index('static uint32_t sie_rd(uint32_t r)',a))+2
source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static struct {uint32_t timeouts,sie_fault,up;} usb;
static uint32_t ticks,started,finish_at,requests,clock_on=1;
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void){return ticks++;}
static uint32_t fm1_usb_sie_done(void){return ticks-started>=finish_at;}
static uint32_t fm1_usb_sie_on(void){return clock_on;}
static void fm1_usb_sie_wr_start(uint32_t r,uint32_t v){(void)r;(void)v;started=ticks;requests++;}
static void fm1_usb_sie_rd_start(uint32_t r){(void)r;started=ticks;requests++;}
static uint32_t fm1_usb_sie_data(void){return 0x5au;}
'''+s[a:b]+r'''
int main(void){
    usb.up=1;finish_at=24;assert(sie_rd(1)==0x5a && usb.up && !usb.timeouts);
    finish_at=UINT32_MAX;uint32_t at=ticks;
    assert(sie_rd(1)==0 && !usb.up && usb.sie_fault && usb.timeouts==1);
    assert(ticks-at<=20u*24u+2u);
    at=ticks;uint32_t n=requests;
    for(uint32_t i=0;i<100;i++){sie_wr(1,2);assert(sie_rd(1)==0);}
    assert(ticks==at && requests==n); /* same IRQ never spins on more dead transactions */
    usb.sie_fault=0;usb.up=1;finish_at=0;
    sie_wr(1,2);assert(sie_rd(1)==0x5a && usb.up); /* retry can recover */
    clock_on=0;at=ticks;n=requests;
    sie_wr(1,2);assert(!sie_rd(1));assert(ticks==at && requests==n);
    clock_on=1;usb.sie_fault=0;usb.up=1;ticks=UINT32_MAX-100u;finish_at=UINT32_MAX;
    at=ticks;(void)sie_rd(1);assert(ticks-at<=482u && usb.sie_fault && !usb.up);
    puts("PASS USB SIE: real accessors stop within 20 us on missing clock, suppress further polling, recover and handle timer wrap");
}
'''
out=r/'build/target-audit';out.mkdir(parents=True,exist_ok=True)
(out/'usb_sie_deadline.c').write_text(source)
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-Wall','-Wextra','-Werror',str(out/'usb_sie_deadline.c'),'-o',str(out/'usb_sie_deadline')],cwd=r,check=True)
subprocess.run([str(out/'usb_sie_deadline')],check=True)
