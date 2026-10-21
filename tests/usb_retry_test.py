#!/usr/bin/env python3
"""Run the production USB startup/retry functions against a fake USB HAL."""
import os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'firmware/src/usb.c').read_text()
body=s[s.index('static void usb_attach_after_reset(void)'):s.index('static void usb_detach(void)')]
source=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static struct {uint32_t timeouts,retries,up,detached,sie_fault;} usb;
static uint8_t ep0buf[64];
static uint32_t now,resets,attaches,delays,writes,fail;
enum {S_POWER,S_INTRUSBE,S_INTRTX1E,S_INTRTX2E,S_INTRRX1E,S_INTRRX2E};
static void fm1_usb_reset(void){resets++;}
static void fm1_usb_attach(void *p){assert(p==ep0buf);attaches++;}
static void fm1_usb_ep_enable(uint32_t p){assert(p==31u);}
static void fm1_delay_ms(uint32_t n){assert(n==25u);delays++;now+=n;}
static void sie_wr(uint32_t reg,uint32_t value){(void)reg;(void)value;writes++;if(fail){usb.timeouts++;usb.sie_fault=1;}}
'''+body+r'''
int main(void){
    usb_start();assert(resets==1 && attaches==1 && delays==1 && usb.up);
    fail=1;usb.up=0;
    for(now=0;now<1025;now++)usb_retry(now);
    assert(resets==2 && attaches==1 && delays==1 && usb_retry_state.waiting);
    usb_retry(1025);assert(attaches==2 && !usb.up && !usb_retry_state.waiting);
    for(now=1026;now<2000;now++)usb_retry(now);
    assert(resets==2);usb_retry(2000);assert(resets==3 && attaches==2);
    fail=0;usb_retry(2024);assert(attaches==2);usb_retry(2025);assert(attaches==3 && usb.up);
    usb_retry(9000);assert(resets==3 && delays==1);
    usb.up=0;usb.detached=1;usb_retry(10000);assert(resets==3);
    usb.detached=0;memset(&usb_retry_state,0,sizeof usb_retry_state);
    usb_retry_state.last=UINT32_MAX-500u;
    usb_retry(499);assert(resets==4 && usb_retry_state.waiting);
    usb_retry(523);assert(attaches==3);usb_retry(524);assert(attaches==4 && usb.up);
    usb.up=0;usb_retry(2000);assert(resets==5);usb.detached=1;
    usb_retry(2025);assert(attaches==4 && !usb_retry_state.waiting && delays==1);
    puts("PASS USB retry: production reset/attach, 25 ms nonblocking wait, hotplug recovery, detached state and timer wrap");
    return 0;
}
'''
out=r/'build/target-audit';out.mkdir(parents=True,exist_ok=True)
(out/'usb_retry_test.c').write_text(source)
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-Wall','-Wextra','-Werror','-Ifirmware/src',str(out/'usb_retry_test.c'),'-o',str(out/'usb_retry_test')],cwd=r,check=True)
subprocess.run([str(out/'usb_retry_test')],check=True)
