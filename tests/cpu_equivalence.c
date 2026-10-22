/* SPDX-License-Identifier: GPL-3.0-only
 * Exact production regression captured from rc5 before the CPU changes.
 * Includes dense notes, all filter3 lengths, synced/free LFO edits and live
 * envelope/LFO rate routing. The complete pre-existing synth state and all FX
 * delay memory enter the trace. Newly appended pure caches are excluded.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "presets.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static void hash_bytes(uint32_t h[2],const void *data,size_t n) {
    const uint8_t *p=data;
    for(size_t i=0;i<n;i++) {
        h[0]=(h[0]^p[i])*16777619u;
        h[1]=(h[1]+p[i]+0x9e3779b9u)*2246822519u;
    }
}
static void trace_state(uint32_t h[2]) {
    IFWFixedSynth copy=synth;copy.effects.buffers=NULL;
    /* End of the rc5 state, before new memoization keys and tail padding. */
    hash_bytes(h,&copy,offsetof(IFWFixedSynth,noise_period)+sizeof copy.noise_period);
}
int main(void) {
    uint64_t frames=0;puts("{\"kind\":\"rc5-production-pcm-state-fx-trace\",\"cases\":[");
    for(unsigned preset=0;preset<6;preset++)for(unsigned mode=0;mode<4;mode++) {
        IFWFixedPatch patch;make_preset(&patch,preset);ifw_fixed_init(&synth,&patch,&buffers);
        uint32_t h[2]={2166136261u,0x12345678u};unsigned old=60;
        ifw_fixed_breath(&synth,INT32_MAX);ifw_fixed_note_on(&synth,old,1700000000);
        for(unsigned block=0;block<768;block++) {
            if(mode==1 && block%8==7)ifw_fixed_note_off(&synth,old);
            if(block && block%8==0) {
                unsigned note=mode==2?60u:48u+(block/8*7u)%37u;
                ifw_fixed_note_on(&synth,note,900000000+(int32_t)(block%107)*11000000);
                if(mode!=1 && note!=old)ifw_fixed_note_off(&synth,old);
                old=note;
            }
            if(mode==3 && block%13==0) {
                unsigned edit=block/13;
                patch.v[P_Filter3Stage]=(int32_t)(2+edit%23)*65536;
                for(unsigned i=0;i<2;i++) {
                    unsigned id=P_Lfo1Speed+i*IFW_LFO_STRIDE;
                    patch.v[id]=(int32_t)(edit%8)*65536;
                    patch.v[P_Lfo1BPMSync+i*IFW_LFO_STRIDE]=(int32_t)(edit%3==0);
                    patch.v[P_Lfo1BpmSync+i*IFW_LFO_STRIDE]=(int32_t)(edit%3==1);
                    patch.v[P_Lfo1Beat+i*IFW_LFO_STRIDE]=edit%2?65536:(int32_t)(1024+edit*71);
                }
                for(unsigned i=0;i<6;i++) {
                    unsigned id=P_Mod1Source1+i*IFW_MOD_STRIDE;
                    patch.v[id]=i%2?IFW_SRC_EG+(int32_t)(i%4):IFW_SRC_LFO+(int32_t)(i%2);
                    patch.v[id+1]=IFW_SRC_NONE;
                    patch.v[id+2]=(int32_t)(i<4?IFW_MOD_EG+i:IFW_MOD_LFO+i-4);
                    patch.v[id+3]=(int32_t)(edit%17)*8192-65536;
                }
                ifw_fixed_set_patch(&synth,&patch);
                if(block%39==0)ifw_fixed_set_patch(&synth,&synth.patch);
            }
            if(block%19==0)ifw_fixed_breath(&synth,(int32_t)(block*2394473u)&INT32_MAX);
            synth.bend_target=(int32_t)(block%9)*65536-262144;
            int32_t pcm[256];ifw_fixed_process(&synth,pcm,128);frames+=128;
            hash_bytes(h,pcm,sizeof pcm);trace_state(h);
            if(block%47==0 || block==767)hash_bytes(h,&buffers,sizeof buffers);
        }
        printf("%s{\"preset\":%u,\"mode\":%u,\"hashes\":[%u,%u]}",
               preset||mode?",":"",preset,mode,h[0],h[1]);
    }
    printf("],\"frames\":%llu}\n",(unsigned long long)frames);return 0;
}
