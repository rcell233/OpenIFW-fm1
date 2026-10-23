/* SPDX-License-Identifier: GPL-3.0-only */
#include "synth.h"
/* Original two-divide expression, independent of the reciprocal kernel. */
static int32_t opt_original_soft20(int32_t x) {
    if(!x)return 0;
    uint32_t a=x<0?(uint32_t)(-(int64_t)x):(uint32_t)x;
    uint32_t d=65536u+(uint32_t)(((uint64_t)a*983u)>>20);
    uint32_t q=a/d,rem=a-q*d;
    uint32_t (*volatile ratio16)(uint32_t,uint32_t)=ifw_ratio16;
    int32_t out=(int32_t)((q<<16)+ratio16(rem,d));
    return x<0?-out:out;
}
#ifdef IFW_TARGET
__attribute__((noinline))
#endif
static unsigned opt_math_checks(void) {
    /* Exercise full-address calls in the target test routine as well. */
    uint32_t (*volatile ratio16)(uint32_t,uint32_t)=ifw_ratio16;
    void (*volatile waves20)(uint32_t,uint32_t,int,uint32_t,int32_t *,int32_t *,int32_t *)=ifw_waves20;
    void (*volatile configure_blep)(IFWFixedBlep *,uint32_t)=ifw_blep_configure;
    void (*volatile cached_waves)(uint32_t,int,uint32_t,const IFWFixedBlep *,int32_t *,int32_t *,int32_t *)=ifw_waves20_cached;
    void (*volatile masked_waves)(uint32_t,int,uint32_t,const IFWFixedBlep *,unsigned,int32_t *,int32_t *,int32_t *)=ifw_waves20_masked;
    int32_t (*volatile saw_wave)(uint32_t,const IFWFixedBlep *)=ifw_saw20_cached;
    int32_t (*volatile pulse_wave)(uint32_t,uint32_t,const IFWFixedBlep *)=ifw_pulse20_cached;
    int32_t (*volatile soft20)(int32_t)=ifw_soft20;
    unsigned errors=0;uint32_t seed=0x5a123456u;
    if(ifw_udiv64(0,0)!=UINT32_MAX || ifw_udiv64(UINT64_MAX,UINT32_MAX)!=UINT32_MAX ||
       ifw_udiv64(UINT64_C(4294967296),2)!=2147483648u || ifw_udiv64(127,127)!=1)errors++;
    const int32_t edge[]={INT32_MIN,INT32_MIN+1,-1048576,-1,0,1,1048576,INT32_MAX-1,INT32_MAX};
    for(unsigned i=0;i<9;i++) {
        int32_t v=edge[i];
        if(soft20(v)!=opt_original_soft20(v))errors++;
        if(ifw_div5(v)!=v/5 || ifw_div10(v)!=v/10 || ifw_div20(v)!=v/20 || ifw_div100(v)!=v/100)errors++;
        for(unsigned n=0;n<31;n++)if(ifw_shr0(v,n)!=v/(int32_t)(1u<<n))errors++;
        if(ifw_shr0(v,31)!=(v==INT32_MIN? -1:0))errors++;
    }
    for(int32_t v=-10001;v<=10001;v++)
        if(ifw_div5(v)!=v/5 || ifw_div10(v)!=v/10 || ifw_div20(v)!=v/20 || ifw_div100(v)!=v/100)errors++;
    for(unsigned i=0;i<9;i++)for(unsigned j=0;j<9;j++) {
        int64_t x=(int64_t)edge[i]+edge[j];int32_t y=x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x;
        if(ifw_addsat(edge[i],edge[j])!=y)errors++;
        x=(int64_t)edge[i]-edge[j];y=x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x;
        if(ifw_subsat(edge[i],edge[j])!=y)errors++;
    }
    for(unsigned i=0;i<100000;i++) {
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;uint32_t phase=seed;
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;uint32_t step=seed;
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;uint32_t width=seed;
        int32_t v=(int32_t)phase;unsigned n=i%32;
        if(soft20(v)!=opt_original_soft20(v))errors++;
        int32_t alpha=(int32_t)(step%1073741825u),from=(int32_t)phase,to=(int32_t)width;
        int32_t interpolated=(int32_t)((int64_t)from+((((int64_t)to-from)*alpha)>>30));
        if(ifw_lerp30(from,to,alpha)!=interpolated)errors++;
        uint32_t frac=step&65535u;
        interpolated=(int32_t)((int64_t)from+((((int64_t)to-from)*frac)>>16));
        if(ifw_lerp16(from,to,frac)!=interpolated)errors++;
        /* The bare-metal build has no compiler-rt signed 64-bit divide. Its
         * existing software divider is independently checked by host tests. */
#ifdef IFW_TARGET
        int32_t shifted=ifw_sdiv64(v,1u<<n);
#else
        int32_t shifted=(int32_t)((int64_t)v/(INT64_C(1)<<n));
#endif
        if(ifw_div5(v)!=v/5 || ifw_div10(v)!=v/10 || ifw_div20(v)!=v/20 || ifw_div100(v)!=v/100 ||
           ifw_shr0(v,n)!=shifted ||
           ifw_udiv10(phase)!=phase/10u || ifw_udiv160(phase)!=phase/160u)errors++;
        int32_t a,b,c;int type=(int)(i%6)-1;
        uint32_t den=step;unsigned shift=0;while(den>65535u){den>>=1;shift++;}
        uint32_t ratio=!step?0:phase>=step?65536u:(phase<<(16-shift))/den;
        if(ratio16(phase,step)!=ratio)errors++;
        waves20(phase,step,type,width,&a,&b,&c);
        IFWFixedBlep cache;configure_blep(&cache,step);
        if(saw_wave(phase,&cache)!=a || pulse_wave(phase,width,&cache)!=c)errors++;
        int32_t ca,cb,cc;
        cached_waves(phase,type,width,&cache,&ca,&cb,&cc);
        if(a!=ca || b!=cb || c!=cc)errors++;
        unsigned mask=i%8u;ca=cb=cc=12345;
        masked_waves(phase,type,width,&cache,mask,&ca,&cb,&cc);
        if(ca!=((mask&1u)?a:12345) || cb!=((mask&2u)?b:12345) || cc!=((mask&4u)?c:12345))errors++;
        if(a!=ifw_wave20(phase,step,2,IFW_PHASE_HALF))errors++;
        if(b!=ifw_wave20(phase+(type<=1?IFW_PHASE_HALF:0),step,type,IFW_PHASE_HALF))errors++;
        if(c!=ifw_wave20(phase+width,step,3,width))errors++;
        int64_t x=(int64_t)((uint64_t)phase<<32|width);
        uint32_t rem=phase,lo=width,q=0;
        if(!step||rem>=step)q=UINT32_MAX;
        else for(unsigned k=0;k<32;k++) {
            uint32_t carry=rem>>31;rem=(rem<<1)|(lo>>31);lo<<=1;q<<=1;
            if(carry||rem>=step){rem-=step;q|=1;}
        }
        if(ifw_udiv64((uint64_t)x,step)!=q)errors++;
        int32_t y=x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x;
        if(ifw_sat(x)!=y)errors++;
        x=(int64_t)(int32_t)phase+width;
        if(ifw_half(x)!=(int32_t)(x/2))errors++;
    }
    return errors;
}
