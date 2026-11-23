/* SPDX-License-Identifier: GPL-3.0-only
 * Exercise the real UART pending/count sequence, timer publisher, app input
 * pass and production fixed DSP. Only registers and DMA writes are modeled. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
static uint32_t con, phase, latched, producer;
static void con_write(uint32_t v) {
    if(!phase){assert(v&0x80);phase=1;}
    else {assert(phase==2 && (v&0x1400)==0x1400);phase=3;con=0;}
}
static uint32_t read_count(void) {assert(phase==3);phase=0;return latched;}
#define FM1_UART1_CON_READ() con
#define FM1_UART1_CON_WRITE(v) con_write(v)
#define FM1_UART1_SYNC() do{assert(phase==1);phase=2;}while(0)
#define FM1_UART1_COUNT_READ() read_count()
#include "../firmware/hal/fm1_uart.h"
static volatile uint8_t *dma;
static uint32_t length;
static void mock_init(volatile uint8_t *ring,uint32_t len){assert(((uintptr_t)ring&15)==0);dma=ring;length=len;}
#define fm1_uart1_midi_init mock_init
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static uint32_t test_time;static uint32_t trs_clock(void){return test_time;}
#include "../firmware/src/trs_midi.h"
#define IFW_HOST
#define IFW_TRS_TEST
#define main panel_simulator_main
#include "../firmware/src/app.c"
#undef main
static void wire(const uint8_t *p,unsigned n) {
    for(unsigned i=0;i<n;i++)dma[producer++ & (length-1)]=p[i];
    con=n<FM1_UART1_RX_FRAME?0x800:0x4000;latched=n;trs_tick();
    uint32_t once=trs_written;trs_tick();assert(trs_written==once); /* HRXCNT retains last value when idle */
}
#define WIRE(...) do{const uint8_t m[]={__VA_ARGS__};wire(m,sizeof m);}while(0)
static int render(void){
    int32_t out[512];int audible=0;app_step();app_audio(out,256);
    for(unsigned i=0;i<512;i++)audible|=out[i]!=0;
    return audible;
}
static void usb_input(uint8_t st,uint8_t a,uint8_t b) {
    uint8_t *p=mq[mq_w++%64];p[0]=st;p[1]=a;p[2]=b;
}
static void performance_event(int trs,uint8_t st,uint8_t a,uint8_t b) {
    static uint8_t previous;
    if(!trs){usb_input(st,a,b);return;}
    if(previous!=st){WIRE(st);previous=st;}
    WIRE(a,0xfe);if((st&0xe0)!=0xc0)WIRE(b);
}
static int pcm_compare(int trs,const char *path) {
    FILE *f=fopen(path,"wb");assert(f);app_init();trs_init();
    for(unsigned p=0;p<FACTORY_COUNT;p++){
        performance_event(trs,0xc1,preset_programs[p],0);
        for(unsigned i=0;i<120;i++) {
            int32_t out[512];performance_event(trs,0xb1,2,(i*3)%128);
            if(i==1)performance_event(trs,0x91,60,110);
            if(i==40){performance_event(trs,0x81,60,0);performance_event(trs,0x91,67,100);}
            if(i==100)performance_event(trs,0x81,67,0);
            fm1_ms+=6;app_step();app_audio(out,256);
            assert(last_breath_raw==(int)((i*3)%128) && synth.mono.count==(i>=1 && i<100));
            assert(fwrite(out,sizeof out,1,f)==1);
        }
    }
    assert(!fclose(f));return 0;
}
/* Compare panel vs TRS: do not feed breath or modulation; match the
 * panel's pitch, velocity 110, and the precise render boundaries. */
static int panel_parity(int panel,const char *path){
    FILE *f=fopen(path,"wb");assert(f);app_init();trs_init();
    for(unsigned p=0;p<FACTORY_COUNT;p++){
        load_preset(p);
        for(unsigned i=0;i<150;i++){
            if(panel){
                if(i==2)fm1_in.notes=1u<<7;
                if(i==50)fm1_in.notes=(1u<<7)|(1u<<14);
                if(i==54)fm1_in.notes=1u<<14;
                if(i==100)fm1_in.notes=0;
            }else{
                if(i==2){WIRE(0x90,60,110);}if(i==50){WIRE(0x90,67,110);}
                if(i==54){WIRE(0x80,60,0);}if(i==100){WIRE(0x80,67,0);}
            }
            int32_t out[512];fm1_ms+=6;app_step();app_audio(out,256);
            assert(fwrite(&synth.pitch,sizeof synth.pitch,1,f)==1);
            assert(fwrite(out,sizeof out,1,f)==1);
        }
    }
    assert(!fclose(f));return 0;
}
static unsigned glide_frames(unsigned cc){
    /* Same EWI legato jump after CC5 has settled, no audio/DMA wall-time estimate. */
    IFWFixedPatch p;make_preset(&p,0);ifw_fixed_init(&synth,&p,&fx_buffers);
    qread=qwrite=overflow=0;ready=1;saved.channel=0;
    WIRE(0xb0,5,cc,0x90,60,110);app_step();app_audio_begin();
    int32_t l,r;for(unsigned i=0;i<22050;i++)ifw_fixed_sample(&synth,&l,&r);
    WIRE(0x90,72,110);app_step();app_audio_begin();
    for(unsigned i=1;i<=44100;i++){ifw_fixed_sample(&synth,&l,&r);
      if(synth.pitch>=72*65536-655)return i;} /* within 0.01 semitone */
    return 44100;
}
static void trace_test(void){
    uint32_t w[28];trace_start();assert(trs_tracing);
    test_time=2400;WIRE(0xb3,2,51);test_time+=48;app_step();
    test_time+=240;app_audio_begin();assert(mt.n==1 && trace_buf[0].us==102);
    assert(trace_buf[0].wait==(10u<<16|2u));
    assert((trace_buf[0].msg>>24)==5);
    WIRE(0x92,64,120);render();assert(trace_buf[1].msg>>24==1); /* ignored CH3 */
    usb_input(0xb3,5,77);render();fm1_ms=12;trace_stop(2);
    assert(trace_words(0,w)==23 && w[4]==3 && w[5]==12 && w[8]==2 && w[3]==2);
    assert(trace_words(1,w)==13 && w[2]==1 && w[4]==102);
    assert(trace_words(2,w)==4);
    trace_start();for(unsigned i=0;i<TRACE_CAP+1;i++)trace_note(0,0x90,69,100);
    assert(mt.mode==3 && mt.n==TRACE_CAP && !trs_tracing);
    trace_start();fm1_ms+=120001;trace_audio();assert(mt.mode==4 && mt.n==0);
}
#include "../firmware/src/diagnostics_protocol.h"
static int fixture(const char *path){
    FILE *f=fopen(path,"wb");assert(f);app_init();trs_init();edit_value(230,4,0);trace_start();
    test_time=2960*24;WIRE(0xb3,2,41);test_time=3000*24;app_step();test_time+=300*24;app_audio_begin();
    for(unsigned i=1;i<18;i++){
        fm1_ms=4+i;test_time=fm1_ms*24000;
        if(i==1){WIRE(0x92,60,100);}else if(i==2){fm1_in.notes=1u<<7;}
        else if(i==3){usb_input(0xb3,5,100);}else {WIRE(0xc3,18);}
        render();
    }
    trace_stop(2);assert(mt.n==18);
    for(unsigned pg=0;pg<=3;pg++){uint8_t req[]={240,125,73,70,87,1,4,pg,pg,0,247};
        assert(ifw_trace_request(req,11));assert(!ifw_trace_request(req,10));
        uint32_t words[28];unsigned n=trace_words(pg,words);uint8_t reply[150];
        ifw_diag_encode_words(reply,pg,5,n,words);
        assert(fwrite(reply,1,10+n*5,f)==10+n*5);
    }
    assert(!fclose(f));return 0;
}
int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"fixture"))return fixture(argv[2]);
    if(argc==3 && (!strcmp(argv[1],"panel")||!strcmp(argv[1],"wirekeys")))return panel_parity(!strcmp(argv[1],"panel"),argv[2]);
    if(argc==3)return pcm_compare(!strcmp(argv[1],"trs"),argv[2]);
    app_init();trs_init();assert(trs_read==0 && length==512);
    /* Select CH4 and CC2. Split a note at each possible boundary, with
     * interleaved real-time/active sensing and zero-velocity note-off. */
    edit_value(230,4,0);assert(breath_cc==2);
    WIRE(0xb3,2);render();assert(last_breath_raw==-1);
    WIRE(0xfe,93,0x93,60);render();assert(last_breath_raw==93 && !synth.mono.count);
    WIRE(0xf8,101);int sound=render();sound|=render();assert(sound && synth.note==60 && synth.mono.count==1);
    WIRE(65,0xfe,106,60,0);render();assert(synth.note==65 && synth.mono.count==1);
    WIRE(0xb3,64,127,0x83,65,0,0xb3,2,0);render();
    assert(synth.mono.count==1 && last_breath_raw==0 && synth.breath_target==0);
    WIRE(64,0);render();assert(!synth.mono.count);
    WIRE(0xb2,2,100,0x92,71,110);render();assert(!synth.mono.count && last_breath_raw==0);
    WIRE(0xc3,18,4,0xe3,0,96);render();assert(preset==0 && synth.bend_target>0);
    WIRE(0xb3,2,22,0xf1,1,2,90,0xf0,0x22,0x24,0x35,0x7d,0xf8,0xf7,2,127);
    render();assert(last_breath_raw==22 && !synth.mono.count);
    /* A UART message and USB input share the playing state, but their parsers
     * are independent: a USB message cannot replace TRS running status. */
    WIRE(0xb3,2,80);render();usb_input(0x93,69,110);WIRE(2,78);render();
    assert(synth.mono.count==1 && synth.note==69 && last_breath_raw==78);
    /* Thousands of bytes wrap the ring, without losing any CC value. */
    for(unsigned i=0;i<2048;i++){WIRE(2,i&127);render();assert(last_breath_raw==(int)(i&127));}
    uint8_t burst[96];for(unsigned i=0;i<sizeof burst;i++)burst[i]=i%2?i/2:2;
    wire(burst,sizeof burst);uint32_t before=trs_read;render();assert(trs_read-before==32 && last_breath_raw==15);
    render();render();assert(last_breath_raw==47);
    /* Heavy loss discards partial state/held notes. Bare data can never
     * resurrect an old note. A new status resynchronizes normally. */
    uint8_t lost[TRS_RING+9];memset(lost,99,sizeof lost);wire(lost,sizeof lost);render();
    assert(!synth.mono.count);WIRE(69,100);render();assert(!synth.mono.count);
    WIRE(0x93,70,100);render();assert(synth.note==70 && synth.mono.count==1);
    trs_read=trs_written=producer=UINT32_MAX-1;WIRE(0x83,70,0);render();assert(trs_read==1 && !synth.mono.count);
    trace_test();
    unsigned fast=glide_frames(0),slow=glide_frames(127);assert(slow>fast*10);
    printf("Tresure Island C4-C5, to within 0.01 semitone: CC5=0 %u frames, CC5=127 %u frames at 44100 Hz\n",fast,slow);
    puts("PASS TRS DMA -> real app -> PCM: CC, notes/legato, channel, sustain, running status, real-time, USB coexistence, wrap, budget and recovery");
}
