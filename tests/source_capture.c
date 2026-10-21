/* SPDX-License-Identifier: GPL-3.0-only
 * Compare sound and all waveform CVs with 0.4.6-rc1 across unaligned edits.
 * Most waves begin disconnected; edits expose the previous FM-offset sample.
 */
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "presets.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
#ifdef IFW_CAPTURE_BASELINE
static void ifw_fixed_capture_sources(IFWFixedSynth *s) {(void)s;}
#endif
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static uint32_t rng,h[2];
static uint32_t random32(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void hash(uint32_t x) {
    h[0]=(h[0]^x)*16777619u;h[1]=(h[1]+x+0x9e3779b9u)*2246822519u;
    h[1]=(h[1]<<13)|(h[1]>>19);
}
int main(void) {
    for(unsigned c=0;c<8;c++) {
        IFWFixedPatch p;make_preset(&p,c%6);ifw_fixed_init(&synth,&p,&buffers);
        rng=0x4b53a627u+c;h[0]=2166136261u;h[1]=0x12345678u;
        ifw_fixed_note_on(&synth,48+c,INT32_MAX);ifw_fixed_breath(&synth,INT32_MAX);
        for(unsigned sample=0;sample<8192;sample++) {
            if(sample%37u==0) {
                for(unsigned i=0;i<4;i++) {
                    unsigned id=i*IFW_OSC_STRIDE;
                    p.v[id+P_Osc1Waveform2]=(int32_t)(random32()%4u);
                    p.v[id+P_Osc1PwmWidth]=(int32_t)(random32()%655361u);
                    p.v[id+P_Osc1Output1]=(int32_t)((sample/37+i)%3u==0);
                    p.v[id+P_Osc1Output2]=(int32_t)((sample/37+i)%5u==0);
                    p.v[id+P_Osc1SawLevel]=(sample/37+i)%3u?0:655360;
                    p.v[id+P_Osc1TriLevel]=(sample/37+i)%4u?0:655360;
                    p.v[id+P_Osc1PwmLevel]=(sample/37+i)%5u?0:655360;
                    p.v[id+P_Osc1Sync]=(int32_t)((sample/37+i)%7u==0);
                }
                for(unsigned i=0;i<8;i++) {
                    unsigned id=P_Mod1Source1+i*IFW_MOD_STRIDE;
                    p.v[id]=sample%111u?(int32_t)(IFW_SRC_SAW+random32()%12u)|128:0;
                    p.v[id+1]=i&1?IFW_SRC_EG+(int32_t)(i%4):0;
                    p.v[id+2]=i<4?IFW_MOD_FM+(int32_t)i:P_Osc1TriLevel+(int32_t)(i%4)*IFW_OSC_STRIDE;
                    p.v[id+3]=(int32_t)(random32()%131073u)-65536;
                }
                /* Both copy and in-place editing are used by real frontends. */
                if(sample&1u) {synth.patch=p;ifw_fixed_set_patch(&synth,&synth.patch);}
                else ifw_fixed_set_patch(&synth,&p);
            }
            if(sample%113u==0)ifw_fixed_breath(&synth,(int32_t)(random32()&INT32_MAX));
            if(sample%251u==0)ifw_fixed_note_on(&synth,36+sample%60,1700000000);
            if(sample%251u==89)ifw_fixed_note_off(&synth,36+(sample-89)%60);
            int32_t l,r;ifw_fixed_sample(&synth,&l,&r);hash((uint32_t)l);hash((uint32_t)r);
            if(sample%73u==0) {
                ifw_fixed_capture_sources(&synth);
                for(unsigned i=0;i<4;i++) {
                    hash((uint32_t)synth.raw_saw[i]);hash((uint32_t)synth.secondary[i]);
                    hash((uint32_t)synth.raw_pulse[i]);hash((uint32_t)synth.secondary_cv[i]);
                    hash(synth.phase[i]);hash((uint32_t)synth.env[i].level);
                }
                hash(synth.seed);hash(synth.master_phase);hash((uint32_t)synth.pitch);
            }
        }
        const unsigned char *b=(const unsigned char *)&buffers;
        for(unsigned i=0;i<sizeof buffers;i++)hash(b[i]);
        printf("%s%u,%u",c?",":"[",h[0],h[1]);
    }
    puts("]");return 0;
}
