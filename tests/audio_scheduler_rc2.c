/* SPDX-License-Identifier: GPL-3.0-only
 * Execute the production foreground producer and bounded DMA ISR with an
 * independent sample clock. Render cost and foreground stalls are injected;
 * they are test inputs, never a prediction of hardware throughput.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IFW_HOST 1
#define HALF_FRAMES 256u
#define HALF_WORDS 512u
#define FM1_AUDIO_HALF 0x80u
static int32_t abuf[1024],master_q12=4096;
static uint8_t in_audio,pending;
static double now,frame_cost;
static uint32_t played,rendered,begun,finished,block_frame,captured_param,input_param;
static uint32_t irqs,tapped,invalid_dma,publications,stalls;
static uint32_t observed_write,active_render;
static void clock_advance(double until);
void fm1_alnk0_irq(void);
static uint32_t fm1_ticks(void) {return (uint32_t)(now*24000000./44100.);}
static uint8_t fm1_audio_pending(void) {return pending;}
static void fm1_audio_ack_aux(uint8_t p) {(void)p;}
static void fm1_audio_ack_half(void) {pending=0;}
static uint32_t fm1_audio_free_half(void) {return ((played/256u)&1u)^1u;}
static void ifw_audio_release(void) {}
static void ifw_audio_acquire(void) {}
static void uac_render_start(void) {}
static void uac_tap(const int32_t *out,uint32_t n) {(void)out;tapped+=n;}
static int32_t sample(uint32_t frame,uint32_t param) {return (int32_t)((frame*17u+param*31u)%60001u)-30000;}
static void app_audio_begin(void) {
    assert(!active_render);active_render=1;begun++;block_frame=rendered;captured_param=input_param;
}
static void app_audio_render(int32_t *out,uint32_t n) {
    assert(active_render && n==32u);
#ifdef IFW_PRODUCTION_AUDIO_TEST
    assert(in_audio);
#endif
    for(uint32_t i=0;i<n;i++) {
        clock_advance(now+frame_cost);
        int32_t v=sample(rendered++,captured_param);out[2*i]=v;out[2*i+1]=-v;
    }
}
static void app_audio_finish(int32_t *out,uint32_t n) {
    assert(active_render && n==256u && rendered-block_frame==n);
    for(uint32_t i=0;i<n;i++){assert(out[2*i]==sample(block_frame+i,captured_param));assert(out[2*i+1]==-out[2*i]);}
    active_render=0;finished++;
}
#include "../firmware/src/audio_perf.h"
#ifdef IFW_PRODUCTION_AUDIO_TEST
#include "../firmware/src/audio_output.h"
#else
#include "audio_output_rc2.h"
#endif

static void fire_irq(void) {
    uint32_t free_half=fm1_audio_free_half(),r=audio_read,w=audio_write,before=rendered;
    uint8_t saved_audio=in_audio;
    int32_t other[512],expected[512];
    memcpy(other,abuf+(free_half^1u)*512u,sizeof other);
    int consume=audio_started && r!=w;
    if(consume)for(uint32_t i=0;i<512u;i++)expected[i]=audio_pcm[r&3u][i]*128;
    else memset(expected,0,sizeof expected);
    fm1_alnk0_irq();irqs++;
    assert(!pending && in_audio==saved_audio && before==rendered); /* the ISR never renders */
    assert(audio_read==r+(uint32_t)consume);
    assert(!memcmp(expected,abuf+free_half*512u,sizeof expected));
    assert(!memcmp(other,abuf+(free_half^1u)*512u,sizeof other));
    assert(w==audio_write); /* sole producer owns the write cursor */
}
static void clock_advance(double until) {
    while((double)(played+1u)<=until) {
        if(abuf[(played%512u)*2u]%128)invalid_dma++;
        played++;
        if(!(played%256u)){now=(double)played;pending=FM1_AUDIO_HALF;fire_irq();}
    }
    now=until;
}
static void reset(double ratio) {
    now=0;frame_cost=ratio;played=rendered=begun=finished=block_frame=0;
    captured_param=input_param=irqs=tapped=invalid_dma=publications=stalls=0;
    observed_write=active_render=pending=in_audio=0;perf_phase=0;
    audio_write=audio_read=audio_started=audio_offset=audio_render_ticks=audio_underruns=0;
    memset(abuf,0,sizeof abuf);memset(audio_pcm,0,sizeof audio_pcm);perf_reset();
}
static void run(double ratio,uint32_t stall_frames) {
    reset(ratio);uint32_t turns=0;double last=0,max_gap=0,next_stall=4410;
    while(played<44100u*3u) {
        if(now-last>max_gap)max_gap=now-last;last=now;turns++;
        if(!(turns%37u))input_param++; /* controls may change mid-render */
        audio_service();
        assert(!in_audio);
        if(audio_write!=observed_write) {
            assert(audio_write==observed_write+1u);observed_write=audio_write;publications++;
        }
        assert(audio_write-audio_read<=4u);assert(audio_can_draw()==(audio_queued()>=2u));
        assert(begun==finished+(uint32_t)!!audio_offset);
        if(stall_frames && now>=next_stall){clock_advance(now+stall_frames);next_stall+=4410;stalls++;}
        clock_advance(now+0.1);
    }
    perf_snapshot();assert(!invalid_dma && !perf_view.dma_late);
    assert(tapped==audio_read*256u && publications==finished);
    assert(rendered==finished*256u+audio_offset); /* no skipped render blocks */
    assert(max_gap<2000u && turns>1000u); /* no interrupt monopolization */
    if(ratio<1 && !stall_frames)assert(!audio_underruns);
    if(ratio>1 || stall_frames>512u)assert(audio_underruns);
    printf("%s{\"injectedCostRatio\":%.2f,\"stallFrames\":%u,\"produced\":%u,\"consumed\":%u,\"underruns\":%u,\"maxMainGapMs\":%.3f,\"invalidDmaWords\":%u}",
        ratio==.75 && !stall_frames?"":",",ratio,stall_frames,audio_write,audio_read,audio_underruns,max_gap*1000./44100.,invalid_dma);
}
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
int main(void) {
    puts("{\"kind\":\"functional-scheduler-model-not-hardware-performance\",\"cases\":[");
    run(.75,0);run(.99,0);run(1.01,0);run(1.25,0);run(1.8,0);run(5.0,0);run(.75,88);run(.75,1764);
    puts("]}");return 0;
}
