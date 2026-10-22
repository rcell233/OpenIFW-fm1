/* SPDX-License-Identifier: GPL-3.0-only
 * Actual FX impulse responses at three sizes; silence and decay, no CPU claim.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"
#include "presets.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static const unsigned ends[]={2205,4410,11025,22050,44100,88200,176400,352800,705600,1411200,1764000};
int main(int argc,char **argv) {
    printf("{\"reverbRate\":%u,\"cases\":[",44100u/IFW_REVERB_DIV);
    for(unsigned c=0;c<3;c++) {
        IFWFixedPatch p;make_preset(&p,0);p.fx_bypass=15;p.v[P_ExciterMix]=0;
        p.v[P_EqLowGain]=p.v[P_EqHighGain]=0;p.fx[4][IFW_FX_Size]=c==0?0:c==1?432538:655360;
        p.fx[4][IFW_FX_WetDry]=655360;ifw_fixed_init(&synth,&p,&buffers);
        double energy[11]={0},lp[2]={0},low_energy=0;uint32_t first=0,last=0,peak=0;unsigned win=0;
        FILE *dump=NULL;if(argc==2&&c==1){dump=fopen(argv[1],"wb");if(!dump)return 2;}
        for(unsigned i=0;i<1764000;i++) {
            ifw_fixed_fx_tick(&synth,i==64?IFW_Q20/2:0);
            int32_t v[2]={synth.effects.sample_left,synth.effects.sample_right};
            if(i>=ends[win]&&win<10)win++;
            if(i>=128)for(unsigned ch=0;ch<2;ch++) {
                uint32_t a=v[ch]<0?(uint32_t)(-(int64_t)v[ch]):(uint32_t)v[ch];
                if(a){if(!first)first=i;last=i;}if(a>peak)peak=a;
                double x=(double)v[ch]/IFW_Q20;energy[win]+=x*x;
                lp[ch]+=0.132791509*(x-lp[ch]);low_energy+=lp[ch]*lp[ch];
            }
            if(dump&&i<6*44100){float frame[2]={v[0]/1048576.f,v[1]/1048576.f};if(fwrite(frame,sizeof frame,1,dump)!=1)return 3;}
        }
        if(dump)fclose(dump);
        printf("%s{\"size\":%.1f,\"firstWetFrame\":%u,\"lastNonzeroFrame\":%u,\"peak\":%.9f,\"energy\":[",c?",":"",p.fx[4][IFW_FX_Size]/65536.,first,last,peak/1048576.);
        for(unsigned k=0;k<11;k++)printf("%s%.12g",k?",":"",energy[k]);printf("],\"lowpass1kEnergy\":%.12g}",low_energy);
        if(!first||last>=1411200||peak>IFW_Q20)return 4;
    }
    puts("]}");return 0;
}
