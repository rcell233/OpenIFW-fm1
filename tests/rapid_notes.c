/* SPDX-License-Identifier: GPL-3.0-only
 * Real app event routing/output versus an independent, unmodified DSP instance.
 * Every state byte and FX buffer must match after each dense note block.
 */
#define IFW_HOST
#define main panel_simulator_main
#include "../firmware/src/app.c"
#undef main
#include "../firmware/src/output_level.h"
#include <assert.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static IFWFixedSynth reference;
static IFWFixedFxBuffers reference_buffers;
static void send(uint8_t st,uint8_t a,uint8_t b) {
    uint32_t w=qwrite;midi_message(st,a,b);
    for(;w<qwrite;w++) {
        AudioEvent e=queue[w&(QSIZE-1u)];
        if(e.type==0)ifw_fixed_note_on(&reference,(uint32_t)e.a,e.b);
        else if(e.type==1)ifw_fixed_note_off(&reference,(uint32_t)e.a);
        else if(e.type==2)ifw_fixed_breath(&reference,e.a);
        else assert(0);
    }
}
int main(void) {
    int32_t out[512],raw[512];uint64_t checked_frames=0;uint32_t core_hash=2166136261u;
    puts("{\"kind\":\"dense-app-output-and-core-regression\",\"cases\":[");
    for(unsigned p=0;p<6;p++)for(unsigned mode=0;mode<3;mode++) {
        memset(host_flash,255,sizeof host_flash);storage_load();ui_init();make_preset(&edit_patch,p);
        ifw_fixed_init(&synth,&edit_patch,&fx_buffers);ifw_fixed_init(&reference,&edit_patch,&reference_buffers);
        ready=1;qread=qwrite=overflow=0;saved.wind=0;nrpn=16383;sustain=0;
        memset(midi_down,0,sizeof midi_down);memset(sustain_hold,0,sizeof sustain_hold);
        ifw_note_transition_reset(&note_transition);note_transition_events=0;
        send(0x90,60,110);
        unsigned old=60,events=0,protected=0,peak=0,clips=0,boundary=0;
        int32_t previous[2]={0,0};unsigned step=mode==0?8u:mode==1?4u:6u;
        for(unsigned block=0;block<900;block++) {
            if(mode==1 && block>=180 && (block-180)%step==step-1)send(0x80,(uint8_t)old,0);
            if(block>=180 && !((block-180)%step)) {
                unsigned note=mode==2?60u:60u+(events*7u)%25u;
                send(0x90,(uint8_t)note,110);
                if(mode!=1 && note!=old)send(0x80,(uint8_t)old,0);
                old=note;events++;
            }
            app_audio_begin();unsigned armed=note_transition.armed;
            if(armed)protected++;
            app_audio_render(out,256);app_audio_finish(out,256);
            ifw_fixed_process(&reference,raw,256);
            IFWFixedSynth actual=synth;actual.effects.buffers=reference.effects.buffers;
            assert(!memcmp(&actual,&reference,sizeof actual));
            assert(!memcmp(&fx_buffers,&reference_buffers,sizeof fx_buffers));
            actual.effects.buffers=0; /* exclude process addresses from the trace */
            const uint8_t *bytes=(const uint8_t *)&actual;
            for(unsigned i=0;i<sizeof actual;i++)core_hash=(core_hash^bytes[i])*16777619u;
            for(unsigned i=0;i<256;i++)for(unsigned ch=0;ch<2;ch++) {
                /* The same engine PCM returns after the fixed short window. */
                if(!armed || i>=64)assert(out[2*i+ch]==raw[2*i+ch]);
                int32_t v=ifw_output_sample(out[2*i+ch],4096);
                if(armed && !i)assert(v==previous[ch]);
                int32_t delta=v-previous[ch];unsigned d=(unsigned)(delta<0?-delta:delta);
                if(armed && !i && d>boundary)boundary=d;
                unsigned a=(unsigned)(v<0?-v:v);if(a>peak)peak=a;if(a>=32767)clips++;
                previous[ch]=v;
            }
            checked_frames+=256;
        }
        assert(!overflow && !clips);
        printf("%s{\"preset\":%u,\"mode\":%u,\"noteOns\":%u,\"protectedEvents\":%u,\"protectedBoundaryStep\":%u,\"peakQ15\":%u,\"clippedChannelSamples\":%u}",
               p||mode?",":"",p,mode,events,protected,boundary,peak,clips);
    }
    printf("],\"coreStateAndFxBuffersExact\":true,\"frames\":%llu,\"coreTraceHash\":%u}\n",(unsigned long long)checked_frames,core_hash);
}
