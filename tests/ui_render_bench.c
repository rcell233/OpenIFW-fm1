/* SPDX-License-Identifier: GPL-3.0-only
 * Relative desktop renderer cost, never a device FPS/CPU measurement.
 * IFW_BENCH_APP can select the pristine released app in another checkout.
 */
#define _POSIX_C_SOURCE 200809L
#define IFW_HOST
#define main panel_script_main
#ifndef IFW_BENCH_APP
#define IFW_BENCH_APP "../firmware/src/app.c"
#endif
#include IFW_BENCH_APP
#undef main
#include <time.h>
static uint64_t ns(void) {
    struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);
    return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;
}
int main(void) {
    memset(host_flash,255,sizeof host_flash);storage_load();ui_init();
    make_preset(&edit_patch,0);fm1_ms=1000;
    const uint32_t pages[]={UP_HOME,UP_OSC1,UP_EG1,UP_LFO1,UP_PRESETS};
    printf("{\"kind\":\"desktop-relative-ui-cost-not-device-fps\",\"cases\":[");
    for(unsigned c=0;c<sizeof pages/sizeof *pages;c++) {
        page=pages[c];ui_drawing=0;ui.menu=ui.confirm=ui.naming=ui.layer=0;
        uint64_t elapsed=0;
        for(unsigned i=0;i<33;i++) {
            ui_display_invalidate();
            uint64_t start=ns();draw_panel(1);while(ui_drawing)draw_panel(0);
            if(i)elapsed+=ns()-start;
        }
        uint32_t hash=2166136261u;
        for(unsigned i=0;i<240u*240u;i++)hash=(hash^fb[i])*16777619u;
        printf("%s{\"page\":%u,\"meanUs\":%.3f,\"pixelHash\":%u}",c?",":"",page,elapsed/32000.,hash);
    }
    ui_init();page=UP_OSC1;ui_drawing=0;draw_panel(1);
    uint32_t completed=0;
    for(unsigned i=0;i<100;i++) {
        fm1_ms++;host_enc[ENC_KNOB1]=(i&1)?-1:1;
        uint32_t last=last_draw;app_step();
        if(last_draw!=last)completed++;
    }
    printf("],\"burstCompletedUpdates\":%u}\n",completed);
    return 0;
}
