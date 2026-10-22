/* SPDX-License-Identifier: GPL-3.0-only
 * Conventional interrupt-driven audio, as in Felucca / OberVave. The ALNK
 * interrupt outranks foreground drawing. DSP never writes live DMA memory:
 * render a complete private block, then commit to the still-owned half.
 */
static int32_t audio_render[HALF_WORDS];
static uint32_t audio_usb_ticks;
#define IFW_AUDIO_COMMIT_GUARD_TICKS (200u * 24u)

/* Feed USB every 32 frames, as in Felucca, rather than starving its 1 ms
 * packets until the end of a long render. Keep the raw block unchanged for
 * the scope/clip meter, and preserve the exact hardware MASTER conversion. */
static void audio_usb_tap(const int32_t *out,uint32_t n) {
    if(!uac.feed)return;
    int32_t usb_pcm[64],master=master_q12;uint32_t t=fm1_ticks();
    for(uint32_t i=0;i<n*2u;i++) {
        int32_t v=(out[i]*master)>>12;
        usb_pcm[i]=v>32767?32767:v< -32767? -32767:v;
    }
    uac_tap(usb_pcm,n);audio_usb_ticks+=fm1_ticks()-t;
}

void fm1_alnk0_irq(void) {
    uint32_t start=fm1_ticks(),pending=fm1_audio_pending();
    in_audio=1;
    fm1_audio_ack_aux((uint8_t)pending);
    if(pending&FM1_AUDIO_HALF) {
        uint32_t half=fm1_audio_free_half();
        /* A boundary arriving during DSP must remain pending. */
        fm1_audio_ack_half();
        uac_render_start();
        for(uint32_t i=0;i<HALF_WORDS;i++)audio_render[i]=0;
        audio_usb_ticks=0;
        uint32_t dsp_start=fm1_ticks();
        app_audio(audio_render,HALF_FRAMES);
        int32_t master=master_q12;
        for(uint32_t i=0;i<HALF_WORDS;i++) {
            int32_t v=(audio_render[i]*master)>>12;
            audio_render[i]=v>32767?32767:v< -32767? -32767:v;
        }
        uint32_t dsp_ticks=fm1_ticks()-dsp_start-audio_usb_ticks;
        audio_perf.dsp_ticks=dsp_ticks;
        if(dsp_ticks>audio_perf.dsp_max_ticks)audio_perf.dsp_max_ticks=dsp_ticks;

        uint32_t flags=irq_save(),committed=0;
        if(fm1_audio_free_half()==half && !(fm1_audio_pending()&FM1_AUDIO_HALF) &&
           fm1_ticks()-start<IFW_AUDIO_BUDGET_TICKS-IFW_AUDIO_COMMIT_GUARD_TICKS) {
            int32_t *dst=abuf+half*HALF_WORDS;
            for(uint32_t i=0;i<HALF_WORDS;i++)dst[i]=audio_render[i]*128;
            ifw_audio_release();
            committed=fm1_audio_free_half()==half && !(fm1_audio_pending()&FM1_AUDIO_HALF);
        }
        irq_restore(flags);
        if(!committed) {audio_underruns++;audio_perf.dma_late++;}
        uint32_t ticks=fm1_ticks()-start;
        audio_perf.last_ticks=ticks;audio_perf.total_ticks+=ticks;audio_perf.blocks++;
        if(ticks>audio_perf.max_ticks)audio_perf.max_ticks=ticks;
        if(ticks>IFW_AUDIO_BUDGET_TICKS)audio_perf.overruns++;
    }
    in_audio=0;
}
