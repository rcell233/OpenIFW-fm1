/* SPDX-License-Identifier: GPL-3.0-only */
#include "math.h"
#include "math_tables.h"
uint32_t ifw_udiv64(uint64_t n,uint32_t d) {
    uint32_t r=(uint32_t)(n>>32),lo=(uint32_t)n,q=0;
    if(!d || r>=d) return UINT32_MAX;
#ifdef IFW_TARGET
    /* pi32v2 has an exact 64/32 divide. Same overflow/zero contract. */
    (void)lo;(void)q;return (uint32_t)(n/d);
#else
    for(unsigned i=0;i<32;i++) {
        uint32_t carry=r>>31; r=(r<<1)|(lo>>31);lo<<=1;q<<=1;
        if(carry || r>=d) {r-=d;q|=1;}
    }
    return q;
#endif
}
int32_t ifw_sdiv64(int64_t n,uint32_t d) {
    uint32_t q=ifw_udiv64(n<0?0u-(uint64_t)n:(uint64_t)n,d);
    if(q>INT32_MAX)return n<0?INT32_MIN:INT32_MAX;
    return n<0?-(int32_t)q:(int32_t)q;
}
uint32_t ifw_ratio16(uint32_t n,uint32_t d) {
    if(!d) return 0;
    if(n>=d) return 65536;
    /* Same truncation as repeatedly shifting d, in one leading-zero count. */
    unsigned shift=d>65535u?16u-(unsigned)__builtin_clz(d):0u;
    uint32_t v=d>>shift;
    return (n<<(16-shift))/v;
}
static int32_t lerp(const int32_t *t,unsigned i,uint32_t frac,unsigned bits) {
    return t[i]+(int32_t)(((int64_t)(t[i+1]-t[i])*frac)>>bits);
}
int32_t ifw_sin20(uint32_t p) {return lerp(ifw_sine_table,p>>22,p&4194303u,22);}
int32_t ifw_triangle20(uint32_t p) {
    uint32_t quadrant=p>>30,offset=p&1073741823u;
    int32_t x=(int32_t)(offset>>10);
    return quadrant==0?x:quadrant==1?IFW_Q20-x:quadrant==2?-x:-IFW_Q20+x;
}
static int32_t blep(uint32_t p,uint32_t dt) {
    if(!dt || dt>=IFW_PHASE_HALF) return 0;
    if(p<dt) {
        int32_t u=(int32_t)ifw_ratio16(p,dt)*16;
        return u+u-ifw_mul20(u,u)-IFW_Q20;
    }
    if(p>0u-dt) {
        int32_t u=IFW_Q20-(int32_t)ifw_ratio16(0u-p,dt)*16;
        return ifw_mul20(u,u);
    }
    return 0;
}
int32_t ifw_wave20(uint32_t p,uint32_t dt,int type,uint32_t width) {
    if(type==0) return ifw_mul30(ifw_sin20(p),1265404740); /* 1.1785 */
    if(type==1) return ifw_mul30(ifw_triangle20(p),1265404740);
    if(type==2) return (int32_t)(p>>11)-IFW_Q20-blep(p,dt);
    if(type==3) return (p<width?IFW_Q20:-IFW_Q20)-((int32_t)(width>>11)-IFW_Q20)+blep(p,dt)-blep(p-width,dt);
    return 0;
}
void ifw_waves20(uint32_t p,uint32_t dt,int type,uint32_t width,
                 int32_t *saw,int32_t *secondary,int32_t *pulse) {
    /* The saw and shifted pulse use the same edge correction. Keep the same
     * Q20 operations/rounding and compute that correction once. */
    int32_t edge=blep(p,dt);
    *saw=(int32_t)(p>>11)-IFW_Q20-edge;
    uint32_t shifted=p+width;
    *pulse=(shifted<width?IFW_Q20:-IFW_Q20)-((int32_t)(width>>11)-IFW_Q20)+blep(shifted,dt)-edge;
    if(type==2)*secondary=*saw;
    else if(type==3)*secondary=(p<IFW_PHASE_HALF?IFW_Q20:-IFW_Q20)+edge-blep(p-IFW_PHASE_HALF,dt);
    else *secondary=ifw_wave20(p+(type<=1?IFW_PHASE_HALF:0),dt,type,IFW_PHASE_HALF);
}
uint64_t ifw_note_step(int32_t note,int32_t tuning) {
    int32_t semitone=note>>16,frac=note-semitone*65536;
    int octave=semitone/12,degree=semitone%12;
    if(degree<0) {degree+=12;octave--;}
    unsigned i=(unsigned)frac>>6;uint32_t f=(unsigned)frac&63u;
    uint32_t ratio=ifw_semitone_fraction[i]+(uint32_t)(((uint64_t)(ifw_semitone_fraction[i+1]-ifw_semitone_fraction[i])*f)>>6);
    uint64_t step=((uint64_t)ifw_note_base[degree]*ratio)>>30;
    tuning=ifw_clamp(tuning,300*65536,500*65536);
    uint32_t tune=(uint32_t)tuning/440u*16384u+((uint32_t)tuning%440u)*16384u/440u;
    step=(step*tune)>>30;
    int shift=octave-10;
    if(shift>20) return UINT64_C(0xFFFFFFFFFFFF);
    if(shift>=0) step<<=shift; else if(shift>-64) step>>=-shift; else step=0;
    return step;
}
uint32_t ifw_lfo_step(int32_t cv) {
    cv=ifw_clamp(cv,-20*65536,30*65536);
    int exponent=cv>>16;uint32_t f=(uint32_t)(cv-exponent*65536),i=f>>6;
    uint32_t ratio=ifw_octave_fraction[i]+(uint32_t)(((uint64_t)(ifw_octave_fraction[i+1]-ifw_octave_fraction[i])*(f&63u))>>6);
    uint64_t step=(UINT64_C(2042448811)*ratio)>>30; /* .02*2^32/44100, Q20 */
    if(exponent>=0) step<<=exponent;else step>>=-exponent;
    return (uint32_t)(step>>20);
}
uint32_t ifw_linear_step31(int32_t ms) {
    ms=ifw_clamp(ms,0,1000*65536);
    uint32_t frames=ifw_udiv64((uint64_t)(uint32_t)ms*441u,10u);
    if(frames<65536u) frames=65536u;
    return ifw_udiv64((uint64_t)INT32_MAX*65536u,frames);
}
uint32_t ifw_smooth31(int32_t ms) {
    uint32_t r=ifw_linear_step31(ms<6554?6554:ms);
    int64_t sum=r;uint32_t term=r;
    for(uint32_t k=2;k<=8;k++) {
        term=(uint32_t)(((uint64_t)term*r)>>31)/k;
        sum+=(k&1)?term:-(int64_t)term;
    }
    return (uint32_t)sum;
}
uint64_t ifw_glide_recip39_fine(int32_t speed) {
    speed=ifw_clamp(speed,0,11*16777216);unsigned i=(unsigned)speed>>16;
    if(i>=2816) return ifw_glide_recip[2816];
    int64_t delta=(int64_t)ifw_glide_recip[i+1]-ifw_glide_recip[i];
    return (uint64_t)((int64_t)ifw_glide_recip[i]+((delta*((unsigned)speed&65535u))>>16));
}
uint64_t ifw_glide_recip39(int32_t speed) {return ifw_glide_recip39_fine(ifw_clamp(speed,0,11*65536)*256);}
uint32_t ifw_glide_recip31(int32_t speed) {return (uint32_t)(ifw_glide_recip39(speed)>>8);}
int32_t ifw_filter_alpha30(int32_t x) {
    x=ifw_clamp(x,-8*65536,12*65536)+8*65536;
    unsigned i=(unsigned)x>>9;if(i>=2560) return ifw_alpha_table[2560];
    return lerp(ifw_alpha_table,i,(unsigned)x&511u,9);
}
int32_t ifw_filter3_a30(int32_t x) {
    x=ifw_clamp(x,-10*65536,20*65536)+10*65536;
    unsigned i=(unsigned)x>>9;if(i>=3840) return ifw_f3_table[3840];
    return lerp(ifw_f3_table,i,(unsigned)x&511u,9);
}
int32_t ifw_allpass_a30(int32_t hz,int half) {
    hz=ifw_clamp(hz,10*65536,22050*65536);unsigned i=(unsigned)hz>>20;
    const int32_t *t=half?ifw_ap_half_table:ifw_ap_table;
    if(i>=1379) return t[1379];
    return lerp(t,i,(unsigned)hz&1048575u,20);
}
int32_t ifw_soft20(int32_t x) {
    if(!x)return 0;
    uint32_t a=x<0?(uint32_t)(-(int64_t)x):(uint32_t)x;
    uint32_t d=65536u+(uint32_t)(((uint64_t)a*983u)>>20);
    uint32_t whole=a/d,rem=a-whole*d,frac=ifw_ratio16(rem,d);
    int32_t out=(int32_t)((whole<<16)+frac);
    return x<0?-out:out;
}
void ifw_eq_coeff(int32_t gain,int high,int32_t c[5]) {
    gain=ifw_clamp(gain,-12*65536,12*65536)+12*65536;
    uint32_t v=(uint32_t)gain*10u,i=v>>16,f=v&65535u;
    const int32_t *t=high?ifw_eq_high_table:ifw_eq_low_table;
    if(i>=240) {for(int k=0;k<5;k++) c[k]=t[1200+k];return;}
    for(int k=0;k<5;k++) c[k]=t[i*5+k]+(int32_t)(((int64_t)(t[(i+1)*5+k]-t[i*5+k])*f)>>16);
}
