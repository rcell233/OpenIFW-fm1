/* SPDX-License-Identifier: GPL-3.0-only
 * Output arithmetic boundaries and long factory playback through the real DSP.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "presets.h"
#include "output_level.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static int32_t expected(int32_t pcm,int32_t master) {
    int64_t product=(int64_t)pcm*master*2;
    int64_t v=product<0?-((-product+4095)/4096):product/4096;
    return v>32767?32767:v< -32767?-32767:(int32_t)v;
}
int main(int argc,char **argv) {
    (void)argv;
    _Static_assert(IFW_OUTPUT_GAIN==2,"Test must exercise the production output calibration");
    const int32_t masters[]={0,1,2,256,1024,2304,4088,4096};
    uint32_t checks=0;
    for(unsigned k=0;k<sizeof masters/sizeof masters[0];k++) {
        int32_t previous=-32768;
        for(int32_t pcm=-32768;pcm<=32767;pcm++) {
            int32_t out=ifw_output_sample(pcm,masters[k]);
            assert(out==expected(pcm,masters[k]));
            assert(out>=previous && out>=-32767 && out<=32767);
            previous=out;checks++;
            if(!masters[k])assert(!out);
        }
    }
    /* Low MASTER must preserve samples that would reach the output boundary
     * at full volume: calibration and MASTER precede the shared clamp. */
    assert(ifw_output_sample(30000,1024)==15000);
    assert(ifw_output_sample(-30000,1024)==-15000);
    printf("{\"kind\":\"native-arithmetic-not-hardware\",\"arithmeticChecks\":%u,\"longScenes\":[",checks);
    if(argc>1){puts("]}");return 0;}
    const unsigned notes[]={36,48,60,69,72,84,96,108};
    for(unsigned p=0;p<FACTORY_COUNT;p++) {
        IFWFixedPatch patch;make_preset(&patch,p);
        ifw_fixed_init(&synth,&patch,&buffers);
        ifw_fixed_breath(&synth,INT32_MAX);
        uint32_t peak=0,boundary=0;double energy=0;
        /* 20 s held low note + 10 s release, then 20 s high + 10 s release,
         * then 40 s of overlapping/repeated notes with breath changes. */
        for(uint32_t frame=0;frame<100u*44100u;frame++) {
            if(frame==0)ifw_fixed_note_on(&synth,36,INT32_MAX);
            if(frame==20u*44100u)ifw_fixed_note_off(&synth,36);
            if(frame==30u*44100u)ifw_fixed_note_on(&synth,108,INT32_MAX);
            if(frame==50u*44100u)ifw_fixed_note_off(&synth,108);
            if(frame>=60u*44100u) {
                unsigned t=frame-60u*44100u,segment=t/(5u*44100u),at=t%(5u*44100u);
                if(at==0) {
                    ifw_fixed_breath(&synth,INT32_MAX/(int32_t)(1u<<(segment%3u)));
                    ifw_fixed_note_on(&synth,notes[segment],INT32_MAX);
                }
                if(at==44100u && segment)ifw_fixed_note_off(&synth,notes[segment-1]);
                if(at==2u*44100u)ifw_fixed_note_on(&synth,notes[segment],INT32_MAX/2);
                if(at==3u*44100u)ifw_fixed_note_off(&synth,notes[segment]);
            }
            int32_t l,r;ifw_fixed_sample(&synth,&l,&r);
            int32_t raw[2]={l,r};
            for(unsigned ch=0;ch<2;ch++) {
                int32_t pcm=ifw_clamp((raw[ch]+16)>>5,-32767,32767);
                int32_t output=ifw_output_sample(pcm,4096);
                uint32_t a=(uint32_t)(output<0?-output:output);
                if(a>peak)peak=a;if(a>=32767u)boundary++;
                double x=output/32768.;energy+=x*x;
            }
        }
        printf("%s{\"preset\":%u,\"seconds\":100,\"peak\":%.9f,\"rms\":%.9f,\"boundaryChannelSamples\":%u}",
               p?",":"",p,peak/32768.,sqrt(energy/(200*44100)),boundary);
        assert(!boundary);
    }
    puts("]}");return 0;
}
