/* SPDX-License-Identifier: GPL-3.0-only
 * Single-producer/single-consumer PCM queue. The foreground owns a slot until
 * all 256 stereo frames are rendered and scaled. The ALNK interrupt owns it
 * from acquire until the complete copy and USB tap finish. DMA never reads a
 * producer slot, and synthesis never runs in an interrupt.
 */
#define AUDIO_SLOTS 4u
#define AUDIO_SLICE 32u
#define AUDIO_TARGET_BLOCKS 2u /* keep latency bounded; one slot may be in production */
static int32_t audio_pcm[AUDIO_SLOTS][HALF_WORDS];
static volatile uint32_t audio_write,audio_read,audio_started;
static uint32_t audio_offset,audio_render_ticks;
static uint32_t audio_queued(void) {return audio_write-audio_read;}
static int audio_can_draw(void) {return audio_queued()>=2u;}

/* One bounded unit of foreground work. Input, USB, watchdog and display
 * scheduling run between slices. Events still enter the engine once a block. */
static void audio_service(void) {
    uint32_t w=audio_write;
    if(w-audio_read>=AUDIO_TARGET_BLOCKS)return;
    int32_t *out=audio_pcm[w&(AUDIO_SLOTS-1u)];
    uint32_t t0=fm1_ticks();uint8_t saved_audio=in_audio;in_audio=1;
    if(!audio_offset) {audio_render_ticks=0;app_audio_begin();}
    app_audio_render(out+2u*audio_offset,AUDIO_SLICE);
    audio_offset+=AUDIO_SLICE;
    if(audio_offset==HALF_FRAMES) {
        app_audio_finish(out,HALF_FRAMES);
        int32_t master=master_q12;
        for(uint32_t i=0;i<HALF_WORDS;i++) {
            /* Q15 * Q12 fits signed 32 bits (MASTER is clamped to 4096). */
            int32_t v=(out[i]*master)>>12;
            out[i]=v>32767?32767:v< -32767? -32767:v;
        }
    }
    in_audio=saved_audio;audio_render_ticks+=fm1_ticks()-t0;
    if(audio_offset==HALF_FRAMES) {
        audio_perf.dsp_ticks=audio_render_ticks;
        if(audio_render_ticks>audio_perf.dsp_max_ticks)audio_perf.dsp_max_ticks=audio_render_ticks;
        if(audio_render_ticks>IFW_AUDIO_BUDGET_TICKS)audio_perf.overruns++;
        audio_offset=0;
        ifw_audio_release();
        audio_write=w+1u;
        if(!audio_started && audio_queued()>=2u)audio_started=1;
    }
}

void fm1_alnk0_irq(void) {
    uint32_t start=fm1_ticks(),pending=fm1_audio_pending();
    uint8_t saved_audio=in_audio;in_audio=1;
    fm1_audio_ack_aux((uint8_t)pending);
    if(pending&FM1_AUDIO_HALF) {
        uint32_t half=fm1_audio_free_half(),r=audio_read;
        fm1_audio_ack_half();
        int32_t *dst=abuf+half*HALF_WORDS;
        if(audio_started && r!=audio_write) {
            ifw_audio_acquire();
            const int32_t *src=audio_pcm[r&(AUDIO_SLOTS-1u)];
            for(uint32_t i=0;i<HALF_WORDS;i++)dst[i]=src[i]*128;
            uac_render_start();
            for(uint32_t b=0;b<HALF_FRAMES;b+=32u)uac_tap(src+2u*b,32u);
            ifw_audio_release();
            audio_read=r+1u;
        }else {
            /* Conventional underrun: silence, count it, keep rendering every
             * synthesis sample in order. Never skip a DSP block to feed UI. */
            for(uint32_t i=0;i<HALF_WORDS;i++)dst[i]=0;
            if(audio_started)audio_underruns++;
        }
        ifw_audio_release();
        uint32_t ticks=fm1_ticks()-start;
        audio_perf.last_ticks=ticks;audio_perf.total_ticks+=ticks;audio_perf.blocks++;
        if(ticks>audio_perf.max_ticks)audio_perf.max_ticks=ticks;
        if(fm1_audio_free_half()!=half || (fm1_audio_pending()&FM1_AUDIO_HALF))audio_perf.dma_late++;
    }
    in_audio=saved_audio;
}
