/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include "../firmware/src/audio_transition.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static int32_t blend_reference(int32_t a,int32_t b,unsigned p) {
    int64_t sum=(int64_t)a*(64u-p)+(int64_t)b*p;
    return (int32_t)(sum<0?-((-sum+63)/64):sum/64);
}
int main(void) {
    const int32_t values[]={-32768,-32767,-30000,-1,0,1,30000,32767};
    unsigned checks=0;uint32_t seed=73921;
    for(unsigned a=0;a<8;a++)for(unsigned b=0;b<8;b++)for(unsigned p=0;p<=64;p++) {
        int32_t x=ifw_audio_blend(values[a],values[b],p);
        assert(x==blend_reference(values[a],values[b],p));checks++;
        assert(x>=(values[a]<values[b]?values[a]:values[b]));
        assert(x<=(values[a]>values[b]?values[a]:values[b]));
    }
    for(unsigned i=0;i<100000;i++) {
        seed=seed*1664525u+1013904223u;int32_t a=(int32_t)(seed&65535u)-32768;
        seed=seed*1664525u+1013904223u;int32_t b=(int32_t)(seed&65535u)-32768;
        assert(ifw_audio_blend(a,b,i%65)==blend_reference(a,b,i%65));checks++;
    }
    const unsigned partitions[]={1,7,31,32,64,256};
    unsigned max_step=0;
    for(unsigned k=0;k<6;k++) {
        IFWNoteTransition t={.last={30000,-30000},.armed=1};
        int32_t previous=30000;unsigned at=0;
        while(at<256) {
            unsigned n=partitions[k];if(n>256-at)n=256-at;
            int32_t block[512];for(unsigned i=0;i<n;i++){block[2*i]=-30000;block[2*i+1]=30000;}
            ifw_note_transition_apply(&t,block,n);
            for(unsigned i=0;i<n;i++,at++) {
                int32_t expected=at<64?blend_reference(30000,-30000,at):-30000;
                int32_t right=at<64?blend_reference(-30000,30000,at):30000;
                assert(block[2*i]==expected && block[2*i+1]==right);
                int32_t delta=expected-previous;unsigned step=(unsigned)(delta<0?-delta:delta);
                if(step>max_step)max_step=step;
                previous=expected;
            }
        }
        assert(!t.left && !t.armed && t.last[0]==-30000);
        /* A second event during the bridge starts at the last emitted sample. */
        t=(IFWNoteTransition){.last={12345,-23456},.armed=1};int32_t block[64]={0};
        ifw_note_transition_apply(&t,block,32);int32_t last[2]={t.last[0],t.last[1]};
        t.armed=1;ifw_note_transition_apply(&t,block,32);
        assert(block[0]==last[0] && block[1]==last[1]);
    }
    assert(max_step<=938); /* formerly a 60,000-LSB one-sample jump */
    printf("{\"arithmeticChecks\":%u,\"oldConstantStep\":60000,\"newMaxConstantStep\":%u,\"partitionCases\":6}\n",checks,max_step);
}
