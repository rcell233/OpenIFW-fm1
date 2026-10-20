/* SPDX-License-Identifier: GPL-3.0-only
 * TIMER4 measures the complete ALNK ISR, including DMA conversion and USB tap.
 * Desktop/WASM intentionally expose no device timing figures.
 */
typedef struct {
    uint32_t magic,tag,blocks,last_ticks,max_ticks,overruns,dma_late;
    uint32_t dsp_ticks,dsp_max_ticks,peak,clips;
    uint64_t total_ticks;
} IFWAudioPerf;
static volatile IFWAudioPerf audio_perf;
static IFWAudioPerf perf_view;
static volatile uint32_t perf_mode; /* 0 synth, 1 diagnostic sine, 2 mute; not saved */
static uint32_t perf_freeze,perf_phase;
#define IFW_AUDIO_BUDGET_TICKS 139319u /* floor(24 MHz * 256 / 44100) */
static void perf_reset(void) {
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    uint32_t irq=irq_save();
#endif
    memset((void *)&audio_perf,0,sizeof audio_perf);
    audio_perf.magic=0x50465749u;audio_perf.tag=0x31524550u;
    perf_view=audio_perf;perf_freeze=0;
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    irq_restore(irq);
#endif
}
static void perf_snapshot(void) {
    if(perf_freeze)return;
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    uint32_t irq=irq_save();
#endif
    perf_view=audio_perf;
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    irq_restore(irq);
#endif
}
#if !defined(IFW_HOST) && !defined(IFW_EMU)
#include "audio_output.h"
#endif
