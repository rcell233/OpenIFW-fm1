/* SPDX-License-Identifier: GPL-3.0-only
 * Physical MIDI IN. UART1 DMA owns the byte ring. TIMER5 is the sole
 * publisher of completed DMA bytes; only the foreground parses/dispatches. */
#pragma once
#include <stdint.h>
enum { TRS_RING=512, TRS_BUDGET=32, TRS_GUARD=64 };
_Static_assert((TRS_RING & (TRS_RING-1))==0,"DMA ring must be a power of two");
_Static_assert(TRS_GUARD >= FM1_UART1_RX_FRAME+TRS_BUDGET,"reserve space for an unlatched DMA frame");
static volatile uint8_t trs_ring[TRS_RING] __attribute__((aligned(16)));
static volatile uint32_t trs_written;
static uint32_t trs_read;
static uint8_t trs_status,trs_first,trs_have_first;

static volatile uint32_t trs_tracing,trs_stamp[TRS_RING],trs_trace_startpos;
static uint32_t trs_lost;
static void trs_init(void) { fm1_uart1_midi_init(trs_ring,TRS_RING); }
static void trs_tick(void) {
    uint32_t count=fm1_uart1_rx_take();
    if(count){
        if(trs_tracing){
            uint32_t start=trs_written,now=trs_clock();
            if(count>TRS_BUDGET)trs_trace_startpos=start+count;
            else for(unsigned i=0;i<count;i++)trs_stamp[(start+i)&(TRS_RING-1u)]=now;
        }
        RING_PUBLISH();trs_written+=count;
    }
}

static void trs_message(uint8_t st,uint8_t a,uint8_t b);
static void panic_panel(void);
static void trs_byte(uint8_t b) {
    if(b>=0xf8)return; /* Clock and active sensing may appear between any two bytes. */
    if(b>=0x80){
        trs_status=b<0xf0?b:0; /* system common / SysEx cancel running status */
        trs_have_first=0;return;
    }
    if(!trs_status)return;
    if((trs_status&0xe0)==0xc0){trs_message(trs_status,b,0);return;}
    if(!trs_have_first){trs_first=b;trs_have_first=1;return;}
    trs_have_first=0;trs_message(trs_status,trs_first,b);
}

static void trs_service(void) {
    uint32_t end=trs_written, n=end-trs_read;
    RING_PUBLISH();
    if(n>TRS_RING-TRS_GUARD){
        /* The DMA may be up to a frame ahead of 'end'. Don't interpret
         * overwritten data as pitches or leave a lost note-off hanging. */
        trs_lost+=n;trs_read=end;trs_status=trs_have_first=0;panic_panel();return;
    }
    if(n>TRS_BUDGET)n=TRS_BUDGET;
    for(uint32_t i=0;i<n;i++)trs_byte(trs_ring[trs_read++ & (TRS_RING-1)]);
}
