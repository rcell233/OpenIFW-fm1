/* SPDX-License-Identifier: GPL-3.0-only
 * Independent arithmetic bounds, stereo link and real factory/dense-note
 * comparisons. These are digital tests, not physical DAC/CPU measurements.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "presets.h"
#include "output_bus.c"
static IFWFixedSynth synth;
static IFWFixedFxBuffers fx;
static uint32_t checks;
/* This sandbox has no /proc task enumeration. Address/UB checks remain on;
 * the DSP test itself has no heap allocations to leak. */
const char *__asan_default_options(void) {return "detect_leaks=0";}
static void arithmetic(void) {
    IFWOutputBus b;
    ifw_output_bus_init(&b,IFW_OUTPUT_NORMAL);
    const int32_t masters[]={0,1,1024,2048,4088,4096};
    for(unsigned m=0;m<sizeof masters/sizeof *masters;m++)
    for(int32_t v=-32768;v<32768;v++) {
        int32_t pcm[]={v,v};
        ifw_output_bus_process(&b,pcm,1,masters[m]);
        int64_t x=(int64_t)v*masters[m];
        int32_t expected=(int32_t)(x<0?-((-x+2047)/2048):x/2048);
        if(expected>32767)expected=32767;
        if(expected< -32767)expected= -32767;
        assert(pcm[0]==expected && pcm[1]==expected);checks++;
    }
    uint32_t previous=IFW_BUS_UNITY;
    for(uint32_t env=0;env<=131072u;env++) {
        uint32_t actual=bus_gain(env);
        uint32_t expected=env<=IFW_BUS_THRESHOLD?IFW_BUS_UNITY:IFW_BUS_THRESHOLD*IFW_BUS_UNITY/env;
        assert(actual<=previous && actual<=IFW_BUS_UNITY);
        assert((int32_t)actual-(int32_t)expected>=-2 && (int32_t)actual-(int32_t)expected<=1);
        previous=actual;checks++;
    }
    int32_t last=0;
    for(int32_t v=0;v<=131072;v++) {
        int32_t y=bus_soft(v,&b);
        assert(y>=last && y<=IFW_BUS_CEILING && bus_soft(-v,&b)==-y);
        if(v<=IFW_BUS_KNEE)assert(y==v);
        last=y;checks++;
    }
    /* Every input code, alternating extremes: loud path never integer-wraps.
     * Exactly linear below threshold, stereo signs share one gain. */
    for(unsigned m=0;m<sizeof masters/sizeof *masters;m++) {
        ifw_output_bus_init(&b,IFW_OUTPUT_LOUD);
        for(int32_t v=-32768;v<32768;v++) {
            int32_t pcm[]={v,v== -32768?32767:-v};
            ifw_output_bus_process(&b,pcm,1,masters[m]);
            assert(bus_abs(pcm[0])<=IFW_BUS_CEILING && bus_abs(pcm[1])<=IFW_BUS_CEILING);
            if(!masters[m])assert(!pcm[0] && !pcm[1]);
            if(masters[m]==4096 && v!=-32768)assert(pcm[0]==-pcm[1]);
            checks++;
        }
    }
    ifw_output_bus_init(&b,IFW_OUTPUT_LOUD);
    for(int32_t v=-6144;v<=6144;v++) {
        int32_t pcm[]={v,v/2};ifw_output_bus_process(&b,pcm,1,4096);
        assert(pcm[0]==v*4 && pcm[1]==(v/2)*4);checks++;
    }
    /* Strong left peaks reduce the quieter right by the SAME detector gain. */
    ifw_output_bus_init(&b,IFW_OUTPUT_LOUD);
    for(unsigned i=0;i<44100;i++) {
        int32_t pcm[]={i&1?16000:-16000,1000};
        ifw_output_bus_process(&b,pcm,1,4096);
        uint32_t gain=bus_gain(b.envelope);
        assert(pcm[1]==bus_scale(4000,gain));checks++;
    }
    /* Split-block processing must be exact, including explicit mode changes. */
    IFWOutputBus a,c;ifw_output_bus_init(&a,1);c=a;
    int32_t x[1024],y[1024];
    for(unsigned mode=0;mode<4;mode++) {
        for(unsigned i=0;i<1024;i++)x[i]=y[i]=(int32_t)((i*173u)%65536u)-32768;
        ifw_output_bus_mode(&a,mode&1);ifw_output_bus_mode(&c,mode&1);
        ifw_output_bus_process(&a,x,512,4088);
        for(unsigned i=0;i<512;i+=16)ifw_output_bus_process(&c,y+2*i,16,4088);
        assert(!memcmp(x,y,sizeof x) && !memcmp(&a,&c,sizeof a));checks++;
    }
}
static void scene(unsigned preset,unsigned note,unsigned velocity,unsigned div,unsigned dense) {
    IFWFixedPatch p;make_preset(&p,preset);ifw_fixed_init(&synth,&p,&fx);
    ifw_fixed_breath(&synth,INT32_MAX/(int32_t)div);
    ifw_fixed_note_on(&synth,note,(int32_t)((int64_t)INT32_MAX*velocity/127));
    IFWOutputBus bus;ifw_output_bus_init(&bus,IFW_OUTPUT_LOUD);
    double normal_energy=0,loud_energy=0;uint32_t peak=0,rawclips=0;
    unsigned seconds=dense?30u:4u,held=note;
    for(unsigned i=0;i<seconds*44100u;i++) {
        if(dense && i && i<20*44100u && !(i%1103u)) {
            unsigned next=note+(i/1103u)%13u;
            if(i&1){ifw_fixed_note_off(&synth,held);ifw_fixed_note_on(&synth,next,INT32_MAX);}
            else {ifw_fixed_note_on(&synth,next,INT32_MAX);ifw_fixed_note_off(&synth,held);}
            held=next;
        }
        if(dense && i==20*44100u)ifw_fixed_panic(&synth);
        int32_t pcm[2];ifw_fixed_process(&synth,pcm,1);
        for(unsigned ch=0;ch<2;ch++) {
            if(bus_abs(pcm[ch])>=32767u)rawclips++;
            double v=ifw_output_sample(pcm[ch],4096)/65536.;normal_energy+=v*v;
        }
        ifw_output_bus_process(&bus,pcm,1,4096);
        for(unsigned ch=0;ch<2;ch++) {
            uint32_t a=bus_abs(pcm[ch]);assert(a<=IFW_BUS_CEILING);if(a>peak)peak=a;
            double v=pcm[ch]/65536.;loud_energy+=v*v;
        }
    }
    assert(!rawclips);
    printf("{\"preset\":%u,\"name\":\"%s\",\"note\":%u,\"velocity\":%u,\"breathDiv\":%u,\"dense\":%u,"
           "\"seconds\":%u,\"normalDacRmsDb\":%.6f,\"loudDacRmsDb\":%.6f,\"gainDb\":%.6f,"
           "\"loudDacPeak\":%.9f,\"maxGainReductionDb\":%.6f,\"softChannelSamples\":%u,\"rawClips\":%u}",
           preset,preset_names[preset],note,velocity,div,dense,seconds,
           10*log10(normal_energy/(seconds*88200u)),10*log10(loud_energy/(seconds*88200u)),
           10*log10(loud_energy/normal_energy),peak/65536.,-20*log10(bus.min_gain/16384.),bus.soft_samples,rawclips);
}
int main(int argc,char **argv) {
    arithmetic();printf("{\"arithmeticChecks\":%u,\"scenes\":[",checks);
    if(argc>1 && !strcmp(argv[1],"--arithmetic-only")){puts("]}");return 0;}
    unsigned rows=0;const unsigned notes[]={36,48,60,69,72,84,96,108};
    for(unsigned p=0;p<FACTORY_COUNT;p++)for(unsigned n=0;n<8;n++)
    for(unsigned v=110;v<=127;v+=17)for(unsigned d=1;d<=4;d*=2) {
        if(rows++)printf(",\n");scene(p,notes[n],v,d,0);
    }
    for(unsigned p=0;p<FACTORY_COUNT;p++){printf(",\n");scene(p,60,127,1,1);}
    puts("]}");return 0;
}
