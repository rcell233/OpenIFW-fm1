/* SPDX-License-Identifier: GPL-3.0-only
 * Run the actual wet tank against the same topology/coefficient values in
 * double precision. The residual includes correlated quantization distortion;
 * it is not a measurement of acoustic noise or hardware SNR.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "presets.h"
static uint32_t advance(uint32_t i,uint32_t n){return ++i==n?0:i;}
#include "reverb_half.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static const unsigned sizes[6]={558,594,639,678,278,221},offsets[6]={0,558,1152,1791,2469,2747};
static struct {double line[2968],store[4];unsigned head[6];} reference;
static double bound(double x){return x>1?1:x< -1?-1:x;}
static void ref_tick(double input,double damp,double fb,double out[2]) {
    double wet=0,tap=0;
    for(unsigned k=0;k<4;k++) {
        unsigned p=offsets[k]+reference.head[k];double value=reference.line[p];
        reference.store[k]=value+(reference.store[k]-value)*damp;
        reference.line[p]=bound(input+reference.store[k]*fb);wet+=value;
        reference.head[k]=advance(reference.head[k],sizes[k]);
    }
    for(unsigned k=4;k<6;k++) {
        unsigned p=offsets[k]+reference.head[k];double value=reference.line[p],in=wet;
        wet=value-in;reference.line[p]=bound(in+value*.5);
        reference.head[k]=advance(reference.head[k],sizes[k]);if(k==4)tap=wet;
    }
    out[0]=wet*4;out[1]=tap*6;
}
int main(void) {
    unsigned checks=0;uint32_t seed=0x12345678;
    const int32_t coefficients[]={0,1,256,16384,32767,32768};
    const int32_t limit=IFW_REVERB_BITS==15?65535:16777216;
    for(unsigned k=0;k<6;k++)for(unsigned i=0;i<32768;i++) {
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        int32_t value=(int32_t)(seed%(uint32_t)(2*limit+1))-limit;
        assert(reverb_mul15(value,coefficients[k])==(int32_t)(((int64_t)value*coefficients[k])/32768));checks++;
    }
    assert(reverb_mul15(-1,32767)==0 && reverb_mul15(1,32767)==0);
    printf("{\"kind\":\"tank-residual-not-hardware-SNR\",\"bits\":%u,\"arithmeticChecks\":%u,\"cases\":[",IFW_REVERB_BITS,checks);
    for(unsigned c=0;c<3;c++) {
        IFWFixedPatch patch;make_preset(&patch,0);ifw_fixed_init(&synth,&patch,&buffers);
        memset(&reference,0,sizeof reference);
        int32_t damp=synth.effects.reverb_damp15,fb=synth.effects.reverb_feedback15;
        double w1=synth.effects.reverb_w1/1048576.,w2=synth.effects.reverb_w2/1048576.;
        double level=c==0?.1:c==1?.01:.001,signal=0,error=0;
        unsigned last=0,zero_errors=0;
        for(unsigned i=0;i<22050;i++) {
            reverb_half_tick(&synth.effects,&buffers,0);
            if(synth.effects.reverb_wet[0]||synth.effects.reverb_wet[1])zero_errors++;
        }
        for(unsigned i=0;i<40*22050u;i++) {
            /* Match the existing equal-L/R downmix. Preserve exact Q20 input
             * in both models, isolating errors added inside the wet tank. */
            int32_t input=i<8*22050u?(int32_t)(level*.03*sin(2*3.141592653589793*440*i/22050)*1048576):0;
            reverb_half_tick(&synth.effects,&buffers,reverb_from20(input));
            double ref[2],actual[2]={synth.effects.reverb_wet[0]/(double)(1u<<IFW_REVERB_BITS),
                                     synth.effects.reverb_wet[1]/(double)(1u<<IFW_REVERB_BITS)};
            ref_tick(input/1048576.,damp/32768.,fb/32768.,ref);
            if(actual[0]||actual[1])last=i;
            if(i>=2*22050u && i<8*22050u)for(unsigned ch=0;ch<2;ch++) {
                double expected=ref[ch]*w1+ref[ch^1u]*w2,got=actual[ch]*w1+actual[ch^1u]*w2;
                signal+=expected*expected;error+=(got-expected)*(got-expected);
            }
        }
        assert(!zero_errors && last<39*22050u && signal>0);
        printf("%s{\"preReverbLevel\":%.4f,\"zeroInputNonzeroFrames\":%u,\"referenceWetRms\":%.12g,"
               "\"errorRms\":%.12g,\"errorRelativeDb\":%.6f,\"lastNonzeroSeconds\":%.6f}",
               c?",":"",level,zero_errors,sqrt(signal/(12*22050)),sqrt(error/(12*22050)),10*log10(error/signal),last/22050.);
    }
    puts("]}");return 0;
}
