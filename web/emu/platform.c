/* SPDX-License-Identifier: GPL-3.0-only
 * Host FM-1 HAL. The board's app.c, renderer and fixed DSP run unchanged above it.
 * Clocked from audio, following Felucca / X0X's emulator design.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
enum {BTN_OCT_DOWN,BTN_OCT_UP,BTN_FX,BTN_SEL,BTN_ENV,BTN_LFO,BTN_EDIT,BTN_GLO,BTN_HOME,BTN_SAVE,BTN_ARP,BTN_SEQ,BTN_PLAY,BTN_REC};
enum {ENC_SELECT,ENC_ALGORITHM,ENC_KNOB1,ENC_KNOB2,ENC_KNOB3,ENC_KNOB4,ENC_PRESETS};
#define KEY_LED(k) (14u+(k))
#define FS 44100u
#define HALF_FRAMES 256u
#define SETTINGS_AT 0u
static volatile uint32_t fm1_ms;
static struct {uint32_t notes,buttons;} fm1_in;
static uint32_t emu_pressed,emu_released,emu_draws,emu_writes;
static int32_t emu_enc[7];
static uint8_t emu_led[41],emu_flash[4096],flash_ok=1;
static uint16_t emu_fb[240*240];
static uint32_t fm1_input_edges(uint32_t *r) {uint32_t p=emu_pressed;*r=emu_released;emu_pressed=emu_released=0;return p;}
static int32_t fm1_enc_take(uint32_t e) {int32_t n=emu_enc[e];emu_enc[e]=0;return n;}
static void fm1_led_key(uint32_t k,int on) {if(k<41)emu_led[k]=(uint8_t)(on!=0);}
static void platform_poll(void) {}
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c) {
    c=(uint16_t)(c>>8|c<<8);
    for(uint32_t j=y;j<y+h&&j<240;j++)for(uint32_t i=x;i<x+w&&i<240;i++)emu_fb[j*240+i]=c;
    emu_draws++;
}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *p) {
    for(uint32_t j=0;j<h&&y+j<240;j++)for(uint32_t i=0;i<w&&x+i<240;i++)emu_fb[(y+j)*240+x+i]=p[j*w+i];
    emu_draws++;
}
#define CV_MAX (240u*64u)
#include "../../vendor/dinghy/firmware/src/gfx.c"
static int st_read(uint32_t off,void *p,uint32_t n) {if(off>4096||n>4096-off)return -1;memcpy(p,emu_flash+off,n);return 0;}
static int fl_erase4k(uint32_t off,uint32_t *took) {if(off)return -1;*took=0;memset(emu_flash,255,sizeof emu_flash);emu_writes++;return 0;}
static int fl_write(uint32_t off,const void *p,uint32_t n) {
    if(off>4096||n>4096-off)return -1;
    const uint8_t *s=p;for(uint32_t i=0;i<n;i++)emu_flash[off+i]&=s[i];emu_writes++;return 0;
}
static uint8_t emu_midi[128][3];
static uint32_t emu_mw,emu_mr;
static int midi_get(uint8_t *s,uint8_t *a,uint8_t *b) {
    if(emu_mr==emu_mw)return 0;
    const uint8_t *p=emu_midi[emu_mr++%128];*s=p[0];*a=p[1];*b=p[2];return 1;
}
static void midi_send(uint32_t s,uint32_t a,uint32_t b) {(void)s;(void)a;(void)b;}
static void str_cpy(char *d,const char *s,unsigned n) {if(!n)return;while(--n&&*s)*d++=*s++;*d=0;}
static void fmt_int(char *d,int32_t v) {
    uint32_t n=v<0?(uint32_t)(-(int64_t)v):(uint32_t)v;char r[12];unsigned k=0;
    if(v<0)*d++='-';do {r[k++]=(char)('0'+n%10);n/=10;}while(n);
    while(k)*d++=r[--k];*d=0;
}
static void fmt_fix(char *d,int32_t v,int decimals) {
    (void)decimals;if(v<0){*d++='-';v=-v;}fmt_int(d,v/10);while(*d)d++;*d++='.';*d++=(char)('0'+v%10);*d=0;
}
