/* SPDX-License-Identifier: GPL-3.0-only
 * Volatile, explicit recording. No flash writes or USB transmission while playing.
 * dispatchUs, packed channel message, (next audio block wait : DMA notice wait).
 * 65535 = unavailable; 65534 = saturated. Times are measured on device. */
#pragma once
enum { TRACE_CAP=4096, TRACE_PAGE=8 };
typedef struct {uint32_t us,msg,wait;} TraceEvent;
static TraceEvent trace_buf[TRACE_CAP] __attribute__((section(".pool")));
static struct {
    uint32_t mode,session,n,mark,tick,ms,duration,patch,ch,cc,curve,raw,loss,flags,glide,bend,gate,patchHash,glideMode,glideMin,glideDepth,breathMs,glideMs;
} mt;
static uint32_t trace_clock(void){
#if defined(IFW_TARGET) || defined(IFW_TRS_TEST)
    return trs_clock();
#else
    return fm1_ms*24000u;
#endif
}
static void trace_stop(uint32_t mode){
    mt.duration=fm1_ms-mt.ms;mt.mode=mode;
#if defined(IFW_TARGET) || defined(IFW_TRS_TEST)
    trs_tracing=0;mt.loss=trs_lost-mt.loss;
#endif
}
static void trace_start(void){
    uint32_t id=mt.session+1u;memset(&mt,0,sizeof mt);mt.session=id;
    mt.ms=fm1_ms;mt.tick=trace_clock();mt.patch=preset;mt.cc=breath_cc;mt.ch=saved.channel;
    for(unsigned i=0;i<4;i++)mt.curve|=(uint32_t)breath_y[i]<<(8*i);
    mt.raw=(uint32_t)(last_breath_raw+1);mt.glide=(uint32_t)synth.glide;
    mt.bend=(uint32_t)synth.bend_target;mt.gate=synth.mono.count;
    mt.glideMode=edit_patch.v[P_GlideMode];mt.glideMin=edit_patch.v[P_GlideMinimumSpeed];mt.glideDepth=edit_patch.v[P_GlideSpeed];
    mt.breathMs=edit_patch.v[P_BreathSmoothTime];mt.glideMs=edit_patch.v[P_GlideSmoothTime];
    mt.flags=(uint32_t)edit_patch.v[P_GlideMidiEnabled];mt.patchHash=2166136261u;
    for(unsigned i=0;i<sizeof edit_patch;i++)mt.patchHash=(mt.patchHash^((uint8_t*)&edit_patch)[i])*16777619u;
#if defined(IFW_TARGET) || defined(IFW_TRS_TEST)
    mt.loss=trs_lost;trs_trace_startpos=trs_written;trs_tracing=1;
#endif
    mt.mode=1;
}
static void trace_note(unsigned src,uint8_t st,uint8_t a,uint8_t b){
    if(mt.mode!=1 || st<0x80 || st>=0xf0)return;
    if(fm1_ms-mt.ms>=120000u){trace_stop(4);return;}
    uint32_t now=trace_clock(),lag=65535;
#if defined(IFW_TARGET) || defined(IFW_TRS_TEST)
    if(src==1 && trs_read!=trs_trace_startpos && (int32_t)(trs_read-trs_trace_startpos)>0){
        uint32_t age=(now-trs_stamp[(trs_read-1u)&(TRS_RING-1u)])/24u;
        lag=age>65534?65534:age;
    }
#endif
    unsigned accepted=src==2 || !saved.channel || saved.channel==(st&15u)+1u;
    trace_buf[mt.n++]=(TraceEvent){(now-mt.tick)/24u,(uint32_t)st|((uint32_t)a<<8)|((uint32_t)b<<16)|(src<<24)|(accepted<<26),0xffff0000u|lag};
    if(mt.n==TRACE_CAP)trace_stop(3);
}
static void trace_audio(void){
    if(mt.mode!=1)return;
    if(fm1_ms-mt.ms>=120000u){trace_stop(4);return;}
    if(mt.n==mt.mark)return;
    uint32_t now=(trace_clock()-mt.tick)/24u;
    while(mt.mark<mt.n){TraceEvent *e=&trace_buf[mt.mark++];uint32_t age=now-e->us;
        e->wait=(e->wait&65535u)|((age>65534?65534:age)<<16);}
}
static unsigned trace_words(unsigned page,uint32_t *w) {
    w[0]=1;w[1]=mt.session;w[2]=page;w[3]=mt.mode;
    if(!page){w[4]=mt.n;w[5]=mt.duration;w[6]=mt.patch;w[7]=mt.ch;w[8]=mt.cc;w[9]=mt.curve;w[10]=mt.raw;
        w[11]=mt.loss;w[12]=mt.flags;w[13]=mt.glide;w[14]=mt.bend;w[15]=mt.gate;w[16]=mt.patchHash;w[17]=40717;w[18]=mt.glideMode;w[19]=mt.glideMin;w[20]=mt.glideDepth;w[21]=mt.breathMs;w[22]=mt.glideMs;return 23;}
    unsigned first=(page-1)*TRACE_PAGE,end=first+TRACE_PAGE;
    if(mt.mode<2 || first>=mt.n)return 4;
    if(end>mt.n)end=mt.n;
    unsigned n=4;for(unsigned i=first;i<end;i++){w[n++]=trace_buf[i].us;w[n++]=trace_buf[i].msg;w[n++]=trace_buf[i].wait;}return n;
}
