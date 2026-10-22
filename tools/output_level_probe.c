/* SPDX-License-Identifier: GPL-3.0-only
 * Measure factory output before the physical MASTER and I2S conversion.
 * This is an arithmetic audit, not an acoustic or CPU performance test.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "presets.h"
#include "output_level.h"

static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static const unsigned notes[] = {36,48,60,69,72,84,96,108};
static const unsigned velocities[] = {110,127};

int main(void) {
    unsigned row=0;
    puts("[");
    for(unsigned p=0;p<FACTORY_COUNT;p++)
    for(unsigned n=0;n<sizeof notes/sizeof notes[0];n++)
    for(unsigned v=0;v<sizeof velocities/sizeof velocities[0];v++)
    for(unsigned div=1;div<=4;div*=2) {
        IFWFixedPatch patch;
        make_preset(&patch,p);
        ifw_fixed_init(&synth,&patch,&buffers);
        ifw_fixed_breath(&synth,INT32_MAX/(int32_t)div);
        ifw_fixed_note_on(&synth,notes[n],(int32_t)((int64_t)INT32_MAX*velocities[v]/127));
        double energy=0,steady_energy=0,output_energy=0;
        int64_t raw_peak=0;
        uint32_t peak=0,raw_clips=0,boost2_clips=0,boost4_clips=0,output_peak=0,output_clips=0;
        for(unsigned i=0;i<4*44100u;i++) {
            int32_t left,right;
            ifw_fixed_sample(&synth,&left,&right);
            int32_t samples[2]={left,right};
            int32_t raw[2]={synth.effects.sample_left,synth.effects.sample_right};
            for(unsigned ch=0;ch<2;ch++) {
                int32_t before=synth.master_gain==IFW_Q16?raw[ch]:ifw_mul16(raw[ch],synth.master_gain);
                int64_t a=before<0?-(int64_t)before:before;
                if(a>raw_peak)raw_peak=a;
                if(a>IFW_Q20)raw_clips++;
                /* Exactly the production ifw_fixed_process Q20 -> Q15 conversion. */
                int32_t pcm=ifw_clamp((samples[ch]+16)>>5,-32767,32767);
                uint32_t absolute=(uint32_t)(pcm<0?-pcm:pcm);
                if(absolute>peak)peak=absolute;
                if(absolute*2u>32767u)boost2_clips++;
                if(absolute*4u>32767u)boost4_clips++;
                int32_t output=ifw_output_sample(pcm,4096);
                uint32_t oa=(uint32_t)(output<0?-output:output);
                if(oa>output_peak)output_peak=oa;
                if(oa>=32767u)output_clips++;
                double ox=output/32768.;output_energy+=ox*ox;
                double x=(double)pcm/32768.;
                energy+=x*x;
                if(i>=2*44100u)steady_energy+=x*x;
            }
        }
        printf("%s{\"preset\":%u,\"name\":\"%s\",\"note\":%u,\"velocity\":%u,\"breath\":%.2f,"
               "\"peak\":%.9f,\"rms\":%.9f,\"steadyRms\":%.9f,\"unclippedPeak\":%.9f,"
               "\"clippedChannelSamples\":%u,\"gain2ClippedChannelSamples\":%u,\"gain4ClippedChannelSamples\":%u,"
               "\"outputGain\":%u,\"outputPeak\":%.9f,\"outputRms\":%.9f,\"outputBoundaryChannelSamples\":%u}",
               row++?",\n":"",p,preset_names[p],notes[n],velocities[v],1./div,
               peak/32768.,sqrt(energy/(8*44100)),sqrt(steady_energy/(4*44100)),
               (double)raw_peak/IFW_Q20,raw_clips,boost2_clips,boost4_clips,
               IFW_OUTPUT_GAIN,output_peak/32768.,sqrt(output_energy/(8*44100)),output_clips);
    }
    puts("\n]");
    return 0;
}
