/* SPDX-License-Identifier: GPL-3.0-only
 * One-block lookahead. DMA only receives completed, final-format samples;
 * expensive synthesis never writes into a buffer that DMA can start reading.
 * Requires the platform's audio HAL, abuf, master_q12 and app_audio().
 */
static int32_t audio_next[HALF_WORDS];
static uint32_t audio_next_ready;
void fm1_alnk0_irq(void) {
    uint32_t start=fm1_ticks(),pending=fm1_audio_pending();
    in_audio=1;
    fm1_audio_ack_aux((uint8_t)pending);
    if(pending&FM1_AUDIO_HALF) {
        uint32_t half=fm1_audio_free_half(),missed=0;
        /* Acknowledge this event BEFORE rendering. A later boundary must stay
         * pending, so a slow render cannot erase the next audio interrupt. */
        fm1_audio_ack_half();
        if(audio_next_ready) {
            int32_t *dst=abuf+half*HALF_WORDS;
            for(uint32_t i=0;i<HALF_FRAMES;i++) {
                /* Stop a late publication at the boundary. Even the prefix
                 * already published consists entirely of valid Q24 samples. */
                if(fm1_audio_free_half()!=half){missed=1;break;}
                dst[i*2]=audio_next[i*2]*128;
                dst[i*2+1]=audio_next[i*2+1]*128;
            }
            uac_render_start();
            for(uint32_t b=0;b<HALF_FRAMES;b+=32u)
                uac_tap(audio_next+2u*b,32u);
        }
        for(uint32_t i=0;i<HALF_WORDS;i++)audio_next[i]=0;
        app_audio(audio_next,HALF_FRAMES);
        int32_t master=master_q12;
        for(uint32_t i=0;i<HALF_WORDS;i++) {
            int32_t v=(int32_t)(((int64_t)audio_next[i]*master)>>12);
            audio_next[i]=v>32767?32767:v< -32767? -32767:v;
        }
        audio_next_ready=1;
        uint32_t ticks=fm1_ticks()-start;
        audio_perf.last_ticks=ticks;audio_perf.total_ticks+=ticks;audio_perf.blocks++;
        if(ticks>audio_perf.max_ticks)audio_perf.max_ticks=ticks;
        if(ticks>IFW_AUDIO_BUDGET_TICKS)audio_perf.overruns++;
        if(missed || fm1_audio_free_half()!=half ||
           (fm1_audio_pending()&FM1_AUDIO_HALF) || ticks>IFW_AUDIO_BUDGET_TICKS)
            audio_perf.dma_late++;
    }
    in_audio=0;
}
