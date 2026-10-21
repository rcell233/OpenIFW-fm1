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
static void raster_tests(void) {
    static uint16_t expected[CV_MAX];
    const felucca_font_t *fonts[]={&UI_S,&UI_M,&UI_L,&FONT_S,&FONT_L};
    uint32_t rng=0x73516284u,checks=0;
    const char *text="AaZz 0129 !? <> \x01\x7f\xe0";
    for(uint32_t i=0;i<4000;i++) {
        rng=rng*1664525u+1013904223u;
        uint32_t w=1u+rng%240u,h=1u+(rng>>8)%64u;
        int32_t x=(int32_t)((rng>>16)%310u)-40,y=(int32_t)((rng>>6)%100u)-35;
        uint16_t color=(uint16_t)(rng>>9);
        const felucca_font_t *f=fonts[i%5];
        cv_oy=-(int32_t)(i%15)*16;
        if(i&1)y-=cv_oy; /* exercise both visible and off-strip glyphs */
        cv_begin(w,h,0x3A5Cu);
        int32_t before=ifw_original_cv_text(x,y,f,text,color);
        memcpy(expected,cv_px,w*h*2u);cv_begin(w,h,0x3A5Cu);
        assert(cv_text(x,y,f,text,color)==before);
        assert(!memcmp(expected,cv_px,w*h*2u));checks++;
        int32_t x1=(int32_t)((rng>>18)%320u)-40,y1=(int32_t)((rng>>3)%320u)-40;
        if(i%3==0)x1=x;
        if(i%3==1)y1=y;
        cv_begin(w,h,0x3A5Cu);ifw_original_cv_line(x,y,x1,y1,color);
        memcpy(expected,cv_px,w*h*2u);cv_begin(w,h,0x3A5Cu);
        cv_line(x,y,x1,y1,color);assert(!memcmp(expected,cv_px,w*h*2u));checks++;
    }
    for(uint32_t p=0;p<4;p++)for(uint32_t style=0;style<2;style++) {
        ui_prefs.palette=(uint8_t)p;ui_prefs.style=(uint8_t)style;ui_palette();
        for(uint32_t y=0;y<240;y+=16) {
            cv_oy=-(int32_t)y;cv_begin(240,16,uc.bg);
            /* Frozen original window fill, independent of the new row writer. */
            for(int j=3;j<201;j++)for(int x=3;x<237;x++) {
                uint32_t n=((uint32_t)x*0x9E37u)^((uint32_t)j*0x7F4Au);
                cv_pset(x,j,style?uc.bg:((n>>7)&7u)?uc.surf:uc.stone);
            }
            ui_frame(0,0,240,204,uc.edge);ui_frame(1,1,238,202,uc.stone);ui_frame(2,2,236,200,uc.theme);
            cv_rect(0,0,3,3,uc.accent);cv_rect(237,0,3,3,uc.accent);
            cv_rect(0,201,3,3,uc.accent);cv_rect(237,201,3,3,uc.accent);
            memcpy(expected,cv_px,240u*16u*2u);cv_begin(240,16,uc.bg);
            ui_window(0,0,240,204,3);assert(!memcmp(expected,cv_px,240u*16u*2u));checks++;
        }
    }
    ui_prefs.palette=ui_prefs.style=0;cv_oy=0;
    printf("PASS raster: %u independent font/line/stone pixel comparisons, five fonts and four palettes\n",checks);
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
        memcpy(expected,fb,sizeof expected);memset(fb,0,sizeof fb);ui_display_invalidate();
        draw_panel(1);unsigned turns=1;
        while(ui_drawing && turns<20){draw_panel(0);turns++;}
        assert(!ui_drawing && turns<=4);
        assert(!memcmp(expected,fb,sizeof expected));
        /* A fresh main-loop turn can accept another knob event even while
         * a full screen is incomplete. The renderer advances one band. */
        ui_display_invalidate();draw_panel(1);assert(ui_drawing);
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
    uint32_t pixels=ui_lcd_pixels;
    draw_panel(1);while(ui_drawing)draw_panel(0);
    assert(ui_lcd_pixels==pixels); /* an identical full request sends no rows */
    /* Continuous edits may invalidate cards/stage repeatedly. Round-robin widget
     * scheduling must still finish updates and never restart a scanline pass. */
    page=UP_OSC1;ui.hot=0;ui_drawing=ui_widgets=0;
    unsigned completed=0;uint32_t cards=ui_widget_updates[1],stage=ui_widget_updates[2];
    for(unsigned i=0;i<100;i++) {
        fm1_ms++;host_enc[ENC_KNOB1]=(i&1)?-1:1;
        uint32_t at=last_draw;app_step();tick();
        if(last_draw!=at)completed++;
    }
    (void)completed;assert(ui_widget_updates[1]-cards>=40 && ui_widget_updates[2]-stage>=40);
    while(ui_drawing)draw_panel(0);
    /* Palette changes invalidate all widgets during an uninterrupted burst. */
    draw_panel(1);
    ui_prefs.palette=1;
    for(unsigned i=0;i<32;i++)draw_panel(1);
    assert(fb[0]==RGB(255,214,92));
    while(ui_drawing)draw_panel(0);
    ui_prefs.palette=0;
    /* Menu row zero and the name text cross the cards/stage boundary. Editing
     * them must invalidate both widgets, so no half-old glyph can remain. */
    for(unsigned overlay=0;overlay<2;overlay++) {
        reset();page=UP_HOME;ui.menu=!overlay;ui.naming=(uint8_t)overlay;
        ui.menu_tab=1;ui.menu_row=0;ui.name_pos=0;str_cpy(ui.name,"ALPHA",13);
        ui_display_invalidate();draw_panel(1);while(ui_drawing)draw_panel(0);
        if(overlay)str_cpy(ui.name,"ZLPHA",13);else ui_prefs.hold++;
        draw_panel(1);while(ui_drawing)draw_panel(0);
        memcpy(expected,fb,sizeof expected);
        for(uint32_t y=0;y<240;y+=60) {
            cv_begin(240,60,uc.bg);cv_oy=-(int32_t)y;ui_paint();cv_oy=0;cv_blit(0,y);
        }
        assert(!memcmp(expected,fb,sizeof expected));
    }
    puts("PASS display: all 57 pages match synchronous pixels; continuous knob edits finish frames; four dirty widgets, no scanline hashing");
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
    raster_tests();render_tests();
    puts("PASS panel: MIDI/velocity-zero/sustain/stack, queue overflow panic, CRC storage, NRPN, all 237 controls, silent save gate");
    return 0;
}
