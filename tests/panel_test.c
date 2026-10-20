/* SPDX-License-Identifier: GPL-3.0-only */
#define IFW_HOST
#define main panel_simulator_main
#include "../firmware/src/app.c"
#undef main
#include <assert.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
static int32_t audio[512];
static void tick(void) {app_audio(audio,256);}
static void reset(void) {
    memset(host_flash,255,sizeof host_flash);storage_load();ui_init();make_preset(&edit_patch,0);
    ifw_fixed_init(&synth,&edit_patch,&fx_buffers);ready=1;qwrite=qread=overflow=0;saved.wind=0;nrpn=16383;
    memset(midi_down,0,sizeof midi_down);memset(sustain_hold,0,sizeof sustain_hold);sustain=0;
}
static void math_tests(void) {
    uint32_t state=581;for(int i=0;i<10000;i++) {state=state*1664525u+1013904223u;uint32_t d=state|1u;
        uint64_t n=((uint64_t)(state/3u)<<32)|(state^0xACE182u);uint64_t expected=n/d;
        assert(ifw_udiv64(n,d)==(expected>UINT32_MAX?UINT32_MAX:(uint32_t)expected));}
    assert(llabs((long long)ifw_note_step(69*65536,440*65536)-21426140)<2);
    assert(ifw_sin20(0)==0 && ifw_sin20(1u<<30)==IFW_Q20);
    assert(ifw_wave20(0,0,4,1u<<31)==0);
    for(int i=-1000;i<=1000;i++) {int32_t v=i*1024;double x=v/1048576.;double expected=x/(1+.015*(x<0?-x:x));assert(__builtin_fabs(ifw_soft20(v)/1048576.-expected)<4e-5);}
}
static void render_tests(void) {
    static uint16_t expected[240*240];
    for(uint32_t p=0;p<UP_COUNT;p++) {
        page=p;ui.menu=ui.confirm=ui.naming=ui.layer=0;
        ui_palette();perf_snapshot();
        /* The previous synchronous renderer is the pixel oracle at a frozen
         * clock/state, including all clipping and canvas coordinate offsets. */
        for(uint32_t y=0;y<240;y+=60) {
            cv_begin(240,60,uc.bg);cv_oy=-(int32_t)y;
            ui_header();ui_cards();ui_stage();ui_footer();
            cv_oy=0;cv_blit(0,y);
        }
        memcpy(expected,fb,sizeof expected);memset(fb,0,sizeof fb);
        draw_panel(1);unsigned turns=1;
        while(ui_drawing && turns<20){draw_panel(0);turns++;}
        assert(!ui_drawing && turns==15);
        assert(!memcmp(expected,fb,sizeof expected));
        /* A fresh main-loop turn can accept another knob event even while
         * a full screen is incomplete. The renderer advances one band. */
        draw_panel(1);assert(ui_drawing && ui_draw_y==16);
        uint32_t before=qwrite;
        host_enc[ENC_KNOB1]=1;dirty=0;fm1_ms+=1;app_step();
        assert(host_enc[ENC_KNOB1]==0);
        if(qwrite!=before)tick();
        while(ui_drawing)draw_panel(0);
    }
    reset();page=UP_HOME;ui.menu=ui.confirm=ui.naming=ui.layer=0;
    draw_panel(1);while(ui_drawing)draw_panel(0);
    /* Identical partial refreshes must not alter even a single LCD pixel. */
    memcpy(expected,fb,sizeof expected);
    draw_panel(0);while(ui_drawing)draw_panel(0);
    assert(!memcmp(expected,fb,sizeof expected));
    puts("PASS display: all 57 pages match synchronous pixels; 16 rows per turn; controls remain serviced between bands");
}
int main(void) {
    math_tests();reset();
    midi_message(0x90,60,80);tick();assert(synth.note==60 && synth.mono.count==1);
    midi_message(0x90,67,100);tick();assert(synth.note==67 && synth.mono.count==2);
    midi_message(0x90,67,90);tick();assert(synth.mono.count==2);
    midi_message(0x90,67,0);tick();assert(synth.note==60 && synth.mono.count==1);
    midi_message(0xB0,64,127);midi_message(0x80,60,0);tick();assert(synth.mono.count==1);
    midi_message(0xB0,64,0);tick();assert(synth.mono.count==0 && !synth.gate_target);
    midi_message(0x90,65,90);tick();midi_message(0xB0,123,0);tick();assert(!synth.mono.count);
    for(unsigned i=0;i<140;i++)enqueue(0,60,INT32_MAX);
    assert(overflow);tick();assert(!synth.mono.count && !overflow);
    saved.wind=1;saved.slot=2;saved.learn[30]=P_Filter1Frequency;saved.user[2]=edit_patch;
    assert(!storage_save());saved.wind=0;storage_load();assert(saved.wind==1 && saved.slot==2 && saved.learn[30]==P_Filter1Frequency);
    host_flash[200]^=1;storage_load();assert(!saved.wind && saved.learn[30]==-1);
    // NRPN must wait for the LSB, and update the exact selected parameter.
    reset();midi_message(0xB0,99,0);midi_message(0xB0,98,P_Filter1Frequency);midi_message(0xB0,6,64);
    assert(!qwrite);midi_message(0xB0,38,0);tick();assert(synth.patch.v[P_Filter1Frequency]>327000 && synth.patch.v[P_Filter1Frequency]<328000);
    // Every panel control can be moved to both ends without invalid array indices.
    for(uint32_t id=0;id<237;id++) {edit_value(id,127,1);tick();edit_value(id,0,1);tick();}
    reset();ifw_fixed_breath(&synth,INT32_MAX);ifw_fixed_note_on(&synth,69,INT32_MAX);tick();ifw_fixed_note_off(&synth,69);
    for(int i=0;i<1400;i++)tick();
    assert(meter.quiet);
    render_tests();
    puts("PASS panel: MIDI/velocity-zero/sustain/stack, queue overflow panic, CRC storage, NRPN, all 237 controls, silent save gate");
    return 0;
}
