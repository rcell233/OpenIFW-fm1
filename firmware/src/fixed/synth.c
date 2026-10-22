/* SPDX-License-Identifier: GPL-3.0-only
 * Integer port of OpenIFW a478abd. Audio is Q20, control CV is Q16, gates Q31.
 */
#include "synth.h"
#include "patch_defaults.h"
#include "memory.h"
#define V(k) (s->patch.v[P_##k])
#define O(i,k) (s->patch.v[P_Osc1##k+(i)*IFW_OSC_STRIDE])
#define F(i,k) (s->patch.v[P_Filter1##k+(i)*IFW_FILTER_STRIDE])
#define E(i,k) (s->patch.v[P_Eg1##k+(i)*IFW_EG_STRIDE])
#define L(i,k) (s->patch.v[P_Lfo1##k+(i)*IFW_LFO_STRIDE])
static int32_t level20(int32_t x) {return ifw_div10(ifw_clamp(x,0,655360)*16);}
static void clear_mod(IFWFixedSynth *s) {
    /* The platform's general memset writes bytes. These are aligned words;
     * volatile prevents the compiler turning this back into a byte memset. */
    volatile int32_t *p=s->mod;
    for(unsigned i=0;i<IFW_MOD_COUNT;i++)p[i]=0;
}
static uint64_t cached_step(IFWFixedSynth *s,unsigned i,int32_t pitch) {
    if(!s->step_valid[i] || s->step_pitch[i]!=pitch || s->step_tuning[i]!=s->patch.tuning) {
        s->step_value[i]=ifw_note_step(pitch,s->patch.tuning);
        s->step_pitch[i]=pitch;s->step_tuning[i]=s->patch.tuning;s->step_valid[i]=1;
    }
    return s->step_value[i];
}
static int32_t source_uncached(IFWFixedSynth *s,uint8_t code) {
    int b=code&128;unsigned id=code&127;int32_t v=0;
    if(id==IFW_SRC_NONE)return IFW_Q20;
    if(id==IFW_SRC_BREATH) {v=s->breath>>11;return b?v*2-IFW_Q20:v;}
    if(id==IFW_SRC_VELOCITY) {v=s->velocity>>11;return b?2*(v-528416):v;}
    if(id==IFW_SRC_NOTE) {
        if(b)return ifw_sdiv64(((int64_t)s->pitch-64*65536)*32,127);
        return s->note*IFW_Q20/127;
    }
    if(id==IFW_SRC_BEND)return s->bend>>11;
    if(id==IFW_SRC_VIBRATO)return s->vibrato>>11;
    if(id==IFW_SRC_GLIDE)return s->glide>>11;
    if(id==IFW_SRC_GATE)return s->gate>>11;
    if(id>=IFW_SRC_EG && id<IFW_SRC_EG+4) {v=s->env[id-IFW_SRC_EG].level>>11;return b?v*2-IFW_Q20:v;}
    if(id>=IFW_SRC_LFO && id<IFW_SRC_LFO+2) {
        unsigned i=id-IFW_SRC_LFO;int type=L(i,Waveform);
        v=ifw_wave20(s->lfo_phase[i],0,type,IFW_PHASE_HALF);
        if(type==1)v=ifw_mul30(v,911108888); /* 1/1.1785 */
        return b?v:ifw_shr0(v+IFW_Q20,1);
    }
    if(id>=IFW_SRC_SAW && id<IFW_SRC_WAVE) v=s->raw_saw[id-IFW_SRC_SAW];
    else if(id>=IFW_SRC_WAVE && id<IFW_SRC_PWM) v=s->secondary_cv[id-IFW_SRC_WAVE];
    else if(id>=IFW_SRC_PWM && id<IFW_SRC_NOISE) v=s->raw_pulse[id-IFW_SRC_PWM];
    else if(id==IFW_SRC_NOISE)v=s->noise_value;
    else return 0;
    return b?v:ifw_shr0(v+IFW_Q20,1);
}
static int32_t source20(IFWFixedSynth *s,uint8_t code) {
    unsigned id=code&127u;
    if(!id)return IFW_Q20;
    /* These controls do not change between the two oscillator subsamples.
     * Raw oscillator/noise CVs remain uncached and keep their exact order. */
    if(id>=IFW_SRC_SAW)return source_uncached(s,code);
    unsigned i=id+((code&128u)?14u:0u);uint32_t bit=1u<<i;
    if(!(s->source_valid&bit)) {
        s->source_cache[i]=source_uncached(s,code);s->source_valid|=bit;
    }
    return s->source_cache[i];
}
static int32_t breath_source31(IFWFixedSynth *s,int selector) {
    return selector>=1 && selector<=4?(int32_t)(((int64_t)s->breath*s->env[selector-1].level)>>31):s->breath;
}
static int32_t log_cutoff(IFWFixedSynth *s,int i,int32_t note) {
    int32_t f=ifw_sat((int64_t)F(i,Frequency)+s->mod[P_Filter1Frequency+i*IFW_FILTER_STRIDE]);
    int32_t depth=(int32_t)(((int64_t)F(i,Breath)*breath_source31(s,F(i,BreathSubMod)))>>31);
    int32_t key=ifw_mul16((note>>2)-15*65536,F(i,KeyTrack));
    int64_t result=113648; /* ln(5.663990017612683), Q16 */
    result+=((int64_t)f*991758428)>>30;
    result+=((int64_t)depth*897177250)>>30;
    result+=((int64_t)key*30425201)>>30;
    result-=((int64_t)F(i,KeyTrack)*299441)>>30;
    return ifw_sat(result);
}
static void configure_rates(IFWFixedSynth *s) {
    for(int i=0;i<4;i++)s->env_rate[i]=ifw_fixed_env_rate_cv(s->mod[IFW_MOD_EG+i]);
    for(int i=0;i<2;i++) {
        if(L(i,BPMSync)||L(i,BpmSync)) {
            uint64_t den=(uint64_t)(uint32_t)ifw_clamp(L(i,Beat),1024,16*65536)*88200u;
            uint64_t num=UINT64_C(281474976710656);
            while(den>UINT32_MAX) {den>>=1;num>>=1;}
            s->lfo_step[i]=ifw_udiv64(num,(uint32_t)den);
        }else s->lfo_step[i]=ifw_lfo_step(ifw_sat((int64_t)L(i,Speed)+s->mod[IFW_MOD_LFO+i]));
    }
}
static void configure_filter3(IFWFixedSynth *s) {
    IFWFixedFilter3 *f=&s->filter3;
    f->enabled=(uint8_t)!!V(Filter3Enabled);
    f->stages=(uint8_t)ifw_clamp((V(Filter3Stage)+32768)>>16,2,24);
    f->mix=ifw_sat((int64_t)(ifw_div10(ifw_sat((int64_t)V(Filter3Mix)+s->mod[P_Filter3Mix])))*16);
    f->feedback=ifw_clamp(ifw_sat((int64_t)(ifw_div10(ifw_sat((int64_t)V(Filter3Resonance)+s->mod[P_Filter3Resonance])))*16),-1047527,1047527);
    int32_t frequency=ifw_sat((int64_t)V(Filter3Frequency)+s->mod[P_Filter3Frequency]);
    if(!s->filter3_valid || s->filter3_frequency!=frequency) {
        f->a=ifw_filter3_a30(frequency);s->filter3_frequency=frequency;s->filter3_valid=1;
    }
}
static void configure_oscillators(IFWFixedSynth *s,int32_t note) {
    for(int i=0;i<4;i++) {
        int32_t pitch=ifw_sat((int64_t)note+O(i,Semi)+ifw_div100(O(i,Tune))+((int64_t)s->mod[IFW_MOD_PITCH+i]*157286>>16));
        uint64_t step=cached_step(s,(unsigned)i,pitch);s->step[i]=(uint32_t)(step>IFW_MAX_STEP?IFW_MAX_STEP:step);
        if(s->blep[i].step!=s->step[i])ifw_blep_configure(&s->blep[i],s->step[i]);
        int32_t width=ifw_clamp((int32_t)(((int64_t)O(i,PwmWidth)*1677722)>>16),83886,16693330);
        width=ifw_clamp(ifw_sat((int64_t)width+(((int64_t)s->mod[P_Osc1PwmWidth+i*IFW_OSC_STRIDE]*1677722)>>16)),83886,16693330);
        s->width[i]=(uint32_t)width<<8;
        s->saw_level[i]=level20(ifw_sat((int64_t)O(i,SawLevel)+s->mod[P_Osc1SawLevel+i*IFW_OSC_STRIDE]));
        s->wave_level[i]=level20(ifw_sat((int64_t)O(i,TriLevel)+s->mod[P_Osc1TriLevel+i*IFW_OSC_STRIDE]));
        s->pulse_level[i]=level20(ifw_sat((int64_t)O(i,PwmLevel)+s->mod[P_Osc1PwmLevel+i*IFW_OSC_STRIDE]));
        s->noise_level[i]=level20(ifw_sat((int64_t)O(i,NoiseLevel)+s->mod[P_Osc1NoiseLevel+i*IFW_OSC_STRIDE]));
        unsigned mask=s->wave_sources[i];
        if(O(i,Output1) || O(i,Output2)) {
            if(s->saw_level[i])mask|=1u;
            if(s->wave_level[i])mask|=2u;
            if(s->pulse_level[i])mask|=4u;
        }
        s->wave_needed[i]=(uint8_t)mask;
    }
}
static void update_controls(IFWFixedSynth *s) {
    /* Only routed destinations can have been written by the previous update.
     * set_patch clears the whole array before replacing the routing list. */
    for(unsigned i=0;i<s->mod_count;i++)s->mod[s->mods[i].dest]=0;
    for(unsigned i=0;i<s->mod_count;i++) {
        IFWFixedMod *m=&s->mods[i];if(m->dest>=IFW_MOD_FM && m->dest<IFW_MOD_PITCH)continue;
        /* An uninitialised upstream noise CV is treated as an absent contribution. */
        if(!s->noise_ready && (((m->a&127)==IFW_SRC_NOISE)||((m->b&127)==IFW_SRC_NOISE)))continue;
        int32_t a=ifw_mul20(source20(s,m->a),source20(s,m->b));
        int32_t v=ifw_sat(((int64_t)a*m->depth)>>20);
        s->mod[m->dest]=ifw_sat((int64_t)s->mod[m->dest]+v);
    }
    int32_t note=ifw_sat((int64_t)s->pitch+V(Transpose)+(((int64_t)s->bend*V(PitchBendRange))>>31)+ifw_div100(V(MasterTune)));
    s->master_step=cached_step(s,4,note);
    s->control_note=note;configure_oscillators(s,note);
    for(int i=0;i<2;i++) {
        IFWFixedFilter *f=&s->filter[i];f->type=(uint8_t)F(i,Type);
        int32_t loghz=log_cutoff(s,i,note);
        if(!s->filter_valid[i] || s->filter_log[i]!=loghz) {
            f->alpha=ifw_filter_alpha30(loghz);s->filter_log[i]=loghz;s->filter_valid[i]=1;
        }
        int32_t res=ifw_clamp(ifw_sat((int64_t)F(i,Resonance)+s->mod[P_Filter1Resonance+i*IFW_FILTER_STRIDE]),0,652083);
        f->res=(int32_t)(((int64_t)res*403702)>>16); /* /10 *3.85 in Q20 */
    }
    configure_filter3(s);configure_rates(s);ifw_fixed_fx_mod_configure(s);
}
void ifw_fixed_patch_default(IFWFixedPatch *p) {
    memcpy(p,ifw_default_values,sizeof *p);
}
#ifdef IFW_TARGET
__attribute__((noinline))
#endif
void ifw_fixed_capture_sources(IFWFixedSynth *s) {
    /* Call before the old step coefficients are replaced. No phase, RNG or
     * envelope is advanced: these are the unevaluated old sample's pure CVs. */
    for(unsigned i=0;i<4;i++)if(s->last_wave[i].missing) {
        unsigned mask=s->last_wave[i].missing;
        ifw_waves20_masked(s->last_wave[i].phase,s->wave_type[i],s->width[i],
            &s->blep[i],mask,&s->raw_saw[i],&s->secondary[i],&s->raw_pulse[i]);
        if(mask&2u)s->secondary_cv[i]=s->wave_type[i]<=1?
            ifw_mul30_bounded(s->secondary[i],911108888):s->secondary[i];
        s->last_wave[i].missing=0;
    }
}
static void require_source(IFWFixedSynth *s,uint8_t code) {
    unsigned id=code&127u;
    if(id>=IFW_SRC_SAW && id<IFW_SRC_WAVE)s->wave_sources[id-IFW_SRC_SAW]|=1u;
    else if(id>=IFW_SRC_WAVE && id<IFW_SRC_PWM)s->wave_sources[id-IFW_SRC_WAVE]|=2u;
    else if(id>=IFW_SRC_PWM && id<IFW_SRC_NOISE)s->wave_sources[id-IFW_SRC_PWM]|=4u;
}
void ifw_fixed_set_patch(IFWFixedSynth *s,const IFWFixedPatch *p) {
    ifw_fixed_capture_sources(s);
    if(p!=&s->patch)s->patch=*p;
    s->mod_count=0;clear_mod(s);memset(s->fm_count,0,sizeof s->fm_count);
    memset(s->wave_sources,0,sizeof s->wave_sources);
    int32_t master=ifw_div10(V(MasterLevel));s->master_gain=ifw_mul16(master,master);
    for(int i=0;i<2;i++)s->amp_depth[i]=ifw_div10(s->patch.v[P_Amp1Level+i*3+1]);
    s->sync_mask=s->output1_mask=s->output2_mask=0;
    for(int i=0;i<4;i++) {
        s->wave_type[i]=O(i,Waveform2);
        if(O(i,Sync))s->sync_mask|=1u<<i;
        if(O(i,Output1))s->output1_mask|=1u<<i;
        if(O(i,Output2))s->output2_mask|=1u<<i;
    }
    int32_t lofi=ifw_clamp(O(3,NoiseLoFi),0,655360);
    s->noise_period=(int32_t)(((int64_t)lofi*lofi*2)>>32);
    s->glide_static_speed=ifw_sat(((int64_t)V(GlideMinimumSpeed)+V(GlideSpeed))*256);
    s->bend_k=ifw_smooth31(V(PitchBendSmoothTime));s->glide_k=ifw_smooth31(V(GlideSmoothTime));
    for(int i=0;i<4;i++)ifw_fixed_env_configure_cv(&s->env[i],E(i,Attack),E(i,Decay),
        ifw_udiv64((uint64_t)(uint32_t)ifw_clamp(E(i,Sustain),0,655360)*INT32_MAX,655360),E(i,Release),E(i,Retrigger));
    for(int i=0;i<8;i++) {
        int id=P_Mod1Source1+i*IFW_MOD_STRIDE,source=s->patch.v[id],dest=s->patch.v[id+2];
        if(source && dest>=0 && dest<IFW_MOD_COUNT && s->patch.v[id+3]) {
            IFWFixedMod *m=&s->mods[s->mod_count++];m->a=(uint8_t)source;m->b=(uint8_t)s->patch.v[id+1];
            m->dest=(int16_t)dest;m->depth=s->patch.v[id+3];
            require_source(s,m->a);require_source(s,m->b);
            if(dest>=IFW_MOD_FM && dest<IFW_MOD_PITCH) {
                unsigned osc=(unsigned)(dest-IFW_MOD_FM);
                s->fm_index[osc][s->fm_count[osc]++]=(uint8_t)(s->mod_count-1u);
            }
        }
    }
    configure_filter3(s);configure_rates(s);configure_oscillators(s,s->control_note);ifw_fixed_fx_configure(s);
}
void ifw_fixed_init(IFWFixedSynth *s,const IFWFixedPatch *p,IFWFixedFxBuffers *buffers) {
    memset(s,0,sizeof *s);s->note=60;s->pitch=s->pitch_target=s->glide_from=60*65536;
    s->control_note=60*65536;s->velocity=INT32_MAX;s->seed=123456789;s->effects.buffers=buffers;
    ifw_fixed_fx_reset(s);ifw_fixed_set_patch(s,p);
}
void ifw_fixed_note_on(IFWFixedSynth *s,uint32_t note,int32_t velocity) {
    if(note>127)return;
    int legato=ifw_mono_on(&s->mono,note);
    s->held_velocity[note]=velocity;s->velocity=velocity;s->note=(int32_t)note;s->pitch_target=(int32_t)note*65536;
    s->glide_from=s->pitch;s->glide_elapsed=0;s->glide_fraction=0;
    if(!legato && s->gate<214748)s->pitch=s->glide_from=s->pitch_target;
    s->gate_target=1;s->gate_step=(int32_t)(((int64_t)(INT32_MAX-s->gate)*ifw_linear_step31(V(GateOpenSmoothTime)))>>31);
    if(!s->gate_step && s->gate<INT32_MAX)s->gate_step=1;
    if(!legato)s->note_samples=0;
    for(int i=0;i<4;i++)ifw_fixed_env_gate(&s->env[i],1,legato);
    for(int i=0;i<2;i++)if(L(i,KeySync))s->lfo_phase[i]=0;
}
void ifw_fixed_note_off(IFWFixedSynth *s,uint32_t note) {
    ifw_mono_off(&s->mono,note);int current=ifw_mono_current(&s->mono);
    if(current>=0) {s->note=current;s->pitch_target=current*65536;s->velocity=s->held_velocity[current];
        s->glide_from=s->pitch;s->glide_elapsed=0;s->glide_fraction=0;}
    else {s->gate_target=0;s->gate_step=(int32_t)(((int64_t)s->gate*ifw_linear_step31(V(GateCloseSmoothTime)))>>31);
        if(!s->gate_step && s->gate)s->gate_step=1;
        for(int i=0;i<4;i++)ifw_fixed_env_gate(&s->env[i],0,0);}
}
void ifw_fixed_breath(IFWFixedSynth *s,int32_t value) {
    value=ifw_clamp(value,0,INT32_MAX);if(value==s->breath_target)return;
    s->breath_target=value;
    uint32_t ms=(uint32_t)ifw_clamp(V(BreathSmoothTime),0,1000*65536);
    uint32_t samples=ifw_udiv64((uint64_t)ms*441u+327680u,655360u);
    if(!samples)samples=1;
    s->breath_left=samples;
    s->breath_step=(value-s->breath)/(int32_t)samples;
}
void ifw_fixed_panic(IFWFixedSynth *s) {
    ifw_mono_clear(&s->mono);s->gate=s->gate_target=s->gate_step=0;
    s->breath=s->breath_target=s->breath_step=s->breath_left=0;
    for(int i=0;i<4;i++) {s->env[i].stage=0;s->env[i].level=0;}
    for(int i=0;i<2;i++) {memset(s->filter[i].z,0,sizeof s->filter[i].z);s->filter[i].last=0;}
    memset(s->filter3.z,0,sizeof s->filter3.z);s->filter3.last=0;
    ifw_fixed_fx_reset(s);ifw_fixed_fx_configure(s);
}
static int32_t filter_tick(IFWFixedFilter *f,int32_t input) {
    int32_t x=f->res?ifw_soft20(ifw_subsat(input,ifw_mul20(f->last,f->res))):input,y=0;
    /* The two/four-pole topology is fixed. Preserve each interpolation and
     * high-pass subtraction, without indexing a variable-length inner loop. */
    y=ifw_lerp30(f->z[0],x,f->alpha);f->z[0]=y;x=f->type==2?x-y:y;
    y=ifw_lerp30(f->z[1],x,f->alpha);f->z[1]=y;x=f->type==2?x-y:y;
    if(f->type==0) {
        y=ifw_lerp30(f->z[2],x,f->alpha);f->z[2]=y;
        y=ifw_lerp30(f->z[3],y,f->alpha);f->z[3]=y;x=y;
    }
    f->last=y;return f->type==3?ifw_sat(((int64_t)f->z[0]-y)*2):x;
}
static int32_t filter3_tick(IFWFixedFilter3 *f,int32_t input) {
    if(!f->enabled || !f->mix)return input;
    int32_t y=ifw_addsat(input,ifw_mul20(f->last,f->feedback));
    /* Frozen a30 coefficients are strictly inside (-Q30,Q30). */
    for(int i=0;i<f->stages;i++) {int32_t v=ifw_addsat(ifw_mul30_bounded(y,f->a),f->z[i]);
        f->z[i]=ifw_subsat(y,ifw_mul30_bounded(v,f->a));y=v;}
    int32_t wet=f->last;f->last=y;
    int32_t mix=f->mix<0?-f->mix:f->mix;
    return ifw_clamp(ifw_sat((int64_t)ifw_mul20(input,IFW_Q20-mix)+ifw_mul20(wet,f->mix)),-5*IFW_Q20,5*IFW_Q20);
}
static int32_t amp_gain(IFWFixedSynth *s,int i) {
    int id=P_Amp1Level+i*3;
    if(!s->patch.v[id] && !s->mod[id])return 0;
    int eg=s->patch.v[id+2];int32_t level=eg>=1&&eg<=4?s->env[eg-1].level:s->gate;
    if(!level && !s->gate)return 0;
    int32_t mod=(int32_t)(((int64_t)s->breath*level)>>31),depth=s->amp_depth[i];
    int32_t blend=depth==IFW_Q16?mod:!depth?s->gate:
        ifw_sat((int64_t)s->gate+(((int64_t)(mod-s->gate)*depth)>>16));
    int32_t base=(int32_t)(((int64_t)s->patch.v[id]*blend)>>31);
    int32_t extra=s->mod[id]?(int32_t)(((int64_t)(ifw_div20(s->mod[id]))*level)>>31):0;
    return ifw_sat((int64_t)(ifw_div10(base))*16+(int64_t)extra*16);
}
static uint32_t elapsed16(uint64_t elapsed,uint64_t step) {
    while(step>UINT32_MAX) {elapsed>>=1;step>>=1;}
    return ifw_ratio16((uint32_t)elapsed,(uint32_t)step);
}
static int32_t oscillator_sample(IFWFixedSynth *s) {
    int64_t total1=0,total2=0;
    for(int sub=0;sub<2;sub++) {
        int32_t bus1=0,bus2=0;uint32_t previous_elapsed=0;int previous_wrap=0;
        uint64_t before=s->master_phase,next=before+s->master_step;
        int master_wrap=(s->sync_mask&1u) && before<IFW_PHASE_HALF && next>=IFW_PHASE_HALF;
        uint32_t master_elapsed=master_wrap?elapsed16(next-IFW_PHASE_HALF,s->master_step):0;
        s->master_phase=(uint32_t)next;
#if defined(IFW_TARGET) && defined(__clang__)
        /* Four fixed oscillators: expose constant offsets and the last-only
         * held-noise branch to the target optimizer. Execution order is kept. */
#pragma clang loop unroll(full)
#endif
        for(int i=0;i<4;i++) {
            int64_t fm=0;
            for(unsigned j=0;j<s->fm_count[i];j++) {IFWFixedMod *m=&s->mods[s->fm_index[i][j]];
                fm+=ifw_div10(ifw_sat(((int64_t)ifw_mul20(source20(s,m->a),source20(s,m->b))*m->depth)>>12));}
            uint32_t step=s->step[i],old=s->phase[i],advanced=old+step;
            /* step < 2^31: when old < 2^31 the sum cannot wrap uint32.
             * Only the following oscillator's hard-sync reads this fraction.
             * The fourth oscillator has no sync consumer at all. */
            int need_next=i<3 && (s->sync_mask&(2u<<i));
            int wrap=need_next && old<IFW_PHASE_HALF && advanced>=IFW_PHASE_HALF;
            uint32_t elapsed=wrap?elapsed16(advanced-IFW_PHASE_HALF,step):0;
            s->phase[i]=advanced;
            if((s->sync_mask&(1u<<i)) && (i==0?master_wrap:previous_wrap)) {
                elapsed=i==0?master_elapsed:previous_elapsed;
                s->phase[i]=IFW_PHASE_HALF+(uint32_t)(((uint64_t)step*elapsed)>>16);wrap=1;
            }
            uint32_t phase=s->phase[i]+(uint32_t)fm*256u;int type=s->wave_type[i];
            unsigned mask=s->wave_needed[i];
            if(mask==7u)ifw_waves20_cached(phase,type,s->width[i],&s->blep[i],&s->raw_saw[i],&s->secondary[i],&s->raw_pulse[i]);
            else if(mask)ifw_waves20_masked(phase,type,s->width[i],&s->blep[i],mask,&s->raw_saw[i],&s->secondary[i],&s->raw_pulse[i]);
            if(mask&2u)s->secondary_cv[i]=type<=1?ifw_mul30_bounded(s->secondary[i],911108888):s->secondary[i];
            if(mask!=7u)s->last_wave[i].phase=phase;
            s->last_wave[i].missing=(uint8_t)(7u^mask);
            s->seed^=s->seed<<13;s->seed^=s->seed>>17;s->seed^=s->seed<<5;
            s->noise_value=(int32_t)(s->seed>>11)-IFW_Q20;s->noise_ready=1;
            if(i==3) {if(s->noise_counter--<=0) {s->noise_held=s->noise_value;
                    s->noise_counter=s->noise_period;}s->noise_value=s->noise_held;}
            /* Oscillator phases, noise and every modulation source above still
             * advance. A zero output coefficient has exactly zero contribution. */
            /* Levels are clamped to 0..Q20; bandlimited waves are <4*Q20
             * and noise <Q20. All four terms sum to <16*Q20, inside int32. */
            int32_t signal=0;
            if((s->output1_mask|s->output2_mask)&(1u<<i)) {
            if(s->saw_level[i])signal+=ifw_mul20_bounded(s->raw_saw[i],s->saw_level[i]);
            if(s->wave_level[i])signal+=ifw_mul20_bounded(s->secondary[i],s->wave_level[i]);
            if(s->pulse_level[i])signal+=ifw_mul20_bounded(s->raw_pulse[i],s->pulse_level[i]);
            if(s->noise_level[i])signal+=ifw_mul20_bounded(ifw_mul20_bounded(s->noise_value,828375),s->noise_level[i]);
            if(s->output1_mask&(1u<<i))bus1=ifw_addsat(bus1,signal);
            if(s->output2_mask&(1u<<i))bus2=ifw_addsat(bus2,signal);
            }
            previous_wrap=wrap;previous_elapsed=elapsed;
        }
        total1+=bus1;total2+=bus2;
    }
    int32_t bus1=ifw_half(total1),bus2=ifw_half(total2),a=0,b=0;
    if(V(FilterConnection)==1)a=filter_tick(&s->filter[1],filter_tick(&s->filter[0],ifw_addsat(bus1,bus2)));
    else if(V(FilterConnection)==2)a=ifw_addsat(filter_tick(&s->filter[0],bus1),filter_tick(&s->filter[1],bus2));
    else {a=filter_tick(&s->filter[0],bus1);b=filter_tick(&s->filter[1],bus2);}
    return ifw_mul30_bounded(ifw_addsat(ifw_mul20(filter3_tick(&s->filter3,a),amp_gain(s,0)),
        ifw_mul20(b,amp_gain(s,1))),39728447);
}
void ifw_fixed_sample(IFWFixedSynth *s,int32_t *left,int32_t *right) {
    s->source_valid=0;
    if(s->breath_left) {s->breath+=s->breath_step;if(!--s->breath_left)s->breath=s->breath_target;}
    if(s->bend!=s->bend_target)s->bend=ifw_sat((int64_t)s->bend+(((int64_t)s->bend_target-s->bend)*s->bend_k>>31));
    s->gate=s->gate_target?ifw_addsat(s->gate,s->gate_step):ifw_clamp(s->gate-s->gate_step,0,INT32_MAX);
    if(s->glide_smoothed!=s->glide)s->glide_smoothed=ifw_sat((int64_t)s->glide_smoothed+(((int64_t)s->glide-s->glide_smoothed)*s->glide_k>>31));
    int32_t speed=V(GlideMidiEnabled)?ifw_sat((int64_t)V(GlideMinimumSpeed)*256+
        ((int64_t)s->glide_smoothed*V(GlideSpeed)>>23)):s->glide_static_speed;
    s->glide_elapsed++;
    if(speed>0) {
        /* Q39 retains sub-LSB timing over a held glide; Q31 rounded the speed
         * every sample and accumulated a measurable phase error in factory patches. */
        if(!s->recip_valid || s->recip_speed!=speed) {
            s->glide_recip=ifw_glide_recip39_fine(speed);s->recip_speed=speed;s->recip_valid=1;
        }
        uint64_t recip=s->glide_recip;
        if(V(GlideMode)==1) {
            uint64_t v=recip*12u+s->glide_fraction;uint32_t step=(uint32_t)(v>>23);s->glide_fraction=(uint32_t)v&8388607u;
            s->pitch+=ifw_clamp(s->pitch_target-s->pitch,-(int32_t)step,(int32_t)step);
        }else {
            uint64_t norm=((uint64_t)s->glide_elapsed*recip)>>8;
            if(norm>=INT32_MAX)s->pitch=s->pitch_target;
            else s->pitch=s->glide_from+(int32_t)(((int64_t)(s->pitch_target-s->glide_from)*(int64_t)norm)>>31);
        }
    }else s->pitch=s->pitch_target;
    for(int i=0;i<4;i++)ifw_fixed_env_tick(&s->env[i],s->env_rate[i]);
    for(int i=0;i<2;i++)s->lfo_phase[i]+=s->lfo_step[i];
    if((s->block_count++&15u)==0)update_controls(s);
    ifw_fixed_fx_tick(s,oscillator_sample(s));
    int32_t master=s->master_gain;
    *left=ifw_clamp(master==IFW_Q16?s->effects.sample_left:ifw_mul16(s->effects.sample_left,master),-IFW_Q20,IFW_Q20);
    *right=ifw_clamp(master==IFW_Q16?s->effects.sample_right:ifw_mul16(s->effects.sample_right,master),-IFW_Q20,IFW_Q20);
    if(s->gate_target)s->note_samples++;
}
void ifw_fixed_process(IFWFixedSynth *s,int32_t *out,uint32_t frames) {
    for(uint32_t i=0;i<frames;i++) {int32_t l,r;ifw_fixed_sample(s,&l,&r);out[2*i]=ifw_clamp((l+16)>>5,-32767,32767);out[2*i+1]=ifw_clamp((r+16)>>5,-32767,32767);}
}
