/* SPDX-License-Identifier: GPL-3.0-only
 * Independent post-FX output dynamics. NORMAL is the byte-exact rc8 output.
 * LOUD drives x4 before a stereo-linked peak envelope and a soft peak guard.
 * The physical MASTER is included before detection, with no intermediate Q15
 * clipping. The board's I2S x128 and USB packing are unchanged.
 *
 * Conventional fast-attack / exponential-release detector, also used by
 * Felucca (GPL-3.0): 1/4 attack, 1/4096 release towards the threshold, at 44.1k.
 * An interpolated reciprocal table replaces per-frame division; the guard
 * rounds attack overshoot. This is not a lookahead or true-peak limiter.
 * There are no note, preset, panic or underrun hooks, and no dropped samples.
 */
#include "output_bus.h"
#include "output_bus_table.h"

#if IFW_OUTPUT_BUS
static uint32_t bus_abs(int32_t v) {return (uint32_t)(v<0?-v:v);}
static uint32_t bus_gain(uint32_t envelope) {
    uint32_t i=envelope>>8,frac=envelope&255u;
    if(i>=512u)return ifw_bus_gain_q14[512];
    uint32_t hi=ifw_bus_gain_q14[i],lo=ifw_bus_gain_q14[i+1];
    return hi-(((hi-lo)*frac+255u)>>8);
}
static int32_t bus_scale(int32_t v,uint32_t gain) {
    /* |v| <= 131072: unsigned product + rounding fits uint32_t. */
    int32_t a=(int32_t)((bus_abs(v)*gain+8192u)>>14);
    return v<0?-a:a;
}
static int32_t bus_soft(int32_t v,IFWOutputBus *b) {
    uint32_t a=bus_abs(v);
    if(a>IFW_BUS_KNEE) {
        b->soft_samples++;
        uint32_t d=a-IFW_BUS_KNEE;
        /* Unit slope at the knee, zero slope at knee+4096. Odd, monotone,
         * exactly linear below the knee; only a multiply and a shift. */
        a=d>=4096u?IFW_BUS_CEILING:a-((d*d+4096u)>>13);
    }
    if(a>b->peak)b->peak=a;
    return v<0?-(int32_t)a:(int32_t)a;
}
#endif
void ifw_output_bus_process(IFWOutputBus *b,int32_t *stereo,uint32_t frames,int32_t master_q12) {
#if !IFW_OUTPUT_BUS
    (void)b;
    for(uint32_t i=0;i<frames*2u;i++)stereo[i]=ifw_output_sample(stereo[i],master_q12);
#else
    if(!b->mode && !b->mix) {
        for(uint32_t i=0;i<frames*2u;i++)stereo[i]=ifw_output_sample(stereo[i],master_q12);
        return;
    }
    uint32_t envelope=b->envelope,mix=b->mix,target=b->mode*256u;
    for(uint32_t i=0;i<frames;i++) {
        int32_t raw_l=stereo[i*2u],raw_r=stereo[i*2u+1u];
        int32_t l=(raw_l*master_q12)>>10,r=(raw_r*master_q12)>>10;
        uint32_t peak=bus_abs(l),right=bus_abs(r);
        if(right>peak)peak=right;
        if(peak>envelope)envelope+=(peak-envelope+3u)>>2;
        else if(envelope>IFW_BUS_THRESHOLD)envelope-=(envelope-IFW_BUS_THRESHOLD+4095u)>>12;
        uint32_t gain=bus_gain(envelope);
        if(gain<b->min_gain)b->min_gain=gain;
        l=bus_soft(bus_scale(l,gain),b);r=bus_soft(bus_scale(r,gain),b);
        if(mix!=target)mix+=mix<target?1u:(uint32_t)-1;
        if(mix<256u) {
            /* 5.8ms control ramp only when the user switches OUTPUT mode. */
            int32_t normal_l=ifw_output_sample(raw_l,master_q12);
            int32_t normal_r=ifw_output_sample(raw_r,master_q12);
            l=normal_l+((l-normal_l)*(int32_t)mix>>8);
            r=normal_r+((r-normal_r)*(int32_t)mix>>8);
        }
        stereo[i*2u]=l;stereo[i*2u+1u]=r;
        if(!mix && !target) {
            for(uint32_t j=(i+1u)*2u;j<frames*2u;j++)
                stereo[j]=ifw_output_sample(stereo[j],master_q12);
            break;
        }
    }
    b->envelope=envelope;b->mix=mix;
#endif
}
