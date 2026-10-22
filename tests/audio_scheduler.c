/* SPDX-License-Identifier: GPL-3.0-only
 * Production ISR with an independent DMA clock. Render costs and foreground
 * work are injected inputs, not a prediction of hardware throughput.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IFW_HOST 1
#define HALF_FRAMES 256u
#define HALF_WORDS 512u
#define FM1_AUDIO_HALF 0x80u
static int32_t abuf[1024],master_q12=4096;
static uint8_t in_audio,pending,masked;
static double now,frame_cost,usb_frame_cost;
static uint32_t played,rendered,irqs,tapped,commits,background;
static struct {uint8_t feed;} uac={1};
static void audio_usb_tap(const int32_t *out,uint32_t n);
static void clock_advance(double work);
void fm1_alnk0_irq(void);
static uint32_t fm1_ticks(void) {return (uint32_t)(now*24000000./44100.);}
static uint8_t fm1_audio_pending(void) {return pending;}
static void fm1_audio_ack_aux(uint8_t p) {(void)p;}
static void fm1_audio_ack_half(void) {pending=0;}
static uint32_t fm1_audio_free_half(void) {return ((played/256u)&1u)^1u;}
static uint32_t irq_save(void) {assert(!masked);masked=1;return 0;}
static void irq_restore(uint32_t flags) {(void)flags;assert(masked);masked=0;}
static void ifw_audio_release(void) {assert(masked);commits++;}
static void uac_render_start(void) {assert(in_audio && !masked);}
static int32_t sample(uint32_t i) {return (int32_t)((i*17u)%60001u)-30000;}
static void uac_tap(const int32_t *out,uint32_t n) {
    assert(!masked && n==32u);
    for(uint32_t i=0;i<n;i++) {
        int32_t v=(sample(tapped+i)*master_q12)>>12;
        assert(out[2*i]==v && out[2*i+1]==((-sample(tapped+i)*master_q12)>>12));
    }
    tapped+=n;clock_advance(n*usb_frame_cost);
}
static void app_audio(int32_t *out,uint32_t n) {
    assert(in_audio && !masked && n==256u);
    /* DMA retains every old word until the whole block is complete. */
    int32_t old[1024];memcpy(old,abuf,sizeof old);
    for(uint32_t i=0;i<n;i++) {
        clock_advance(frame_cost);
        int32_t v=sample(rendered++);out[2*i]=v;out[2*i+1]=-v;
        if((i&31u)==31u)audio_usb_tap(out+2u*(i-31u),32u);
        assert(!memcmp(old,abuf,sizeof old));
    }
}
#include "../firmware/src/audio_perf.h"
#include "../firmware/src/audio_output.h"
static void fire(void) {
    assert(!in_audio && !masked);
    uint32_t half=fm1_audio_free_half(),before=commits,first=rendered;
    int32_t old[1024];memcpy(old,abuf,sizeof old);
    fm1_alnk0_irq();irqs++;assert(!in_audio && !masked);
    assert(!memcmp(old+(half^1u)*512u,abuf+(half^1u)*512u,512u*sizeof(int32_t)));
    if(commits!=before) {
        assert(fm1_audio_free_half()==half);
        for(uint32_t i=0;i<256u;i++) {
            int32_t v=(sample(first+i)*master_q12)>>12;
            assert(abuf[half*512u+2*i]==v*128 && abuf[half*512u+2*i+1]==(((-sample(first+i)*master_q12)>>12)*128));
        }
    }else assert(!memcmp(old,abuf,sizeof old));
}
static void clock_advance(double work) {
    double end=now+work;
    while((double)(played+1u)<=end) {
        played++;now=(double)played;
        if(!(played%256u))pending=FM1_AUDIO_HALF;
        if(pending && !in_audio) {
            double start=now;fire();end+=now-start; /* foreground is preempted */
        }
    }
    now=end;
}
static void reset(double cost) {
    now=0;frame_cost=cost;played=rendered=irqs=tapped=commits=background=0;
    in_audio=pending=masked=0;audio_underruns=0;
    memset(abuf,0,sizeof abuf);perf_phase=0;perf_reset();
}
static void run(double cost,uint32_t draw_work) {
    reset(cost);
    while(played<44100u*3u) {clock_advance(draw_work);background++;}
    assert(!audio_underruns && !audio_perf.overruns && !audio_perf.dma_late);
    assert(rendered==irqs*256u && tapped==rendered && commits==irqs && background);
    perf_snapshot();assert(perf_view.blocks==irqs);
    printf("%s{\"injectedCostRatio\":%.2f,\"injectedUsbCostRatio\":%.2f,\"masterQ12\":%d,\"foregroundWorkFrames\":%u,\"blocks\":%u,\"missedCommits\":%u}",
           cost==.75&&draw_work==1?"":",",cost,usb_frame_cost,master_q12,draw_work,irqs,audio_underruns);
}
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
int main(void) {
    puts("{\"kind\":\"production-interrupt-scheduler-functional-model-not-hardware\",\"cases\":[");
    run(.75,1);run(.75,88);run(.75,1764);run(.90,1764);run(.95,1764);
    master_q12=2048;run(.75,1764);master_q12=0;run(.75,1764);master_q12=4096;
    usb_frame_cost=.1;run(.75,1764);
    assert(audio_perf.last_ticks>audio_perf.dsp_ticks && audio_perf.dsp_ticks>0);
    usb_frame_cost=0;
    puts("],\"latePublicationChecks\":[");
    const double costs[]={.99,1.01,1.25,1.8,2.1};
    for(unsigned k=0;k<5;k++) {
        reset(costs[k]);now=played=256;pending=FM1_AUDIO_HALF;fire();
        assert(!commits && audio_underruns==1 && audio_perf.dma_late==1);
        assert(audio_perf.overruns==(costs[k]>1.0));
        if(costs[k]>1.0)assert(pending&FM1_AUDIO_HALF); /* no late ACK */
        printf("%s{\"injectedCostRatio\":%.2f,\"dmaWrites\":%u,\"missedCommits\":%u}",k?",":"",costs[k],commits,audio_underruns);
    }
    puts("]}");return 0;
}
