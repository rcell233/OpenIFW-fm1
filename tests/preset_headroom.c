/* SPDX-License-Identifier: GPL-3.0-only
 * Offline arithmetic/headroom audit. No device timing is inferred here.
 * Read the actual post-FX sample immediately before synth.c's output clamp.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "presets.h"
_Static_assert(offsetof(IFWFixedPatch,fx_bypass)==228*4,"Probe word layout changed");
_Static_assert(offsetof(IFWFixedSynth,mono)==916,"Probe mono layout changed");
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
static const unsigned presets[]={0,3,5},notes[]={36,48,60,69,72,84,96,108};
int main(void) {
    puts("{\"kind\":\"native-arithmetic-not-hardware\",\"secondsPerCase\":4,\"cases\":[");
    unsigned row=0;
    for(unsigned p=0;p<3;p++)for(unsigned n=0;n<8;n++)for(unsigned breath=1;breath<=4;breath*=2) {
        IFWFixedPatch patch;make_preset(&patch,presets[p]);ifw_fixed_init(&synth,&patch,&buffers);
        ifw_fixed_breath(&synth,INT32_MAX/(int32_t)breath);ifw_fixed_note_on(&synth,notes[n],INT32_MAX);
        uint32_t clipped=0,boundary=0;int64_t peak=0;double energy=0;
        for(unsigned i=0;i<4*44100;i++) {
            int32_t l,r;ifw_fixed_sample(&synth,&l,&r);
            int32_t raw[2]={synth.effects.sample_left,synth.effects.sample_right};
            for(unsigned ch=0;ch<2;ch++) {
                int32_t v=synth.master_gain==IFW_Q16?raw[ch]:ifw_mul16(raw[ch],synth.master_gain);
                int64_t a=v<0?-(int64_t)v:v;if(a>peak)peak=a;
                if(a>IFW_Q20)clipped++;if(a>=IFW_Q20)boundary++;
                double x=(double)v/IFW_Q20;energy+=x*x;
            }
        }
        printf("%s{\"preset\":%u,\"note\":%u,\"breath\":%.2f,\"peak\":%.9f,\"peakDbFS\":%.5f,\"rms\":%.9f,\"clippedChannelSamples\":%u,\"boundaryChannelSamples\":%u,\"channelSamples\":%u}",
               row++?",\n":"",presets[p],notes[n],1.0/breath,(double)peak/IFW_Q20,
               peak?20*log10((double)peak/IFW_Q20):-999.,sqrt(energy/(8*44100)),clipped,boundary,8*44100u);
    }
    puts("\n]}");return 0;
}
