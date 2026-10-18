/* SPDX-License-Identifier: GPL-3.0-only */
#include "envelope.h"
#include <limits.h>
#include "envelope_tables.h"
#include "decay_curve_q31.h"

static int cv(int value) { return value<0?0:value>1000?1000:value; }
void ifw_fixed_env_configure(IFWFixedEnvelope *e,int a,int d,int32_t sustain,int r,int retrigger) {
    e->attack_step=attack_step_q31[cv(a)]; e->decay_step=decay_step_q22[cv(d)];
    e->release_step=decay_step_q22[cv(r)]; e->sustain=sustain<0?0:sustain;
    e->retrigger=(uint8_t)!!retrigger;
}
static uint32_t cv_step(const uint32_t *t,int32_t value) {
    if(value<=0)return t[0];
    if(value>=655360)return t[1000];
    uint32_t x=(uint32_t)value*100u,i=x>>16,f=x&65535u;
    return t[i]+(int32_t)((((int64_t)t[i+1]-t[i])*f)>>16);
}
void ifw_fixed_env_configure_cv(IFWFixedEnvelope *e,int32_t a,int32_t d,int32_t sustain,int32_t r,int retrigger) {
    e->attack_step=cv_step(attack_step_q31,a);e->decay_step=cv_step(decay_step_q22,d);e->release_step=cv_step(decay_step_q22,r);
    e->sustain=sustain<0?0:sustain;e->retrigger=(uint8_t)!!retrigger;
}
uint32_t ifw_fixed_env_rate(int value) {
    int index=value<-1000?0:value>1000?2000:value+1000;
    return rate_q18[index];
}
uint32_t ifw_fixed_env_rate_cv(int32_t value) {
    if(value<=-655360) return rate_q18[0];
    if(value>=655360) return rate_q18[2000];
    uint32_t x=(uint32_t)(value+655360)*100u,i=x>>16,f=x&65535u;
    return rate_q18[i]+(uint32_t)(((uint64_t)(rate_q18[i+1]-rate_q18[i])*f)>>16);
}
void ifw_fixed_env_gate(IFWFixedEnvelope *e,int open,int legato) {
    if(open) { if(!legato || e->retrigger) { e->stage=1; e->progress=0; e->fraction=0; e->level=0; } }
    else if(e->stage!=0 && e->stage!=4) { e->stage=4; e->release_start=e->level; e->progress=0; e->fraction=0; }
}
static uint32_t advance(IFWFixedEnvelope *e,uint32_t step,uint32_t rate,uint32_t limit) {
    uint32_t progress=e->progress;
    uint64_t scaled=(uint64_t)step*rate+e->fraction;
    uint64_t delta=scaled>>18;e->fraction=(uint32_t)scaled&262143u;
    return delta>=limit-progress?limit:progress+(uint32_t)delta;
}
static int32_t decay(uint32_t progress) {
    uint32_t i=progress>>22,frac=progress&((1u<<22)-1u);
    if(i>=sizeof decay_curve_q31/sizeof *decay_curve_q31-1u) return 0;
    int32_t a=decay_curve_q31[i],b=decay_curve_q31[i+1];
    return a-(int32_t)(((int64_t)(a-b)*frac)>>22);
}
int32_t ifw_fixed_env_tick(IFWFixedEnvelope *e,uint32_t rate) {
    if(e->stage==1) {
        e->progress=advance(e,e->attack_step,rate,INT32_MAX); e->level=(int32_t)e->progress;
        if(e->progress==INT32_MAX) { e->stage=2; e->progress=0; e->fraction=0; }
    } else if(e->stage==2 || e->stage==4) {
        uint32_t limit=(uint32_t)(sizeof decay_curve_q31/sizeof *decay_curve_q31-1u)<<22;
        e->progress=advance(e,e->stage==2?e->decay_step:e->release_step,rate,limit);
        int32_t v=decay(e->progress);
        if(e->stage==2) { e->level=e->sustain+(int32_t)(((int64_t)(INT32_MAX-e->sustain)*v)>>31);
            if(!v) { e->stage=3; e->level=e->sustain; } }
        else { e->level=(int32_t)(((int64_t)e->release_start*v)>>31); if(!v) { e->stage=0; e->level=0; } }
    } else if(e->stage==3) e->level=e->sustain;
    return e->level;
}
