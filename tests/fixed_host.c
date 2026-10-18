/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/src/control/mono.h"
#include "../firmware/src/fixed/envelope.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void mono_tests(void) {
    IFWMono m={0};
    assert(!ifw_mono_on(&m,60)); assert(ifw_mono_on(&m,67)); assert(ifw_mono_current(&m)==67);
    assert(ifw_mono_on(&m,60)); assert(m.count==2 && ifw_mono_current(&m)==60);
    ifw_mono_off(&m,60); assert(ifw_mono_current(&m)==67);
    ifw_mono_off(&m,60); assert(m.count==1);
    ifw_mono_clear(&m);
    for(unsigned i=0;i<128;i++) ifw_mono_on(&m,i);
    assert(m.count==128); ifw_mono_on(&m,64); assert(m.count==128 && ifw_mono_current(&m)==64);
    ifw_mono_on(&m,255); assert(m.count==128);
    for(unsigned i=0;i<128;i++) ifw_mono_off(&m,i);
    assert(!m.count && ifw_mono_current(&m)==-1);
}
int main(int argc,char **argv) {
    mono_tests(); if(argc==1) { puts("PASS integer mono: duplicates, return priority, all 128 keys"); return 0; }
    if(argc!=8) return 2;
    int a=atoi(argv[1]),d=atoi(argv[2]),sustain=atoi(argv[3]),r=atoi(argv[4]),retrigger=atoi(argv[5]),rate=atoi(argv[6]);
    unsigned frames=(unsigned)atoi(argv[7]);
    IFWFixedEnvelope e={0};
    ifw_fixed_env_configure(&e,a,d,(int32_t)((int64_t)sustain*INT32_MAX/1000),r,retrigger);
    for(unsigned i=0;i<frames;i++) {
        if(i==0) ifw_fixed_env_gate(&e,1,0);
        if(i==4410) ifw_fixed_env_gate(&e,1,1);
        if(i==11025) ifw_fixed_env_gate(&e,0,0);
        if(i==26460) ifw_fixed_env_gate(&e,1,0);
        if(i==39690) ifw_fixed_env_gate(&e,0,0);
        int32_t value=ifw_fixed_env_tick(&e,ifw_fixed_env_rate(rate));
        assert(value>=0);
        if(fwrite(&value,sizeof value,1,stdout)!=1) return 2;
    }
    return 0;
}
