/* SPDX-License-Identifier: GPL-3.0-only
 * Golden-output regression: live routing, every waveform CV, FM, FX edits and
 * preserved delay/EQ history. Also runs inside the pi32v2 test firmware.
 */
#ifdef IFW_TARGET
#include "memory.h"
#else
#include <string.h>
#endif
#include "synth.h"
#ifndef IFW_DSP_SELFTEST
#include "presets.h"
#endif
static uint32_t opt_rng;
#ifdef IFW_TARGET
__attribute__((section(".text.ifw_target_test")))
#endif
static uint32_t opt_random(void) {
    opt_rng^=opt_rng<<13;opt_rng^=opt_rng>>17;opt_rng^=opt_rng<<5;return opt_rng;
}
#ifdef IFW_TARGET
__attribute__((section(".text.ifw_target_test")))
#endif
static void opt_hash(uint32_t h[2],uint32_t x) {
    h[0]=(h[0]^x)*16777619u;h[1]=(h[1]+x+0x9e3779b9u)*2246822519u;
    h[1]=(h[1]<<13)|(h[1]>>19);
}
#ifdef IFW_TARGET
__attribute__((noinline,section(".text.ifw_target_test")))
#endif
static void opt_regression(IFWFixedSynth *s,IFWFixedFxBuffers *b,uint32_t result[16]) {
    /* Keep a full-address call in target test builds as well. */
#ifdef IFW_TARGET
    void (*volatile load_preset)(IFWFixedPatch *,uint32_t)=make_preset;
#else
    void (*load_preset)(IFWFixedPatch *,uint32_t)=make_preset;
#endif
    for(unsigned c=0;c<8;c++) {
        IFWFixedPatch p;load_preset(&p,c%6);ifw_fixed_init(s,&p,b);s->dry=(uint8_t)(c&1);
        uint32_t h[2]={2166136261u,0x12345678u};opt_rng=0x98765432u+c;
        ifw_fixed_breath(s,INT32_MAX);ifw_fixed_note_on(s,48+c*3,1800000000);
        for(unsigned block=0;block<128;block++) {
            if(block%23==0)load_preset(&p,(block/23+c)%6);
            for(unsigned i=0;i<4;i++) {
                unsigned id=i*IFW_OSC_STRIDE;
                p.v[id+P_Osc1Waveform2]=(int32_t)(opt_random()%4);
                p.v[id+P_Osc1PwmWidth]=(int32_t)(opt_random()%650000u);
                p.v[id+P_Osc1SawLevel]=block%3?(int32_t)(opt_random()%655361u):0;
                p.v[id+P_Osc1TriLevel]=block%4?(int32_t)(opt_random()%655361u):0;
                p.v[id+P_Osc1PwmLevel]=block%5?(int32_t)(opt_random()%655361u):0;
                p.v[id+P_Osc1Output1]=(int32_t)((block+i)&1);
                p.v[id+P_Osc1Output2]=(int32_t)((block+i)%3==0);
                p.v[id+P_Osc1Sync]=(int32_t)((block+i)%5==0);
            }
            for(unsigned i=0;i<8;i++) {
                unsigned id=P_Mod1Source1+i*4;
                p.v[id]=(int32_t)(IFW_SRC_SAW+(block+i)%12)|((block&1)?128:0);
                p.v[id+1]=block%3?IFW_SRC_EG+(int32_t)(i%4):IFW_SRC_LFO+(int32_t)(i%2);
                p.v[id+2]=(int32_t)(IFW_MOD_FM+i%4);
                if(block%7==0)p.v[id+2]=P_Filter1Frequency;
                if(block%11==0)p.v[id]=0;
                p.v[id+3]=(int32_t)(opt_random()%131073u)-65536;
            }
            p.v[P_EqLowGain]=block%4?(int32_t)(opt_random()%1572865u)-786432:0;
            p.v[P_EqHighGain]=block%3?(int32_t)(opt_random()%1572865u)-786432:0;
            p.v[P_EnhancerDelayL]=(int32_t)(opt_random()%6400000u);
            p.v[P_EnhancerDelayR]=(int32_t)(opt_random()%6400000u);
            p.tuning=(432+(int32_t)(block%19))*65536;p.chorus_phase=(int32_t)(opt_random()%400000u);
            p.fx_bypass=block%5==0?31u:0u;
            for(unsigned k=0;k<5;k++)p.fx[k][IFW_FX_Bypass]=(int32_t)((block+k)%13==0);
            p.fx[0][IFW_FX_Speed]=(int32_t)(opt_random()%200000u);
            p.fx[0][IFW_FX_Feedback]=(int32_t)(opt_random()%500000u);
            p.fx[0][IFW_FX_Stereo]=(int32_t)(opt_random()%655361u);
            p.fx[1][IFW_FX_Depth]=(int32_t)(opt_random()%655361u);
            p.fx[1][IFW_FX_Rate]=(int32_t)(opt_random()%655361u);
            p.fx[1][IFW_FX_Mix]=(int32_t)(opt_random()%655361u);
            for(unsigned k=2;k<4;k++) {
                p.fx[k][IFW_FX_Time]=(int32_t)(66+opt_random()%65470u);
                p.fx[k][IFW_FX_Depth]=(int32_t)(opt_random()%600000u);
                p.fx[k][IFW_FX_WetDry]=(int32_t)(opt_random()%655361u);
            }
            p.fx[4][IFW_FX_Size]=(int32_t)(opt_random()%655361u);
            p.fx[4][IFW_FX_HighDamp]=(int32_t)(opt_random()%655361u);
            p.fx[4][IFW_FX_WetDry]=(int32_t)(opt_random()%655361u);
            p.fx[4][IFW_FX_Width]=(int32_t)(opt_random()%655361u);
            ifw_fixed_set_patch(s,&p);
            if(block%17==0)ifw_fixed_note_on(s,48+block%36,1200000000+(int32_t)(opt_random()%900000000u));
            if(block%17==5)ifw_fixed_note_off(s,48+(block-5)%36);
            if(block%19==0)ifw_fixed_breath(s,(int32_t)(opt_random()&INT32_MAX));
            if(block==91)ifw_fixed_panic(s);
            for(unsigned i=0;i<32;i++) {
                int32_t l,r;ifw_fixed_sample(s,&l,&r);opt_hash(h,(uint32_t)l);opt_hash(h,(uint32_t)r);
            }
            opt_hash(h,(uint32_t)s->pitch);opt_hash(h,s->master_phase);opt_hash(h,s->seed);
            for(unsigned i=0;i<4;i++) {opt_hash(h,s->phase[i]);opt_hash(h,(uint32_t)s->env[i].level);}
        }
        result[c*2]=h[0];result[c*2+1]=h[1];
    }
}
#ifdef IFW_SELFTEST_MAIN
#include <stdio.h>
#include "dsp_math.c"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
static IFWFixedSynth test_synth;
static IFWFixedFxBuffers test_buffers;
int main(void) {
    if(opt_math_checks())return 1;
    uint32_t h[16];opt_regression(&test_synth,&test_buffers,h);
    for(unsigned i=0;i<16;i++)printf("%s%u",i?",":"[",h[i]);puts("]");return 0;
}
#endif
