/* SPDX-License-Identifier: GPL-3.0-only
 * Run unchanged against rc13 (--report) and the target's production DSP.
 * PCM counts are differences between message orderings, not counts of pops. */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "presets.h"
static IFWFixedSynth s;
static IFWFixedFxBuffers fx;
enum { N = 8192 };
static int32_t a[N*2], b[N*2], tmp[512];
static void start(unsigned p) {
    IFWFixedPatch patch;make_preset(&patch,p);ifw_fixed_init(&s,&patch,&fx);
    ifw_fixed_breath(&s,1503238553);ifw_fixed_note_on(&s,60,1610612735);
}
static uint32_t advance(unsigned n) {
    uint32_t hash=2166136261u;
    while(n){unsigned k=n>256?256:n;ifw_fixed_process(&s,tmp,k);
        for(unsigned i=0;i<2*k;i++)hash=(hash^(uint32_t)tmp[i])*16777619u;
        n-=k;}
    return hash;
}
static uint32_t handoff(unsigned p,unsigned pre,int reverse,int32_t *out){
    start(p);uint32_t hash=advance(pre);
    if(reverse){ifw_fixed_note_on(&s,67,1288490188);ifw_fixed_note_off(&s,60);}
    else{ifw_fixed_note_off(&s,60);ifw_fixed_process(&s,NULL,0);ifw_fixed_note_on(&s,67,1288490188);}
    ifw_fixed_process(&s,out,N);return hash;
}
int main(int argc,char **argv) {
    int report=argc==2 && !strcmp(argv[1],"--report");
    for(unsigned p=0;p<FACTORY_COUNT;p++){
        unsigned diffs=0;uint32_t hold=0;
        for(unsigned i=0;i<3;i++){unsigned pre=2*44100-1+i;
            uint32_t h=handoff(p,pre,0,a);assert(handoff(p,pre,1,b)==h);
            if(i==1)hold=h;
            for(unsigned n=0;n<N*2;n++)diffs+=a[n]!=b[n];
        }
        printf("%u %-20s order_diff=%5u/49152 hold_2s=%08" PRIx32 "\n",p,preset_names[p],diffs,hold);
        if(report)continue;
        assert(diffs==0);
        start(p);advance(4800);
        /* Exercise every envelope in both modes, including presets that
         * ordinarily neither route nor retrigger that envelope. */
        for(unsigned mask=0;mask<2;mask++){
            for(unsigned i=0;i<4;i++)s.env[i].retrigger=(i+mask)&1;
            IFWFixedEnvelope old[4];memcpy(old,s.env,sizeof old);
            unsigned current=mask?67:60,next=mask?60:67;
            ifw_fixed_note_off(&s,current);ifw_fixed_process(&s,NULL,0);ifw_fixed_note_on(&s,next,INT32_MAX);
            for(unsigned i=0;i<4;i++){if(old[i].retrigger){assert(s.env[i].level==0 && s.env[i].stage==1);}
                else assert(memcmp(&s.env[i],&old[i],sizeof old[i])==0);}
            advance(32);
        }
        ifw_fixed_note_on(&s,72,INT32_MAX);advance(32);
        uint32_t elapsed=s.glide_elapsed;int32_t from=s.glide_from,target=s.pitch_target;
        ifw_fixed_note_off(&s,10);
        assert(s.mono.count==2 && s.glide_elapsed==elapsed && s.glide_from==from && s.pitch_target==target);
        ifw_fixed_note_off(&s,72);assert(s.note==60);
        ifw_fixed_note_off(&s,60);advance(1);assert(s.gate_target==0 && s.env[0].stage==4);
        ifw_fixed_note_on(&s,67,INT32_MAX);assert(s.env[0].level==0 && s.env[0].stage==1);
        advance(128);ifw_fixed_note_off(&s,67);ifw_fixed_panic(&s);advance(1);
        ifw_fixed_note_on(&s,69,INT32_MAX);assert(s.gate_target && s.env[0].level==0 && s.env[0].stage==1);
    }
    return 0;
}
