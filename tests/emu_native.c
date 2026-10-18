/* SPDX-License-Identifier: GPL-3.0-only */
#include "../web/emu/openifw_web.c"
#include <stdio.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
/* Binary protocol for native/WASM equivalence: identical API calls, then PCM,
 * actual RGB565 screen, patch words and musical state after each render. */
int main(void) {
    uint32_t c[4];web_nor_erase();web_boot();
    while(fread(c,sizeof c,1,stdin)==1) {
        switch(c[0]) {
        case 0:web_keys(c[1]);break;
        case 1:web_buttons(c[1]);break;
        case 2:web_enc(c[1],(int32_t)c[2]);break;
        case 3:web_midi(c[1],c[2],c[3]);break;
        case 4:web_master(c[1]);break;
        case 6:web_render(c[1]);break; /* full UI replay under ASan/UBSan */
        case 5:{
            web_render(c[1]);
            fwrite(web_out_l(),4,c[1],stdout);fwrite(web_out_r(),4,c[1],stdout);
            fwrite(web_screen(),2,240*240,stdout);fwrite(web_patch(),sizeof(IFWFixedPatch),1,stdout);
            uint32_t state[]={web_now_ms(),web_preset(),web_page(),web_home(),web_held(),(uint32_t)web_note(),
                web_lit_buttons(),web_lit_keys(),web_env_stage(0),(uint32_t)web_env_level(0),
                web_env_stage(1),(uint32_t)web_env_level(1),web_env_stage(2),(uint32_t)web_env_level(2),
                web_env_stage(3),(uint32_t)web_env_level(3),web_flash_writes_count()};
            fwrite(state,sizeof state,1,stdout);break;
        }
        }
    }
    return ferror(stdout)?1:0;
}
