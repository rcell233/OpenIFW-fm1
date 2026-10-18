/* SPDX-License-Identifier: GPL-3.0-only
 * Host reference only. Float32 delay storage mirrors OpenIFW, not FM-1 RAM.
 */
#include "internal.h"
#include <stdlib.h>

static int alloc_line(IFWDelay *d,uint32_t length) {
    d->length=length; d->data=calloc(length,sizeof(float)); return d->data!=NULL;
}
static void free_line(IFWDelay *d) { free(d->data); d->data=NULL; }
static double read_line(IFWDelay *d,double samples) {
    double p=fmod(d->head-clip(samples,1,d->length-2)+d->length,d->length);
    unsigned i=(unsigned)floor(p); double f=p-i;
    return d->data[i]*(1-f)+d->data[(i+1)%d->length]*f;
}
static void push(IFWDelay *d,double v) {
    d->data[d->head]=(float)(isfinite(v)?v:0); d->head=(d->head+1)%d->length;
}
static double comb_tick(IFWDelay *d,double input,double feedback,double damp) {
    double y=d->data[d->head]; d->store=y*(1-damp)+d->store*damp;
    d->data[d->head]=(float)(input+d->store*feedback); d->head=(d->head+1)%d->length; return y;
}
static double allpass_tick(IFWDelay *d,double input) {
    double b=d->data[d->head],y=b-input; d->data[d->head]=(float)(input+b*.5);
    d->head=(d->head+1)%d->length; return y;
}
static void shelf(IFWBiquad *b,double sr,double freq,double gain,int high) {
    double A=pow(10,gain/40),w=TAU*freq/sr,c=cos(w),alpha=sin(w)/sqrt(2),q=2*sqrt(A)*alpha,a0;
    if(high) {
        b->b0=A*(A+1+(A-1)*c+q); b->b1=-2*A*(A-1+(A+1)*c); b->b2=A*(A+1+(A-1)*c-q);
        a0=A+1-(A-1)*c+q; b->a1=2*(A-1-(A+1)*c); b->a2=A+1-(A-1)*c-q;
    } else {
        b->b0=A*(A+1-(A-1)*c+q); b->b1=2*A*(A-1-(A+1)*c); b->b2=A*(A+1-(A-1)*c-q);
        a0=A+1+(A-1)*c+q; b->a1=-2*(A-1+(A+1)*c); b->a2=A+1+(A-1)*c-q;
    }
    b->b0/=a0; b->b1/=a0; b->b2/=a0; b->a1/=a0; b->a2/=a0;
}
static double biquad_tick(IFWBiquad *b,double x) {
    double y=b->b0*x+b->z1; b->z1=b->b1*x-b->a1*y+b->z2; b->z2=b->b2*x-b->a2*y; return y;
}
int ifw_effects_init(IFWSynth *s) {
    IFWEffects *e=&s->effects; memset(e,0,sizeof *e);
    if(!alloc_line(&e->chorus,(uint32_t)ceil(s->sr*.1)) || !alloc_line(&e->enhancer,(uint32_t)ceil(s->sr*.1))) return 0;
    for(int i=0;i<4;i++) if(!alloc_line(&e->delay[i],(uint32_t)ceil(s->sr*2.05))) return 0;
    static const unsigned comb[]={1116,1188,1277,1356,1422,1491,1557,1617},diff[]={556,441,341,225};
    for(int ch=0;ch<2;ch++) {
        for(int i=0;i<8;i++) if(!alloc_line(&e->comb[ch][i],(unsigned)floor((comb[i]+ch*23)*s->sr/44100))) return 0;
        for(int i=0;i<4;i++) if(!alloc_line(&e->diff[ch][i],(unsigned)floor((diff[i]+ch*23)*s->sr/44100))) return 0;
        if(!alloc_line(&e->exciter[ch],(unsigned)ceil(s->sr*22/48000)+4)) return 0;
    }
    return 1;
}
void ifw_effects_destroy(IFWSynth *s) {
    IFWEffects *e=&s->effects; free_line(&e->chorus); free_line(&e->enhancer);
    for(int i=0;i<4;i++) free_line(&e->delay[i]);
    for(int ch=0;ch<2;ch++) {
        for(int i=0;i<8;i++) free_line(&e->comb[ch][i]);
        for(int i=0;i<4;i++) free_line(&e->diff[ch][i]);
        free_line(&e->exciter[ch]);
    }
}
void ifw_effects_configure(IFWSynth *s) {
    for(int ch=0;ch<2;ch++) {
        shelf(&s->effects.low[ch],s->sr,350,N(EqLowGain),0);
        shelf(&s->effects.high[ch],s->sr,6000,N(EqHighGain),1);
    }
}
static int active(IFWSynth *s,int i) { return !s->dry && s->p.fx[i][FX_Bypass]==0 && !s->p.fx_bypass[i]; }
void ifw_effects_tick(IFWSynth *s,double input,double *out_l,double *out_r) {
    IFWEffects *e=&s->effects; double sr=s->sr;
    double delay=clip(21-2*(N(ExciterFrequency)+nz(s->mod[P_ExciterFrequency])),1,21)*sr/48000;
    double g=.2*(N(ExciterMix)+nz(s->mod[P_ExciterMix]));
    double center=read_line(&e->exciter[0],delay),end=read_line(&e->exciter[1],delay);
    push(&e->exciter[0],input); push(&e->exciter[1],center);
    double x=(1+2*g)*center-g*(input+end),l=x,r=x;
    double mix=N(EnhancerMix)+nz(s->mod[P_EnhancerMix]);
    if(mix) { l+=read_line(&e->enhancer,N(EnhancerDelayL)*.001*sr)*(mix/10);
        r+=read_line(&e->enhancer,N(EnhancerDelayR)*.001*sr)*(mix/10); }
    push(&e->enhancer,x); e->phase=fmod(e->phase+1/sr,10000);
    if(active(s,0)) {
        double *q=s->p.fx[0],wet=q[FX_WetDry]/10,rate=q[FX_Speed],spread=q[FX_Stereo]/10;
        for(int ch=0;ch<2;ch++) {
            double freq=200+2500*(.5+.5*sin(TAU*e->phase*rate+ch*spread*acos(-1)));
            double gg=tan(acos(-1)*freq/sr),a=(gg-1)/(gg+1);
            double y=(ch?r:l)+e->phaser_fb[ch]*q[FX_Feedback]*.075;
            for(int k=0;k<6;k++) { int i=ch*6+k; double v=-a*y+e->phaser[i]; e->phaser[i]=y+a*v; y=v; }
            double low=clip(hz(12+10*q[FX_Lowpass],440),10,sr*.45);
            e->phaser_low[ch]+=(y-e->phaser_low[ch])*(1-exp(-TAU*low/sr)); y=e->phaser_low[ch];
            e->phaser_fb[ch]=soft(y); if(ch) r+=y*wet; else l+=y*wet;
        }
    }
    if(active(s,1)) {
        double *q=s->p.fx[1],amount=.0042*(q[FX_Depth]/10)*sr,base=.0016*sr+amount,m=q[FX_Mix]/10;
        double phase=fmod(q[FX_Rate]*1.00048*e->phase+s->p.chorus_phase/TAU,1);
        double triangle=(2/acos(-1))*asin(sin(TAU*phase));
        double cl=read_line(&e->chorus,base+amount*triangle),cr=read_line(&e->chorus,base-amount*triangle);
        push(&e->chorus,(l+r)*.5); l=l*(1-m*.5)+cl*m*.5; r=r*(1-m*.5)+cr*m*.5;
    } else push(&e->chorus,(l+r)*.5);
    for(int k=0;k<2;k++) {
        if(!active(s,k+2)) continue;
        double *q=s->p.fx[k+2],time=clip(q[FX_Time],.001,2)*sr,fb=clip(q[FX_Depth]/10,0,.96),wetmix=q[FX_WetDry]/10;
        double gl=tan(acos(-1)*clip(q[FX_Lowpass],10,sr*.45)/sr),gh=tan(acos(-1)*clip(q[FX_Highpass],10,sr*.45)/sr);
        double lp=gl/(1+gl),hp=gh/(1+gh);
        for(int ch=0;ch<2;ch++) {
            int i=k*2+ch; double d=read_line(&e->delay[i],time),v=(d-e->delay_lp[i])*lp,low=v+e->delay_lp[i];
            e->delay_lp[i]=low+v; double hv=(low-e->delay_hp[i])*hp,hlo=hv+e->delay_hp[i]; e->delay_hp[i]=hlo+hv;
            double wet=low-hlo,dry=ch?r:l; push(&e->delay[i],dry+soft(wet)*fb);
            if(ch) r=dry*(1-wetmix)+wet*wetmix; else l=dry*(1-wetmix)+wet*wetmix;
        }
    }
    if(active(s,4)) {
        double *q=s->p.fx[4],size=clip(q[FX_Size]/10,0,1),damp=q[FX_HighDamp]/10,width=q[FX_Width]/10,m=q[FX_WetDry]/10;
        double mono=(l+r)*.015,feedback=.7+size*.28,wl=0,wr=0;
        for(int i=0;i<8;i++) { wl+=comb_tick(&e->comb[0][i],mono,feedback,damp*.4); wr+=comb_tick(&e->comb[1][i],mono,feedback,damp*.4); }
        for(int i=0;i<4;i++) wl=allpass_tick(&e->diff[0][i],wl);
        for(int i=0;i<4;i++) wr=allpass_tick(&e->diff[1][i],wr);
        double wet1=m*2.25*(1+width),wet2=m*2.25*(1-width),dry=2-m;
        l=l*dry+wl*wet1+wr*wet2; r=r*dry+wr*wet1+wl*wet2;
    }
    l=biquad_tick(&e->high[0],biquad_tick(&e->low[0],l));
    r=biquad_tick(&e->high[1],biquad_tick(&e->low[1],r));
    *out_l=isfinite(l)?l:0; *out_r=isfinite(r)?r:0;
}
