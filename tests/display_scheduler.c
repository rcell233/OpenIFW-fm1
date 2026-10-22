/* SPDX-License-Identifier: GPL-3.0-only
 * Execute the production display scheduler with the target LCD DMA gate.
 * Audio now preempts drawing; waveform updates never depend on queue depth.
 */
#include <stdint.h>
static uint32_t lcd_busy,test_ready=1;
static int ifw_display_ready(void) {return (int)test_ready;}
#define IFW_DISPLAY_SCHEDULER_TEST
#define IFW_HOST
#define main panel_simulator_main
#include "../firmware/src/app.c"
#undef main
#include <assert.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
static uint16_t expected[240*240],actual[240*240];
static void drain(void) {
    for(unsigned i=0;ui_drawing && i<20;i++)draw_panel(0);
    assert(!ui_drawing);
}
static void pixels(void) {
    memcpy(actual,fb,sizeof actual);
    for(unsigned y=0;y<240;y+=60) {
        cv_begin(240,60,uc.bg);cv_oy=-(int32_t)y;
        ui_paint();cv_oy=0;cv_blit(0,y);
    }
    memcpy(expected,fb,sizeof expected);
    if(memcmp(actual,expected,sizeof actual))for(unsigned i=0;i<240u*240u;i++)
        if(actual[i]!=expected[i]) {fprintf(stderr,"display differs at (%u,%u), t=%u, dirty=%u\n",i%240u,i/240u,fm1_ms,ui_widgets);break;}
    assert(!memcmp(actual,expected,sizeof actual));
}
int main(void) {
    fm1_ms=1000;app_init();ui_prefs.animate=0;
    ui_display_invalidate();draw_panel(1);drain();pixels();
    /* The target's busy gate latches an input invalidation without touching
     * the DMA canvas. Its next opportunity must still apply that input. */
    lcd_busy=1;test_ready=0;
    memcpy(actual,cv_px,sizeof(uint16_t)*cv_w*cv_h);
    unsigned at=ui_widget_updates[1];
    set_word(&edit_patch,control_id(0),123456);draw_panel(1);
    assert(ui_widget_updates[1]==at);
    assert(!memcmp(actual,cv_px,sizeof(uint16_t)*cv_w*cv_h));
    test_ready=1;draw_panel(0);drain();assert(ui_widget_updates[1]>at);pixels();
    /* Both waveform extrema must update without a foreground PCM queue. */
    ui_prefs.animate=1;at=ui_widget_updates[4];
    for(unsigned t=0;t<250;t++) {
        fm1_ms++;
        for(unsigned i=0;i<96;i++)scope[i]=(int16_t)(((i+t/20u)&1)?30000: -30000);
        meter.env[0]=(t/20u)&1u?INT32_MAX:INT32_MAX/2;draw_panel(0);
    }
    assert(ui_widget_updates[4]>at && ui_widget_updates[4]-at<=5);
    assert(ui_widget_updates[5]);fm1_ms+=100;draw_panel(0);drain();pixels();
    /* Legacy ANIM=OFF selects a lower refresh rate, never a frozen waveform. */
    ui_prefs.animate=0;draw_panel(1);drain();at=ui_widget_updates[4];
    for(unsigned t=0;t<250;t++) {fm1_ms++;scope[t%96]=(int16_t)(t*37);draw_panel(0);}
    assert(ui_widget_updates[4]>at && ui_widget_updates[4]-at<=3);
    fm1_ms+=100;draw_panel(0);drain();pixels();
    puts("PASS target display: busy DMA preserves input/canvas; waveform extrema, normal/low refresh both live, exact pixels");
    return 0;
}
