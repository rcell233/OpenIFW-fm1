/* SPDX-License-Identifier: GPL-3.0-only
 * OpenIFW FX topology. Delays use 22.05 kHz/Q15; reverb storage uses Q15.
 */
#include "synth.h"
#include "memory.h"
#define V(k) (s->patch.v[P_##k])
#define M(k) (s->mod[P_##k])
static int32_t read32(const int32_t *b,uint32_t n,uint32_t head,uint32_t delay) {
    uint32_t max=(n-2u)<<16;if(delay<65536)delay=65536;if(delay>max)delay=max;
    uint32_t d=delay>>16,f=delay&65535u,i=(head+n-d)%n,j=(i+n-1u)%n;
    return ifw_sat((int64_t)b[i]+((((int64_t)b[j]-b[i])*f)>>16));
}
static int32_t read16(const int16_t *b,uint32_t n,uint32_t head,uint32_t delay) {
    uint32_t max=(n-2u)<<16;if(delay<65536)delay=65536;if(delay>max)delay=max;
    uint32_t d=delay>>16,f=delay&65535u,i=(head+n-d)%n,j=(i+n-1u)%n;
    int32_t a=b[i]*32,c=b[j]*32;return a+(int32_t)(((int64_t)(c-a)*f)>>16);
}
/* Truncate toward zero: arithmetic right shift fed -1 back forever in Q15
 * delay/reverb lines, leaving an audible DC tail after all envelopes closed. */
static int16_t q15(int32_t v) {return (int16_t)ifw_clamp(v/32,-32768,32767);}
static uint32_t advance(uint32_t head,uint32_t n) {return head+1u==n?0:head+1u;}
static int32_t cv20(int32_t v) {return ifw_sat((int64_t)(v/10)*16);}
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
void ifw_fixed_fx_configure(IFWFixedSynth *s) {
    IFWFixedEffects *e=&s->effects;
    ifw_eq_coeff(V(EqLowGain),0,e->eq_coeff[0]);ifw_eq_coeff(V(EqHighGain),1,e->eq_coeff[1]);
    /* log(Hz(12+10*CV)) = log(440)+(CV*10-57)*log(2)/12 */
    int32_t loghz=ifw_sat(398903+(((int64_t)(s->patch.fx[0][IFW_FX_Lowpass]-373555)*620217598)>>30));
    e->phaser_lowpass=ifw_filter_alpha30(loghz);
    for(int i=0;i<2;i++) {
        e->delay_lp_coeff[i]=(IFW_Q20+(ifw_allpass_a30(s->patch.fx[i+2][IFW_FX_Lowpass],1)>>10))/2;
        e->delay_hp_coeff[i]=(IFW_Q20+(ifw_allpass_a30(s->patch.fx[i+2][IFW_FX_Highpass],1)>>10))/2;
    }
}
static int32_t biquad(IFWFixedEffects *e,int ch,int k,int32_t x) {
    int32_t *c=e->eq_coeff[k],*z=e->eq_state[ch][k];
    /* Extra four fractional bits and unbiased rounding avoid low-shelf DC drift. */
    x=ifw_sat((int64_t)x*16);
    int32_t y=ifw_sat((((int64_t)c[0]*x+(1<<27))>>28)+z[0]);
    z[0]=ifw_sat((((int64_t)c[1]*x-(int64_t)c[3]*y+(1<<27))>>28)+z[1]);
    z[1]=ifw_sat(((int64_t)c[2]*x-(int64_t)c[4]*y+(1<<27))>>28);return y>>4;
}
void ifw_fixed_fx_tick(IFWFixedSynth *s,int32_t input) {
    IFWFixedEffects *e=&s->effects;IFWFixedFxBuffers *b=e->buffers;
    int32_t freq=ifw_sat((int64_t)V(ExciterFrequency)+M(ExciterFrequency));
    uint32_t delay=(uint32_t)ifw_clamp(ifw_sat(21*65536-(int64_t)freq*2),65536,21*65536)*147u/160u;
    int32_t g=ifw_sat((int64_t)(ifw_sat((int64_t)V(ExciterMix)+M(ExciterMix))/5)*16);
    int32_t center=read32(b->exciter[0],25,e->exciter_head[0],delay),end=read32(b->exciter[1],25,e->exciter_head[1],delay);
    b->exciter[0][e->exciter_head[0]]=input;b->exciter[1][e->exciter_head[1]]=center;
    for(int i=0;i<2;i++)e->exciter_head[i]=advance(e->exciter_head[i],25);
    int32_t x=ifw_sat((int64_t)center+ifw_mul20(ifw_sat((int64_t)center*2-input-end),g));
    int32_t l=x,r=x,mix=cv20(ifw_sat((int64_t)V(EnhancerMix)+M(EnhancerMix)));
    if(mix) {
        uint32_t dl=(uint32_t)ifw_clamp(V(EnhancerDelayL),0,99*65536)*441u/10u;
        uint32_t dr=(uint32_t)ifw_clamp(V(EnhancerDelayR),0,99*65536)*441u/10u;
        l=ifw_sat((int64_t)l+ifw_mul20(read32(b->enhancer,IFW_SHORT_SAMPLES,e->enhancer_head,dl),mix));
        r=ifw_sat((int64_t)r+ifw_mul20(read32(b->enhancer,IFW_SHORT_SAMPLES,e->enhancer_head,dr),mix));
    }
    b->enhancer[e->enhancer_head]=x;e->enhancer_head=advance(e->enhancer_head,IFW_SHORT_SAMPLES);
    e->time_phase+=97391u;e->time_remainder+=24196u;
    if(e->time_remainder>=44100u) {e->time_remainder-=44100u;e->time_phase++;}
    if(e->time_phase>=(UINT64_C(10000)<<32))e->time_phase-=UINT64_C(10000)<<32;
    if(active(s,0)) {
        int32_t *q=s->patch.fx[0],wet=cv20(q[IFW_FX_WetDry]);uint32_t phase=time_cycles(e,q[IFW_FX_Speed]);
        for(int ch=0;ch<2;ch++) {
            uint32_t spread=(uint32_t)q[IFW_FX_Stereo]*3276u;
            int32_t hz=1450*65536+ifw_mul16(ifw_sin20(phase+(ch?spread:0))>>4,1250*65536);
            int32_t a=ifw_allpass_a30(hz,0),y=ifw_sat((int64_t)(ch?r:l)+ifw_mul20(e->phaser_fb[ch],cv20(q[IFW_FX_Feedback])*3/4));
            for(int k=0;k<6;k++) {int i=ch*6+k;int32_t v=ifw_sat((int64_t)e->phaser[i]-ifw_mul30(y,a));
                e->phaser[i]=ifw_sat((int64_t)y+ifw_mul30(v,a));y=v;}
            e->phaser_low[ch]=ifw_sat((int64_t)e->phaser_low[ch]+((((int64_t)y-e->phaser_low[ch])*e->phaser_lowpass)>>30));
            y=e->phaser_low[ch];e->phaser_fb[ch]=ifw_soft20(y);
            if(ch)r=ifw_sat((int64_t)r+ifw_mul20(y,wet));else l=ifw_sat((int64_t)l+ifw_mul20(y,wet));
        }
    }
    if(active(s,1)) {
        int32_t *q=s->patch.fx[1];int32_t rate=ifw_sat(((int64_t)q[IFW_FX_Rate]*65567)>>16);
        uint32_t phase=time_cycles(e,rate)+(uint32_t)((int64_t)s->patch.chorus_phase*10430);
        int32_t triangle=ifw_triangle20(phase),amount=ifw_sat(((int64_t)q[IFW_FX_Depth]*1213858)>>16),base=4624220+amount;
        int32_t swing=ifw_mul20(amount,triangle),m=cv20(q[IFW_FX_Mix])/2;
        int32_t cl=read32(b->chorus,IFW_SHORT_SAMPLES,e->chorus_head,(uint32_t)(base+swing));
        int32_t cr=read32(b->chorus,IFW_SHORT_SAMPLES,e->chorus_head,(uint32_t)(base-swing));
        b->chorus[e->chorus_head]=ifw_sat(((int64_t)l+r)/2);
        l=ifw_sat((int64_t)ifw_mul20(l,IFW_Q20-m)+ifw_mul20(cl,m));
        r=ifw_sat((int64_t)ifw_mul20(r,IFW_Q20-m)+ifw_mul20(cr,m));
    }else b->chorus[e->chorus_head]=ifw_sat(((int64_t)l+r)/2);
    e->chorus_head=advance(e->chorus_head,IFW_SHORT_SAMPLES);
    for(int k=0;k<2;k++)if(active(s,k+2)) {
        int32_t *q=s->patch.fx[k+2],m=cv20(q[IFW_FX_WetDry]),fb=ifw_clamp(cv20(q[IFW_FX_Depth]),0,1006633);
        uint32_t d=(uint32_t)ifw_clamp(q[IFW_FX_Time],66,65536)*22050u;
        int32_t dry[2]={l,r};
        for(int ch=0;ch<2;ch++)e->delay_acc[k][ch]=ifw_sat((int64_t)e->delay_acc[k][ch]+dry[ch]);
        if(e->delay_half[k]) {
            for(int ch=0;ch<2;ch++) {
                int i=k*2+ch;int32_t v=read16(b->delay[k][ch],IFW_DELAY_SAMPLES,e->delay_head[k],d);
                int32_t t=ifw_mul20(ifw_sat((int64_t)v-e->delay_lp[i]),e->delay_lp_coeff[k]);
                int32_t low=ifw_sat((int64_t)t+e->delay_lp[i]);e->delay_lp[i]=ifw_sat((int64_t)low+t);
                t=ifw_mul20(ifw_sat((int64_t)low-e->delay_hp[i]),e->delay_hp_coeff[k]);
                int32_t hlo=ifw_sat((int64_t)t+e->delay_hp[i]);e->delay_hp[i]=ifw_sat((int64_t)hlo+t);
                e->delay_wet[i]=ifw_sat((int64_t)low-hlo);
                b->delay[k][ch][e->delay_head[k]]=q15(ifw_sat((int64_t)e->delay_acc[k][ch]/2+ifw_mul20(ifw_soft20(e->delay_wet[i]),fb)));
                e->delay_acc[k][ch]=0;
            }
            e->delay_head[k]=advance(e->delay_head[k],IFW_DELAY_SAMPLES);
        }
        e->delay_half[k]^=1;
        l=ifw_sat((int64_t)ifw_mul20(l,IFW_Q20-m)+ifw_mul20(e->delay_wet[k*2],m));
        r=ifw_sat((int64_t)ifw_mul20(r,IFW_Q20-m)+ifw_mul20(e->delay_wet[k*2+1],m));
    }
    if(active(s,4)) {
        static const uint16_t lengths[12]={1116,1188,1277,1356,1422,1491,1557,1617,556,441,341,225};
        int32_t *q=s->patch.fx[4],size=ifw_clamp(cv20(q[IFW_FX_Size]),0,IFW_Q20),damp=cv20(q[IFW_FX_HighDamp])*2/5;
        int32_t width=cv20(q[IFW_FX_Width]),m=cv20(q[IFW_FX_WetDry]);
        int32_t mono=ifw_sat(((int64_t)l+r)*15729>>20),feedback=734003+ifw_mul20(size,293601),wet[2]={0,0};
        for(int ch=0;ch<2;ch++) {
            uint32_t offset=0;
            for(int i=0;i<12;i++) {
                uint32_t n=lengths[i]+(uint32_t)ch*23u,head=e->reverb_head[ch][i];
                int16_t *line=b->reverb[ch]+offset;int32_t value=line[head]*32;
                if(i<8) {int32_t store=ifw_sat((int64_t)ifw_mul20(value,IFW_Q20-damp)+ifw_mul20(e->reverb_store[ch][i],damp));
                    e->reverb_store[ch][i]=store;line[head]=q15(ifw_sat((int64_t)mono+ifw_mul20(store,feedback)));
                    wet[ch]=ifw_sat((int64_t)wet[ch]+value);
                }else {int32_t in=wet[ch];wet[ch]=ifw_sat((int64_t)value-in);line[head]=q15(ifw_sat((int64_t)in+value/2));}
                e->reverb_head[ch][i]=advance(head,n);offset+=n;
            }
        }
        int32_t w1=ifw_mul20(ifw_sat((int64_t)m*9/4),IFW_Q20+width),w2=ifw_mul20(ifw_sat((int64_t)m*9/4),IFW_Q20-width),dry=2*IFW_Q20-m;
        l=ifw_sat((int64_t)ifw_mul20(l,dry)+ifw_mul20(wet[0],w1)+ifw_mul20(wet[1],w2));
        r=ifw_sat((int64_t)ifw_mul20(r,dry)+ifw_mul20(wet[1],w1)+ifw_mul20(wet[0],w2));
    }
    e->sample_left=biquad(e,0,1,biquad(e,0,0,l));e->sample_right=biquad(e,1,1,biquad(e,1,0,r));
}
