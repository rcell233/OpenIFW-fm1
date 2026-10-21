/* SPDX-License-Identifier: GPL-3.0-only
 * OpenIFW FX topology. Delays use 22.05 kHz/Q15; reverb storage uses Q15.
 */
#include "synth.h"
#include "memory.h"
#define V(k) (s->patch.v[P_##k])
#define M(k) (s->mod[P_##k])
static int32_t read32(const int32_t *b,uint32_t n,uint32_t head,uint32_t delay) {
    uint32_t max=(n-2u)<<16;if(delay<65536)delay=65536;if(delay>max)delay=max;
    uint32_t d=delay>>16,f=delay&65535u,i=head+n-d;
    if(i>=n)i-=n;
    uint32_t j=i?i-1u:n-1u;
    return ifw_sat((int64_t)b[i]+((((int64_t)b[j]-b[i])*f)>>16));
}
static int32_t read16(const int16_t *b,uint32_t n,uint32_t head,uint32_t delay) {
    uint32_t max=(n-2u)<<16;if(delay<65536)delay=65536;if(delay>max)delay=max;
    uint32_t d=delay>>16,f=delay&65535u,i=head+n-d;
    if(i>=n)i-=n;
    uint32_t j=i?i-1u:n-1u;
    int32_t a=b[i]*32,c=b[j]*32;return a+(int32_t)(((int64_t)(c-a)*f)>>16);
}
/* Truncate toward zero: arithmetic right shift fed -1 back forever in Q15
 * delay/reverb lines, leaving an audible DC tail after all envelopes closed. */
static int16_t q15(int32_t v) {return (int16_t)ifw_clamp(ifw_shr0(v,5),-32768,32767);}
static uint32_t advance(uint32_t head,uint32_t n) {return head+1u==n?0:head+1u;}
static int32_t cv20(int32_t v) {return ifw_sat((int64_t)ifw_div10(v)*16);}
static int active(IFWFixedSynth *s,int i) {return !s->dry && !s->patch.fx[i][IFW_FX_Bypass] && !(s->patch.fx_bypass&(1u<<i));}
static uint32_t time_cycles(IFWFixedEffects *e,int32_t rate) {
    uint32_t lo=(uint32_t)e->time_phase,hi=(uint32_t)(e->time_phase>>32);
    return (uint32_t)(((uint64_t)lo*(uint32_t)rate)>>16)+(hi*(uint32_t)rate<<16);
}
void ifw_fixed_fx_reset(IFWFixedSynth *s) {
    IFWFixedFxBuffers *b=s->effects.buffers;
    memset(&s->effects,0,sizeof s->effects);s->effects.buffers=b;
    if(b)memset(b,0,sizeof *b);
}
void ifw_fixed_fx_mod_configure(IFWFixedSynth *s) {
    IFWFixedEffects *e=&s->effects;
    int32_t freq=ifw_addsat(V(ExciterFrequency),M(ExciterFrequency));
    e->exciter_delay=ifw_udiv160((uint32_t)ifw_clamp(ifw_sat(21*65536-(int64_t)freq*2),65536,21*65536)*147u);
    e->exciter_gain=ifw_sat((int64_t)ifw_div5(ifw_addsat(V(ExciterMix),M(ExciterMix)))*16);
    e->enhancer_mix=cv20(ifw_addsat(V(EnhancerMix),M(EnhancerMix)));
}
void ifw_fixed_fx_configure(IFWFixedSynth *s) {
    IFWFixedEffects *e=&s->effects;
    ifw_fixed_fx_mod_configure(s);
    ifw_eq_coeff(V(EqLowGain),0,e->eq_coeff[0]);ifw_eq_coeff(V(EqHighGain),1,e->eq_coeff[1]);
    for(int k=0;k<2;k++) {
        int32_t *c=e->eq_coeff[k];
        e->eq_identity[k]=(uint8_t)(c[0]==268435456 && c[1]==c[3] && c[2]==c[4]);
    }
    /* log(Hz(12+10*CV)) = log(440)+(CV*10-57)*log(2)/12 */
    int32_t loghz=ifw_sat(398903+(((int64_t)(s->patch.fx[0][IFW_FX_Lowpass]-373555)*620217598)>>30));
    e->phaser_lowpass=ifw_filter_alpha30(loghz);
    e->enhancer_delay[0]=ifw_udiv10((uint32_t)ifw_clamp(V(EnhancerDelayL),0,99*65536)*441u);
    e->enhancer_delay[1]=ifw_udiv10((uint32_t)ifw_clamp(V(EnhancerDelayR),0,99*65536)*441u);
    const int32_t *ph=s->patch.fx[0],*ch=s->patch.fx[1],*rv=s->patch.fx[4];
    e->phaser_rate=ph[IFW_FX_Speed];e->phaser_wet=cv20(ph[IFW_FX_WetDry]);
    e->phaser_spread=(uint32_t)ph[IFW_FX_Stereo]*3276u;
    e->phaser_feedback=cv20(ph[IFW_FX_Feedback])*3/4;
    e->chorus_rate=ifw_sat(((int64_t)ch[IFW_FX_Rate]*65567)>>16);
    e->chorus_offset=(uint32_t)((int64_t)s->patch.chorus_phase*10430);
    e->chorus_amount=ifw_sat(((int64_t)ch[IFW_FX_Depth]*1213858)>>16);
    e->chorus_base=4624220+e->chorus_amount;e->chorus_mix=cv20(ch[IFW_FX_Mix])/2;
    int32_t size=ifw_clamp(cv20(rv[IFW_FX_Size]),0,IFW_Q20);
    int32_t width=cv20(rv[IFW_FX_Width]),mix=cv20(rv[IFW_FX_WetDry]);
    e->reverb_damp=cv20(rv[IFW_FX_HighDamp])*2/5;
    e->reverb_feedback=734003+ifw_mul20(size,293601);
    e->reverb_w1=ifw_mul20(ifw_sat((int64_t)mix*9/4),IFW_Q20+width);
    e->reverb_w2=ifw_mul20(ifw_sat((int64_t)mix*9/4),IFW_Q20-width);
    e->reverb_dry=2*IFW_Q20-mix;
    for(int i=0;i<2;i++) {
        e->delay_lp_coeff[i]=(IFW_Q20+(ifw_allpass_a30(s->patch.fx[i+2][IFW_FX_Lowpass],1)>>10))/2;
        e->delay_hp_coeff[i]=(IFW_Q20+(ifw_allpass_a30(s->patch.fx[i+2][IFW_FX_Highpass],1)>>10))/2;
        e->delay_mix[i]=cv20(s->patch.fx[i+2][IFW_FX_WetDry]);
        e->delay_feedback[i]=ifw_clamp(cv20(s->patch.fx[i+2][IFW_FX_Depth]),0,1006633);
        e->delay_time[i]=(uint32_t)ifw_clamp(s->patch.fx[i+2][IFW_FX_Time],66,65536)*22050u;
    }
}
static int32_t biquad(IFWFixedEffects *e,int ch,int k,int32_t x) {
    int32_t *c=e->eq_coeff[k],*z=e->eq_state[ch][k];
    /* Extra four fractional bits and unbiased rounding avoid low-shelf DC drift. */
    x=ifw_sat((int64_t)x*16);
    /* Exact identity, including the input saturation and retained old state. */
    if(e->eq_identity[k] && !z[0] && !z[1])return x>>4;
    int32_t y=ifw_sat((((int64_t)c[0]*x+(1<<27))>>28)+z[0]);
    z[0]=ifw_sat((((int64_t)c[1]*x-(int64_t)c[3]*y+(1<<27))>>28)+z[1]);
    z[1]=ifw_sat(((int64_t)c[2]*x-(int64_t)c[4]*y+(1<<27))>>28);return y>>4;
}
void ifw_fixed_fx_tick(IFWFixedSynth *s,int32_t input) {
    IFWFixedEffects *e=&s->effects;IFWFixedFxBuffers *b=e->buffers;
    uint32_t delay=e->exciter_delay;int32_t g=e->exciter_gain;
    int32_t center=read32(b->exciter[0],25,e->exciter_head[0],delay),end=read32(b->exciter[1],25,e->exciter_head[1],delay);
    b->exciter[0][e->exciter_head[0]]=input;b->exciter[1][e->exciter_head[1]]=center;
    for(int i=0;i<2;i++)e->exciter_head[i]=advance(e->exciter_head[i],25);
    int32_t x=ifw_sat((int64_t)center+ifw_mul20(ifw_sat((int64_t)center*2-input-end),g));
    int32_t l=x,r=x,mix=e->enhancer_mix;
    if(mix) {
        uint32_t dl=e->enhancer_delay[0],dr=e->enhancer_delay[1];
        l=ifw_addsat(l,ifw_mul20(read32(b->enhancer,IFW_SHORT_SAMPLES,e->enhancer_head,dl),mix));
        r=ifw_addsat(r,ifw_mul20(read32(b->enhancer,IFW_SHORT_SAMPLES,e->enhancer_head,dr),mix));
    }
    b->enhancer[e->enhancer_head]=x;e->enhancer_head=advance(e->enhancer_head,IFW_SHORT_SAMPLES);
    e->time_phase+=97391u;e->time_remainder+=24196u;
    if(e->time_remainder>=44100u) {e->time_remainder-=44100u;e->time_phase++;}
    if(e->time_phase>=(UINT64_C(10000)<<32))e->time_phase-=UINT64_C(10000)<<32;
    if(active(s,0)) {
        int32_t wet=e->phaser_wet;uint32_t phase=time_cycles(e,e->phaser_rate);
        for(int ch=0;ch<2;ch++) {
            uint32_t spread=e->phaser_spread;
            int32_t hz=1450*65536+ifw_mul16(ifw_sin20(phase+(ch?spread:0))>>4,1250*65536);
            int32_t a=ifw_allpass_a30(hz,0),y=ifw_addsat(ch?r:l,ifw_mul20(e->phaser_fb[ch],e->phaser_feedback));
            for(int k=0;k<6;k++) {int i=ch*6+k;int32_t v=ifw_subsat(e->phaser[i],ifw_mul30_bounded(y,a));
                e->phaser[i]=ifw_addsat(y,ifw_mul30_bounded(v,a));y=v;}
            e->phaser_low[ch]=ifw_lerp30(e->phaser_low[ch],y,e->phaser_lowpass);
            y=e->phaser_low[ch];e->phaser_fb[ch]=ifw_soft20(y);
            if(ch)r=ifw_addsat(r,ifw_mul20(y,wet));else l=ifw_addsat(l,ifw_mul20(y,wet));
        }
    }
    if(active(s,1)) {
        uint32_t phase=time_cycles(e,e->chorus_rate)+e->chorus_offset;
        int32_t triangle=ifw_triangle20(phase),base=e->chorus_base;
        int32_t swing=ifw_mul20(e->chorus_amount,triangle),m=e->chorus_mix;
        int32_t cl=read32(b->chorus,IFW_SHORT_SAMPLES,e->chorus_head,(uint32_t)(base+swing));
        int32_t cr=read32(b->chorus,IFW_SHORT_SAMPLES,e->chorus_head,(uint32_t)(base-swing));
        b->chorus[e->chorus_head]=ifw_half((int64_t)l+r);
        l=ifw_addsat(ifw_mul20(l,IFW_Q20-m),ifw_mul20(cl,m));
        r=ifw_addsat(ifw_mul20(r,IFW_Q20-m),ifw_mul20(cr,m));
    }else b->chorus[e->chorus_head]=ifw_half((int64_t)l+r);
    e->chorus_head=advance(e->chorus_head,IFW_SHORT_SAMPLES);
    for(int k=0;k<2;k++)if(active(s,k+2)) {
        int32_t m=e->delay_mix[k],fb=e->delay_feedback[k];uint32_t d=e->delay_time[k];
        int32_t dry[2]={l,r};
        for(int ch=0;ch<2;ch++)e->delay_acc[k][ch]=ifw_addsat(e->delay_acc[k][ch],dry[ch]);
        if(e->delay_half[k]) {
            for(int ch=0;ch<2;ch++) {
                int i=k*2+ch;int32_t v=read16(b->delay[k][ch],IFW_DELAY_SAMPLES,e->delay_head[k],d);
                int32_t t=ifw_mul20_bounded(ifw_sat((int64_t)v-e->delay_lp[i]),e->delay_lp_coeff[k]);
                int32_t low=ifw_addsat(t,e->delay_lp[i]);e->delay_lp[i]=ifw_addsat(low,t);
                t=ifw_mul20_bounded(ifw_sat((int64_t)low-e->delay_hp[i]),e->delay_hp_coeff[k]);
                int32_t hlo=ifw_addsat(t,e->delay_hp[i]);e->delay_hp[i]=ifw_addsat(hlo,t);
                e->delay_wet[i]=ifw_sat((int64_t)low-hlo);
                b->delay[k][ch][e->delay_head[k]]=q15(ifw_addsat(ifw_half(e->delay_acc[k][ch]),ifw_mul20_bounded(ifw_soft20(e->delay_wet[i]),fb)));
                e->delay_acc[k][ch]=0;
            }
            e->delay_head[k]=advance(e->delay_head[k],IFW_DELAY_SAMPLES);
        }
        e->delay_half[k]^=1;
        l=ifw_addsat(ifw_mul20(l,IFW_Q20-m),ifw_mul20(e->delay_wet[k*2],m));
        r=ifw_addsat(ifw_mul20(r,IFW_Q20-m),ifw_mul20(e->delay_wet[k*2+1],m));
    }
    if(active(s,4)) {
        int32_t damp=e->reverb_damp,feedback=e->reverb_feedback;
        /* l/r are int32: this downmix is bounded to < 65 million. */
        int32_t mono=(int32_t)((((int64_t)l+r)*15729)>>20),wet[2]={0,0};
        int unit_damp=damp>=0 && damp<=IFW_Q20;
        /* Fixed topology, same cell order and per-product rounding. Constants
         * remove the per-cell length/offset/index bookkeeping. Each Q15 line
         * read is <=Q20; the eight-comb/four-allpass wet sum is <13*Q20. */
#define COMB(CH,I,N,OFF) do { \
        uint32_t head=e->reverb_head[CH][I]; \
        int16_t *line=b->reverb[CH]+(OFF);int32_t value=line[head]*32; \
        int32_t store=unit_damp? \
            ifw_addsat(ifw_mul20_bounded(value,IFW_Q20-damp),ifw_mul20_bounded(e->reverb_store[CH][I],damp)): \
            ifw_addsat(ifw_mul20(value,IFW_Q20-damp),ifw_mul20(e->reverb_store[CH][I],damp)); \
        e->reverb_store[CH][I]=store; \
        line[head]=q15(ifw_addsat(mono,ifw_mul20_bounded(store,feedback))); \
        wet[CH]+=value;e->reverb_head[CH][I]=advance(head,N); \
        } while(0)
#define ALLPASS(CH,I,N,OFF) do { \
        uint32_t head=e->reverb_head[CH][I]; \
        int16_t *line=b->reverb[CH]+(OFF);int32_t value=line[head]*32,in=wet[CH]; \
        wet[CH]=value-in;line[head]=q15(in+value/2); \
        e->reverb_head[CH][I]=advance(head,N); \
        } while(0)
        COMB(0,0,1116,0);
        COMB(0,1,1188,1116);
        COMB(0,2,1277,2304);
        COMB(0,3,1356,3581);
        COMB(0,4,1422,4937);
        COMB(0,5,1491,6359);
        COMB(0,6,1557,7850);
        COMB(0,7,1617,9407);
        ALLPASS(0,8,556,11024);
        ALLPASS(0,9,441,11580);
        ALLPASS(0,10,341,12021);
        ALLPASS(0,11,225,12362);
        COMB(1,0,1139,0);
        COMB(1,1,1211,1139);
        COMB(1,2,1300,2350);
        COMB(1,3,1379,3650);
        COMB(1,4,1445,5029);
        COMB(1,5,1514,6474);
        COMB(1,6,1580,7988);
        COMB(1,7,1640,9568);
        ALLPASS(1,8,579,11208);
        ALLPASS(1,9,464,11787);
        ALLPASS(1,10,364,12251);
        ALLPASS(1,11,248,12615);
#undef COMB
#undef ALLPASS
        int32_t w1=e->reverb_w1,w2=e->reverb_w2,dry=e->reverb_dry;
        l=ifw_sat((int64_t)ifw_mul20(l,dry)+ifw_mul20(wet[0],w1)+ifw_mul20(wet[1],w2));
        r=ifw_sat((int64_t)ifw_mul20(r,dry)+ifw_mul20(wet[1],w1)+ifw_mul20(wet[0],w2));
    }
    e->sample_left=biquad(e,0,1,biquad(e,0,0,l));e->sample_right=biquad(e,1,1,biquad(e,1,0,r));
}
