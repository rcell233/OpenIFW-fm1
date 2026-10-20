/* SPDX-License-Identifier: GPL-3.0-only */
#include "synth.h"
#ifdef IFW_TARGET
__attribute__((noinline))
#endif
static unsigned opt_math_checks(void) {
    /* Calls to RAM functions use full addresses in this XIP test routine. */
    uint32_t (*volatile ratio16)(uint32_t,uint32_t)=ifw_ratio16;
    void (*volatile waves20)(uint32_t,uint32_t,int,uint32_t,int32_t *,int32_t *,int32_t *)=ifw_waves20;
    unsigned errors=0;uint32_t seed=0x5a123456u;
    if(ifw_udiv64(0,0)!=UINT32_MAX || ifw_udiv64(UINT64_MAX,UINT32_MAX)!=UINT32_MAX ||
       ifw_udiv64(UINT64_C(4294967296),2)!=2147483648u || ifw_udiv64(127,127)!=1)errors++;
    const int32_t edge[]={INT32_MIN,INT32_MIN+1,-1048576,-1,0,1,1048576,INT32_MAX-1,INT32_MAX};
    for(unsigned i=0;i<9;i++) {
        int32_t v=edge[i];
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
