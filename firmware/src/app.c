/* SPDX-License-Identifier: GPL-3.0-only */
#define APP_NAME "OPENIFW FM1"
#if defined(IFW_EMU)
#include "../../web/emu/platform.c"
#elif defined(IFW_HOST)
#include "../../tests/panel_host.c"
#else
#define fm1_alnk0_irq ifw_platform_alnk0_irq
#include "platform.c"
#undef fm1_alnk0_irq
#endif
#include "fixed/synth.h"
#include "audio_perf.h"
#include "panel_params.h"
#include "presets.h"
#ifdef IFW_DSP_ISR_TEST
#include "dsp_irq_stream.h"
#endif
#ifdef IFW_DSP_STRESS
#include "dsp_stress.h"
#endif
#ifdef IFW_DSP_SELFTEST
#include "dsp_selftest.h"
#ifndef IFW_DSP_BASELINE
#include "dsp_math_test.h"
#else
static unsigned opt_math_checks(void) {return 0;}
#endif
static volatile uint32_t dsp_check[20];
#endif
#ifndef RING_PUBLISH
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#endif
#define PARAM_WORDS 229u
#define QSIZE 128u
static IFWFixedSynth synth;
static IFWFixedFxBuffers fx_buffers __attribute__((section(".pool")));
static IFWFixedPatch edit_patch;
static struct {uint32_t magic,version,checksum;IFWFixedPatch user[3];uint8_t octave,wind,channel,slot;int16_t learn[128];} saved;
_Static_assert(sizeof(IFWFixedPatch)==916,"patch layout changed");
_Static_assert(sizeof saved<=4096,"settings sector overflow");
typedef struct {uint32_t type;int32_t a,b;} AudioEvent;
static AudioEvent queue[QSIZE];
static volatile uint32_t qwrite,qread,overflow,ready;
static uint8_t patch_dirty;
static volatile struct {int32_t note,breath,gate,env[4];uint32_t held,quiet,max_us,overruns;} meter;
static volatile int16_t scope[96];
static uint8_t key_notes[27],midi_down[128],sustain_hold[128],sustain;
static uint16_t nrpn=16383,nrpn_value;static uint32_t quiet_frames;
static uint32_t key_prev,page,preset,home=1,dirty=1,last_control,learn_armed,save_pending,last_draw;
static const char *notice="SELECT: PAGES / ALG: MODULE";
#include "ui/state.h"
static int32_t word(const IFWFixedPatch *p,uint32_t id) {if(id==229)return saved.wind;if(id==230)return saved.channel;if(id==231)return saved.octave;if(id==232)return saved.slot;if(id==233)return (int32_t)meter.max_us;if(id==234)return (int32_t)meter.overruns;if(id==235)return FS;if(id==236)return 404;int32_t v;memcpy(&v,(const uint8_t *)p+id*4,4);return v;}
static void set_word(IFWFixedPatch *p,uint32_t id,int32_t v) {if(id<PARAM_WORDS)memcpy((uint8_t *)p+id*4,&v,4);}
static void enqueue(uint32_t type,int32_t a,int32_t b) {
    if(overflow)return;
    uint32_t w=qwrite;if(w-qread>=QSIZE) {overflow=1;return;}
    queue[w&(QSIZE-1)]=(AudioEvent){type,a,b};RING_PUBLISH();qwrite=w+1;
}
static int32_t midi31(uint32_t v) {return (int32_t)(v*16909320u);}
static void app_audio(int32_t *out,uint32_t n) {
    if(!ready)return;
#ifdef IFW_DSP_ISR_TEST
    irq_stream_render(&synth,out,n);return;
#endif
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    uint32_t start=fm1_ticks();
#endif
    uint32_t changed=0;
    if(overflow) {qread=qwrite;ifw_fixed_panic(&synth);ui_transfer_pending=0;RING_PUBLISH();overflow=0;}
    while(qread!=qwrite) {
        RING_PUBLISH();AudioEvent e=queue[qread&(QSIZE-1)];qread++;
        switch(e.type) {
        case 0:ifw_fixed_note_on(&synth,(uint32_t)e.a,e.b);break;
        case 1:ifw_fixed_note_off(&synth,(uint32_t)e.a);break;
        case 2:ifw_fixed_breath(&synth,e.a);break;
        case 3:set_word(&synth.patch,(uint32_t)e.a,e.b);changed=1;break;
        case 4:ifw_fixed_panic(&synth);break;
        case 5:if(e.a<(int32_t)FACTORY_COUNT)make_preset(&synth.patch,(uint32_t)e.a);else synth.patch=saved.user[e.a-FACTORY_COUNT];changed=1;break;
        case 6:synth.bend_target=e.a;break;case 7:synth.vibrato=e.a;break;case 8:synth.glide=e.a;break;
        case 9:synth.patch=ui_transfer;RING_PUBLISH();ui_transfer_pending=0;changed=1;break;
        }
    }
    if(changed)ifw_fixed_set_patch(&synth,&synth.patch);
    if(perf_mode) {
        for(uint32_t i=0;i<n;i++) {
            int32_t sample=perf_mode==1?ifw_sin20(perf_phase)/128:0;
            perf_phase+=42852281u; /* 440 Hz at 44.1 kHz, Q32 */
            out[2*i]=out[2*i+1]=sample;
        }
    }else ifw_fixed_process(&synth,out,n);
    if(n>=96)for(uint32_t i=0;i<96;i++)scope[i]=(int16_t)ifw_clamp(out[(i*n/96)*2],-32767,32767);
    meter.note=synth.note;meter.held=synth.mono.count;meter.breath=synth.breath;meter.gate=synth.gate;
    uint32_t quiet=!synth.gate_target && !synth.gate;
    for(int i=0;i<4;i++) {meter.env[i]=synth.env[i].level;if(synth.env[i].stage)quiet=0;}
    uint32_t peak=0,clips=0;
    for(uint32_t i=0;i<n*2;i++) {
        int32_t v=out[i];if(v>1 || v<-1)quiet=0;
        uint32_t a=(uint32_t)(v<0?-(int64_t)v:v);if(a>peak)peak=a;
        if(a>=32767u)clips++;
    }
    if(peak>audio_perf.peak)audio_perf.peak=peak;
    audio_perf.clips+=clips;
    quiet_frames=quiet?quiet_frames+n:0;if(quiet_frames>FS*2)quiet_frames=FS*2;
    meter.quiet=quiet_frames>=FS;
#if !defined(IFW_HOST) && !defined(IFW_EMU)
    uint32_t ticks=fm1_ticks()-start,us=ticks/FM1_TICKS_PER_US;
    audio_perf.dsp_ticks=ticks;
    if(ticks>audio_perf.dsp_max_ticks)audio_perf.dsp_max_ticks=ticks;
    if(us>meter.max_us)meter.max_us=us;
    if(us>5804)meter.overruns++;
#endif
}
static uint32_t app_crc(const void *data,uint32_t n) {
    const uint8_t *p=data;uint32_t c=UINT32_MAX;
    while(n--) {c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^((0u-(c&1u))&0xEDB88320u);}return ~c;
}
static void storage_load(void) {
    memset(&saved,0,sizeof saved);
    for(int i=0;i<3;i++)make_preset(&saved.user[i],0);
    for(int i=0;i<128;i++)saved.learn[i]=-1;
    saved.octave=3;
    static uint8_t raw[sizeof saved];
    if(!st_read(SETTINGS_AT,raw,sizeof raw)) {
        uint32_t h[3];memcpy(h,raw,12);
        if(h[0]==0x32465749u && h[1]==2 && h[2]==app_crc(raw+12,sizeof raw-12)) {
            memcpy(&saved,raw,sizeof saved);
            if(saved.octave>6)saved.octave=3;
            if(saved.channel>16)saved.channel=0;
            if(saved.slot>2)saved.slot=0;
            for(int i=0;i<128;i++)if(saved.learn[i]<-1 || saved.learn[i]>=233)saved.learn[i]=-1;
        }
    }
    ui_storage_load();
}
static int storage_save(void) {
    if(!flash_ok)return -1;
    saved.magic=0x32465749u;saved.version=2;saved.checksum=app_crc((const uint8_t *)&saved+12,sizeof saved-12);
    uint32_t took;if(fl_erase4k(SETTINGS_AT,&took))return -1;
    if(fl_write(SETTINGS_AT+12,(const uint8_t *)&saved+12,sizeof saved-12))return -1;
    if(fl_write(SETTINGS_AT,(const uint8_t *)&saved,12))return -1;
    return ui_storage_write();
}
static uint32_t control_id(uint32_t knob) {return ui_control_id(knob);}
static void edit_value(uint32_t id,int32_t movement,int absolute) {
    if(id>=233)return;
    const IFWPanelParam *p=&panel_params[id];int32_t value=word(&edit_patch,id);
    if(p->choice) {
        uint32_t count=option_count[p->choice];int32_t index=0;
        for(uint32_t i=0;i<count;i++)if(options[p->choice][i].value==value)index=(int32_t)i;
        index=absolute?movement*(int32_t)(count-1)/127:ifw_clamp(index+movement,0,(int32_t)count-1);
        value=options[p->choice][index].value;
    }else if(absolute)value=ifw_sat((int64_t)p->lo+ifw_sdiv64(((int64_t)p->hi-p->lo)*movement,127));
    else value=ifw_clamp(ifw_sat((int64_t)value+(int64_t)p->step*movement),p->lo,p->hi);
    if(id>=229) {
        if(id==229) {saved.wind=(uint8_t)value;enqueue(2,value?0:INT32_MAX,0);}
        if(id==230)saved.channel=(uint8_t)value;
        if(id==231)saved.octave=(uint8_t)value;
        if(id==232)saved.slot=(uint8_t)value;
    }else {set_word(&edit_patch,id,value);enqueue(3,(int32_t)id,value);patch_dirty=1;}
    last_control=id;dirty=1;
}
static void load_preset(uint32_t index) {
    ui_before_preset();patch_dirty=0;preset=index%PRESET_COUNT;if(preset<FACTORY_COUNT)make_preset(&edit_patch,preset);else edit_patch=saved.user[preset-FACTORY_COUNT];
    enqueue(5,(int32_t)preset,0);ui_after_preset();
}
static void panic_panel(void) {
    memset(midi_down,0,sizeof midi_down);memset(sustain_hold,0,sizeof sustain_hold);sustain=0;
    enqueue(4,0,0);ui_message("PANIC / ALL NOTES OFF");
}
static void note_start(uint32_t note,uint32_t velocity) {
    if(note>127)return;
    enqueue(0,(int32_t)note,midi31(velocity));
    if(!saved.wind)enqueue(2,INT32_MAX,0);
}
static void midi_message(uint8_t st,uint8_t a,uint8_t b) {
    if(saved.channel && (st&15u)+1u!=saved.channel)return;
    uint32_t type=st>>4;
    if(type==9 && b) {midi_down[a]=1;sustain_hold[a]=0;note_start(a,b);}
    else if(type==8 || type==9) {midi_down[a]=0;if(sustain)sustain_hold[a]=1;else enqueue(1,a,0);}
    else if(type==12) {for(uint32_t i=0;i<FACTORY_COUNT;i++)if(preset_programs[i]==a) {load_preset(i);break;}}
    else if(type==14)enqueue(6,((int32_t)a+(int32_t)b*128-8192)*262144,0);
    else if(type==11) {
        if(a==120 || a==123)panic_panel();
        else if(a==99)nrpn=(uint16_t)((nrpn&127u)|((uint16_t)b<<7));
        else if(a==98)nrpn=(uint16_t)((nrpn&16256u)|b);
        else if(a==6)nrpn_value=(uint16_t)((nrpn_value&127u)|((uint16_t)b<<7));
        else if(a==38 && nrpn<PARAM_WORDS) {
            nrpn_value=(uint16_t)((nrpn_value&16256u)|b);const IFWPanelParam *p=&panel_params[nrpn];int32_t v;
            if(p->choice) {uint32_t index=((uint32_t)nrpn_value*(option_count[p->choice]-1u)+8191u)/16383u;v=options[p->choice][index].value;}
            else v=ifw_sat((int64_t)p->lo+ifw_sdiv64(((int64_t)p->hi-p->lo)*nrpn_value,16383));
            set_word(&edit_patch,nrpn,v);enqueue(3,nrpn,v);patch_dirty=1;dirty=1;
        }
        else if(a==64) {uint8_t old=sustain;sustain=b>=64;if(old&&!sustain)for(int i=0;i<128;i++)if(sustain_hold[i]&&!midi_down[i]) {enqueue(1,i,0);sustain_hold[i]=0;}}
        else if(a==7)enqueue(2,midi31(b),0);
        else if(a==9)enqueue(7,midi31(b),0);
        else if(a==5)enqueue(8,midi31(b),0);
        else if(learn_armed) {saved.learn[a]=(int16_t)last_control;learn_armed=0;ui_message("CC LEARNED / SAVE TO KEEP");}
        else if(saved.learn[a]>=0)edit_value((uint32_t)saved.learn[a],b,1);
        else if(a>=20 && a<=23)edit_value(control_id(a-20),b,1);
    }
}
static void format_value(char *text,uint32_t id) {
    const IFWPanelParam *p=&panel_params[id];int32_t v=word(&edit_patch,id);
    if(p->choice) {
        const char *label="?";for(uint32_t i=0;i<option_count[p->choice];i++)if(options[p->choice][i].value==v)label=options[p->choice][i].label;
        str_cpy(text,label,32);
    }else if(id==233) {
#if defined(IFW_HOST) || defined(IFW_EMU)
        str_cpy(text,"HOST / N.A.",32);
#else
        fmt_int(text,v);
#endif
    }else if(id>=228)fmt_int(text,v);
    else fmt_fix(text,(int32_t)(((int64_t)v*10+(v<0?-32768:32768))/65536),1);
}
#include "ui/controls.c"
#include "ui/draw.c"
static void app_init(void) {
#ifdef IFW_DSP_SELFTEST
    /* Use full-address calls for target self-test entry points. */
    void (*volatile regression)(IFWFixedSynth *,IFWFixedFxBuffers *,uint32_t *)=opt_regression;
    unsigned (*volatile math_checks)(void)=opt_math_checks;
    uint32_t checks[16];regression(&synth,&fx_buffers,checks);
    for(unsigned i=0;i<16;i++)dsp_check[i+2]=checks[i];
    dsp_check[18]=math_checks();dsp_check[19]=1;
    dsp_check[1]=20;dsp_check[0]=0x54535749u;
#endif
    perf_mode=0;perf_phase=0;perf_reset();
    storage_load();make_preset(&edit_patch,0);
#ifdef IFW_DSP_STRESS
    perf_stress_patch(&edit_patch);
#endif
    ifw_fixed_init(&synth,&edit_patch,&fx_buffers);
#ifdef IFW_DSP_ISR_TEST
    irq_stream_init();
#endif
    RING_PUBLISH();ready=1;
#ifdef IFW_DSP_STRESS
    enqueue(2,INT32_MAX,0);enqueue(0,108,INT32_MAX);
#endif
    for(int i=0;i<27;i++)key_notes[i]=255;
    ui_init();lcd_fill(0,0,240,240,RGB(14,11,8));draw_panel(1);
}
/* One main-loop iteration, shared verbatim by the board and the WASM host. */
static void app_step(void) {
        uint32_t now=fm1_ms,gap=now-ui_poll_last;
        if(ui_poll_turns && gap>ui_poll_max_gap)ui_poll_max_gap=gap;
        ui_poll_last=now;ui_poll_turns++;
        uint32_t released=0,pressed=fm1_input_edges(&released);ui_poll(pressed,released);ui_encoders();
        uint32_t keys=fm1_in.notes,down=keys&~key_prev,up=key_prev&~keys;key_prev=keys;
        for(uint32_t k=0;k<27;k++) {
            if((down&(1u<<k))&&!ui_key(k)) {int32_t note=(int32_t)k+53+((int32_t)saved.octave-3)*12;key_notes[k]=(uint8_t)ifw_clamp(note,0,127);
                note_start(key_notes[k],110);midi_send(0x90,key_notes[k],110);}
            if((up&(1u<<k)) && key_notes[k]<128) {enqueue(1,key_notes[k],0);midi_send(0x80,key_notes[k],0);key_notes[k]=255;}
            fm1_led_key(KEY_LED(k),(int)(keys>>k&1u));
        }
        uint8_t st,a,b;while(midi_get(&st,&a,&b))midi_message(st,a,b);
        ui_tick();ui_leds();
        if(dirty || ui_drawing || fm1_ms-last_draw>=50) {draw_panel(dirty);dirty=0;}
}
static void app_main(void) {
    app_init();
    for(;;) {platform_poll();app_step();}
}
