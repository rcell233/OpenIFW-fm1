/* SPDX-License-Identifier: GPL-3.0-only
 * Execute the production display scheduler with target DMA/headroom gates.
 * In particular a one-block queue must not starve a moving home waveform.
 */
#include <stdint.h>
static uint32_t lcd_busy,test_ready=1,test_queued=2;
static int ifw_display_ready(void) {return (int)test_ready;}
static uint32_t audio_queued(void) {return test_queued;}
static int audio_can_draw(void) {return test_queued>=2u;}
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
    /* Continuous low headroom used to leave only the centre line on screen.
     * Test both extrema: the entire +/-43 pixel trace must be repainted. */
    test_queued=1;at=ui_widget_updates[4];
    for(unsigned t=0;t<250;t++) {
        fm1_ms++;
        for(unsigned i=0;i<96;i++)scope[i]=(int16_t)((i&1)?30000: -30000);
        meter.env[0]=INT32_MAX;draw_panel(0);
    }
    assert(ui_widget_updates[4]>at && ui_widget_updates[4]-at<=3);
    assert(ui_widget_updates[5]);pixels();
    /* With no completed audio block, animation waits; it recovers when one
     * becomes available. The gate never pretends a background draw succeeded. */
    test_queued=0;at=ui_widget_updates[4];
    for(unsigned t=0;t<150;t++) {fm1_ms++;scope[t%96]=(int16_t)(t*37);draw_panel(0);}
    assert(ui_widget_updates[4]==at && (ui_widgets&16u));
    test_queued=1;fm1_ms+=100;draw_panel(0);
    assert(ui_widget_updates[4]>at);pixels();
    puts("PASS target display gates: busy DMA invalidation, 1-block waveform fairness, extrema, empty-queue recovery, exact pixels");
    return 0;
}
