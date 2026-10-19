/* SPDX-License-Identifier: GPL-3.0-only
 * The same app.c + fixed DSP as the board. No JavaScript synthesizer or UI model.
 * Simulated FM-1 input, LCD, flash and audio clock; USB/bootloader/CPU are not emulated.
 */
#define IFW_EMU 1
#include "../../firmware/src/app.c"
#ifdef __wasm__
#define EXPORT __attribute__((used,visibility("default")))
#else
#define EXPORT
#endif
static uint32_t emu_booted,emu_owed,emu_read,emu_write,emu_master=700;
static float emu_ring[4096*2],emu_left[1024],emu_right[1024];
static int32_t emu_audio[HALF_FRAMES*2];
static int32_t emu_master_avg=512*16;
EXPORT void web_nor_erase(void) {memset(emu_flash,255,sizeof emu_flash);}
EXPORT uint8_t *web_nor(void) {return emu_flash;}
EXPORT uint32_t web_nor_size(void) {return sizeof emu_flash;}
EXPORT uint32_t web_flash_writes_count(void) {return emu_writes;}
EXPORT void web_boot(void) {if(!emu_booted){app_init();emu_booted=1;}}
EXPORT void web_master(uint32_t v) {emu_master=v>1023?1023:v;}
/* Printed roles and labels match Felucca's physical FM-1 panel. */
static const uint8_t emu_button_map[]={BTN_FX,BTN_SEL,BTN_ENV,BTN_LFO,BTN_EDIT,BTN_GLO,BTN_HOME,BTN_SAVE,BTN_ARP,BTN_SEQ,BTN_PLAY,BTN_REC,BTN_OCT_DOWN,BTN_OCT_UP};
static const uint8_t emu_encoder_map[]={ENC_SELECT,ENC_ALGORITHM,ENC_PRESETS,ENC_KNOB1,ENC_KNOB2,ENC_KNOB3,ENC_KNOB4};
EXPORT void web_buttons(uint32_t labels) {
    uint32_t b=0;for(uint32_t i=0;i<14;i++)if(labels&(1u<<i))b|=1u<<emu_button_map[i];
    emu_pressed|=b&~fm1_in.buttons;emu_released|=fm1_in.buttons&~b;fm1_in.buttons=b;
}
EXPORT void web_keys(uint32_t m) {fm1_in.notes=m&((1u<<27)-1);}
EXPORT void web_enc(uint32_t role,int32_t n) {if(role<7)emu_enc[emu_encoder_map[role]]=ifw_sat((int64_t)emu_enc[emu_encoder_map[role]]+n);}
EXPORT uint32_t web_midi(uint32_t s,uint32_t a,uint32_t b) {
    if(s<128||s>=240||a>127||b>127)return 0;
    if(emu_mw-emu_mr>=128)return 0;
    uint8_t *p=emu_midi[emu_mw++%128];p[0]=(uint8_t)s;p[1]=(uint8_t)a;p[2]=(uint8_t)b;return 1;
}
static void emu_ms(void) {
    fm1_ms++;
    emu_master_avg+=((int32_t)emu_master*16-emu_master_avg)/8;
    app_step();emu_owed+=FS;
    while(emu_owed>=1000*HALF_FRAMES) {
        emu_owed-=1000*HALF_FRAMES;memset(emu_audio,0,sizeof emu_audio);app_audio(emu_audio,HALF_FRAMES);
        uint32_t k10=(uint32_t)(emu_master_avg/16);int32_t master_q12=(int32_t)(k10*k10>>8);
        for(uint32_t i=0;i<HALF_FRAMES;i++) {
            uint32_t j=emu_write++%4096;
            emu_ring[j*2]=(float)ifw_clamp(emu_audio[i*2]*master_q12>>12,-32767,32767)/65536.f;
            emu_ring[j*2+1]=(float)ifw_clamp(emu_audio[i*2+1]*master_q12>>12,-32767,32767)/65536.f;
        }
    }
}
EXPORT void web_render(uint32_t n) {
    if(n>1024)n=1024;if(!emu_booted)web_boot();
    while(emu_write-emu_read<n)emu_ms();
    for(uint32_t i=0;i<n;i++){uint32_t j=emu_read++%4096;emu_left[i]=emu_ring[j*2];emu_right[i]=emu_ring[j*2+1];}
}
EXPORT float *web_out_l(void) {return emu_left;}
EXPORT float *web_out_r(void) {return emu_right;}
EXPORT uint16_t *web_screen(void) {return emu_fb;}
EXPORT uint32_t web_screen_draws(void) {return emu_draws;}
EXPORT uint32_t web_sample_rate(void) {return FS;}
EXPORT uint32_t web_now_ms(void) {return fm1_ms;}
EXPORT uint32_t web_lit_keys(void) {uint32_t v=0;for(uint32_t i=0;i<27;i++)v|=(uint32_t)emu_led[14+i]<<i;return v;}
EXPORT uint32_t web_lit_buttons(void) {uint32_t v=0;for(uint32_t i=0;i<14;i++)v|=(uint32_t)emu_led[emu_button_map[i]]<<i;return v;}
EXPORT uint32_t web_dim_buttons(void) {return 0;}
EXPORT uint32_t web_dim_keys(void) {return 0;}
EXPORT uint32_t web_dim_level(void) {return 0;}
EXPORT uint32_t web_breath_buttons(void) {return 0;}
EXPORT uint32_t web_breath_keys(void) {return 0;}
/* Read-only debugging/verification exports. */
EXPORT IFWFixedPatch *web_patch(void) {return &edit_patch;}
EXPORT uint32_t web_preset(void) {return preset;}
EXPORT uint32_t web_page(void) {return page;}
EXPORT uint32_t web_home(void) {return home;}
EXPORT uint32_t web_held(void) {return meter.held;}
EXPORT int32_t web_note(void) {return meter.note;}
EXPORT uint32_t web_env_stage(uint32_t i) {return i<4?synth.env[i].stage:0;}
EXPORT int32_t web_env_level(uint32_t i) {return i<4?synth.env[i].level:0;}
EXPORT uint32_t web_ui_menu(void) {return ui.menu;}
EXPORT uint32_t web_ui_layer(void) {return ui.layer;}
EXPORT uint32_t web_ui_confirm(void) {return ui.confirm;}
EXPORT uint32_t web_ui_naming(void) {return ui.naming;}
EXPORT uint32_t web_ui_slot(void) {return ui.mod_slot;}
EXPORT uint32_t web_ui_control_id(uint32_t k) {return control_id(k);}
EXPORT uint32_t web_perf_mode(void) {return perf_mode;}
EXPORT const char *web_ui_control_name(uint32_t k) {return k>3?"":ui.menu?ui_menu_label(ui.menu_tab,ui.menu_row):ui.naming?(k==0?"LETTER":k==1?"CURSOR":k==3?"SAVE":""):ui_card_label(k);}
EXPORT const char *web_ui_control_value(uint32_t k) {static char value[4][32];if(k>3)return "";if(ui.menu)ui_menu_text(ui.menu_tab,ui.menu_row,value[k]);else if(ui.naming)str_cpy(value[k],ui.name,32);else ui_value(control_id(k),value[k],0);return value[k];}
EXPORT const char *web_ui_title(void) {return ui.menu?"MENU":ui.naming?"NAME USER SOUND":ui.layer?ui_layer_name():ui_pages[page].title;}
