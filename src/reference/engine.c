/* SPDX-License-Identifier: GPL-3.0-only
 * Algorithm port from OpenIFW (MIT): vendor/openifw/LICENSE.
 * Keep sequencing equal to the pinned JS oracle; optimize the fixed target separately.
 */
#include "internal.h"
#include "envelope_curve.h"
#include <ctype.h>
#include <stdlib.h>

static int param_id(const char *key) {
    for (int i=0;i<IFW_PARAM_COUNT;i++) if (!strcmp(key,ifw_param_defs[i].name)) return i;
    return -1;
}
void ifw_patch_default(IFWPatch *p) {
    memset(p,0,sizeof *p); p->tuning=440;
    for (int i=0;i<IFW_PARAM_COUNT;i++) {
        p->num[i]=ifw_param_defs[i].number;
        if (ifw_param_defs[i].text) strcpy(p->text[i],ifw_param_defs[i].text);
    }
    for (int i=0;i<5;i++) {
        p->fx[i][FX_Bypass]=1;
        p->fx[i][FX_Speed]=1; p->fx[i][FX_Rate]=0.3;
        p->fx[i][FX_Depth]=i==1?7:0; p->fx[i][FX_Mix]=2.7;
        p->fx[i][FX_Lowpass]=i==0?4:15000; p->fx[i][FX_Highpass]=10;
        p->fx[i][FX_Time]=0.2; p->fx[i][FX_Size]=6.6;
        p->fx[i][FX_HighDamp]=3; p->fx[i][FX_Width]=6.4;
        p->fx[i][FX_WetDry]=i==4?2:0;
    }
}
int ifw_patch_number(IFWPatch *p,const char *key,double v) {
    int i=param_id(key); if(i<0 || !isfinite(v)) return 0;
    p->num[i]=v; p->text[i][0]=0; return 1;
}
int ifw_patch_text(IFWPatch *p,const char *key,const char *v) {
    int i=param_id(key); if(i<0 || strlen(v)>=sizeof p->text[0]) return 0;
    strcpy(p->text[i],v); p->num[i]=0; return 1;
}
int ifw_patch_fx(IFWPatch *p,int index,const char *key,double v) {
    static const char *const keys[]={"Bypass","Stereo","Speed","Feedback","Lowpass","WetDry",
        "Rate","Depth","Mix","Time","Highpass","Size","HighDamp","Width"};
    if (index<0 || index>=5 || !isfinite(v)) return 0;
    for(int i=0;i<FX_COUNT;i++) if(!strcmp(key,keys[i])) { p->fx[index][i]=v; return 1; }
    return 0;
}
static double clock_value(const double (*knots)[2],int n,double v) {
    v=clip(v,0,10); int i=1;
    while(i<n-1 && v>knots[i][0]) i++;
    return knots[i-1][1]*exp(log(knots[i][1]/knots[i-1][1])*(v-knots[i-1][0])/(knots[i][0]-knots[i-1][0]));
}
double ifw_attack_seconds(double v) {
    static const double k[][2]={{0,.0028046},{2,.0142461},{3,.0320786},{5,.1625949},{8,1.8569279}};
    return clock_value(k,5,v);
}
double ifw_decay_seconds(double v) {
    static const double k[][2]={{0,.001794804017599996},{4.6,.03928657751073832},
        {5.6,.0766502365383003},{7,.19640721210337342},{8,.3823215381376901}};
    return clock_value(k,5,v);
}
double ifw_native_decay(double u) {
    double x=fmax(0,u)*100;
    if(x>=sizeof decay_curve/sizeof *decay_curve-1) return 0;
    unsigned i=(unsigned)floor(x);
    return decay_curve[i]+(decay_curve[i+1]-decay_curve[i])*(x-i);
}
static void env_gate(IFWEnvelope *e,int open,int legato) {
    if(open) { if(!legato || e->retrigger) { e->stage=1; e->time=0; e->level=0; } }
    else if(e->stage!=0 && e->stage!=4) { e->stage=4; e->release_start=e->level; e->time=0; }
}
static void env_tick(IFWEnvelope *e,double sr,double rate) {
    e->time+=rate/sr;
    if(e->stage==1) { e->level=fmin(1,e->time/e->a); if(e->level>=1) { e->stage=2; e->time=0; } }
    else if(e->stage==2) { double v=ifw_native_decay(e->time/e->d); e->level=e->s+(1-e->s)*v;
        if(v==0) { e->stage=3; e->level=e->s; } }
    else if(e->stage==3) e->level=e->s;
    else if(e->stage==4) { double v=ifw_native_decay(e->time/e->r); e->level=e->release_start*v;
        if(v==0) { e->stage=0; e->level=0; } }
}
static double polyblep(double t,double dt) {
    if(dt<=0 || dt>=.5) return 0;
    if(t<dt) { t/=dt; return t+t-t*t-1; }
    if(t>1-dt) { t=(t-1)/dt; return t*t+t+t+1; }
    return 0;
}
double ifw_wave(double phase,double dt,const char *type,double width) {
    double p=phase-floor(phase);
    if(!strcmp(type,"SIN") || !strcmp(type,"SINE")) return sin(TAU*p)*1.1785;
    if(!strcmp(type,"TRI")) return (1.1785*2/acos(-1))*asin(sin(TAU*p));
    if(!strcmp(type,"SAW")) return 2*p-1-polyblep(p,dt);
    if(!strcmp(type,"SQU") || !strcmp(type,"PWM") || !strcmp(type,"PULSE")) {
        width=clip(width,.005,.995);
        return (p<width?1:-1)-(2*width-1)+polyblep(p,dt)-polyblep(fmod(p-width+1,1),dt);
    }
    return 0;
}
static void upper(char *out,const char *in) {
    while(*in) *out++=(char)toupper((unsigned char)*in++);
    *out=0;
}
static int module_index(const char *s,const char *prefix,int max) {
    const char *p=strstr(s,prefix); if(!p) return -1;
    p+=strlen(prefix); while(*p==' ') p++;
    int i=*p-'1'; return i>=0 && i<max?i:-1;
}
static double source(IFWSynth *s,const char *name,int fm_reader) {
    char u[64]; upper(u,name); int bipolar=strstr(u,"+-")!=NULL || strstr(u,"-+")!=NULL;
    if(!strcmp(name,"None") || (fm_reader && !strcmp(u,"NONE"))) return 1;
    if(strstr(u,"BREATH") || !strcmp(u,"BR")) return bipolar?s->breath*2-1:s->breath;
    if(strstr(u,"VELOCITY")) return bipolar?2*(s->velocity-64.0/127):s->velocity;
    if(strstr(u,"NOTE")) return bipolar?2*(s->pitch-64)/127:s->note/127;
    if(strstr(u,"PITCH BEND")) return s->bend;
    if(strstr(u,"VIBRATO")) return s->vibrato;
    if(strstr(u,"GLIDE")) return s->glide;
    if(strstr(u,"GATE")) return s->gate;
    int i=module_index(u,"EG",4);
    if(i>=0) return bipolar?2*s->env[i].level-1:s->env[i].level;
    i=module_index(u,"LFO",2);
    if(i>=0) {
        const char *type=LT(i,Waveform);
        double v=ifw_wave(s->lfo_phase[i],0,!strcmp(type,"SQU")?"PULSE":type,.5)/(!strcmp(type,"TRI")?1.1785:1);
        return bipolar?v:(v+1)*.5;
    }
    i=module_index(u,"OSC",4);
    if(i>=0) {
        double v=strstr(u,"SAW")?s->raw_saw[i]:strstr(u,"PWM")?s->raw_pulse[i]:
                 strstr(u,"NOISE")?s->noise_value:s->secondary_cv[i];
        return bipolar?v:(v+1)*.5;
    }
    return 0;
}
/* Destination spellings are resolved using the upstream aliases and case/space rule. */
static int destination(const char *text) {
    char u[64],v[64]; upper(u,text); int j=0,space=0;
    for(int i=0;u[i];i++) { if(isspace((unsigned char)u[i])) { space=j>0; continue; }
        if(space) v[j++]=' ';
        space=0; v[j++]=u[i]; }
    v[j]=0;
    struct Alias { const char *label,*key; };
    static const struct Alias osc[]={{"PITCH","Pitch"},{"FM","FM"},{"SAW LEVEL","SawLevel"},
        {"TRI LEVEL","TriLevel"},{"WAVE LEVEL","TriLevel"},{"PWM LEVEL","PwmLevel"},
        {"PWM WIDTH","PwmWidth"},{"PWM PHASE","PwmWidth"},{"NOISE LEVEL","NoiseLevel"},{"NOISE LO-FI","NoiseLoFi"}};
    static const struct Alias filter[]={{"FREQUENCY","Frequency"},{"RESONANCE","Resonance"},{"MIX","Mix"}};
    char label[64],key[64];
    for(int i=1;i<=4;i++) for(unsigned a=0;a<sizeof osc/sizeof *osc;a++) {
        if(i!=4 && a>=8) continue;
        snprintf(label,sizeof label,"OSC %d %s",i,osc[a].label);
        if(!strcmp(v,label)) {
            if(a==0) return IFW_PARAM_COUNT+10+i-1;
            if(a==1) return IFW_PARAM_COUNT+6+i-1;
            snprintf(key,sizeof key,"Osc%d%s",i,osc[a].key); return param_id(key);
        }
    }
    for(int i=1;i<=3;i++) for(int a=0;a<(i==3?3:2);a++) {
        snprintf(label,sizeof label,"FILTER %d %s",i,filter[a].label);
        if(!strcmp(v,label)) { snprintf(key,sizeof key,"Filter%d%s",i,filter[a].key); return param_id(key); }
    }
    for(int i=1;i<=4;i++) {
        snprintf(label,sizeof label,"EG %d RATE",i);
        if(!strcmp(v,label)) return IFW_PARAM_COUNT+i-1;
    }
    for(int i=1;i<=2;i++) {
        snprintf(label,sizeof label,"AMP %d LEVEL",i); if(!strcmp(v,label)) return P_Amp1Level+(i-1)*3;
        snprintf(label,sizeof label,"LFO %d RATE",i); if(!strcmp(v,label)) return IFW_PARAM_COUNT+4+i-1;
    }
    if(!strcmp(v,"EXCITER FREQUENCY")) return P_ExciterFrequency;
    if(!strcmp(v,"EXCITER MIX")) return P_ExciterMix;
    if(!strcmp(v,"ENHANCER MIX")) return P_EnhancerMix;
    return -1;
}
static double breath_source(IFWSynth *s,const char *name) {
    char u[64]; upper(u,name); int i=module_index(u,"EG",4);
    return s->breath*(i>=0?s->env[i].level:1);
}
static double cutoff(double f,double depth,double source_value,double kt,double note,double sr) {
    return fmin(fmin(20700,sr*.49),5.663990017612683*exp(.9236470127266707*f+
        .8355614261719994*depth*source_value+.07083919091247709*(note-60)*kt/10-.0002788762124903089*kt));
}
static void configure_filter(IFWFilter *f,const char *type,double frequency,double res,double sr) {
    f->type=!strcmp(type,"LPF24")?0:!strcmp(type,"HPF12")?2:!strcmp(type,"BPF12")?3:1;
    f->res=clip(res/10,0,.995); f->a=1-exp(-TAU*fmax(.1,frequency)/sr);
}
static double filter_tick(IFWFilter *f,double input) {
    double x=f->res?soft(input-f->last*f->res*3.85):input,y=0;
    for(int i=0;i<(f->type==0?4:2);i++) {
        y=f->z[i]+(x-f->z[i])*f->a; f->z[i]=y; x=f->type==2?x-y:y;
    }
    f->last=y; return f->type==3?2*(f->z[0]-y):x;
}
static double filter3_hz(double cv,double sr) {
    static const double x[]={5.7,7.7+5*(2.0*(48-64)/127),7.7+5*(2.0*(60-64)/127),7.7,
        7.7+5*(2.0*(72-64)/127),9.7};
    static const double y[]={850.4219584482449,1443.3044461941413,2646.581370739413,
        3210.0425529737995,4677.170704061711,10254.617860351776};
    int i=1; while(i<5 && cv>x[i]) i++;
    double ga=tan(acos(-1)*y[i-1]/48000),gb=tan(acos(-1)*y[i]/48000);
    double g=ga*exp(log(gb/ga)*(cv-x[i-1])/(x[i]-x[i-1]));
    return fmin(sr*.49,(48000/acos(-1))*atan(g));
}
static void configure_filter3(IFWSynth *s) {
    IFWFilter3 *f=&s->filter3;
    f->enabled=is_on(&s->p,P_Filter3Enabled); f->mix=(N(Filter3Mix)+nz(s->mod[P_Filter3Mix]))/10;
    f->stages=(int)clip(floor(N(Filter3Stage)+.5),2,24);
    f->feedback=clip((N(Filter3Resonance)+nz(s->mod[P_Filter3Resonance]))/10,-.999,.999);
    double g=tan(acos(-1)*filter3_hz(N(Filter3Frequency)+nz(s->mod[P_Filter3Frequency]),s->sr)/s->sr);
    f->a=(g-1)/(g+1);
}
static double filter3_tick(IFWFilter3 *f,double input) {
    if(!f->enabled || f->mix==0) return input;
    double y=input+f->last*f->feedback;
    for(int i=0;i<f->stages;i++) { double v=f->a*y+f->z[i]; f->z[i]=y-f->a*v; y=v; }
    double wet=f->last; f->last=y;
    return clip(input*(1-fabs(f->mix))+wet*f->mix,-5,5);
}
static int is_fm(int id) {
    return id>=IFW_PARAM_COUNT+6 && id<IFW_PARAM_COUNT+10;
}
static double *mod_slot(IFWSynth *s,int id) { return &s->mod[id]; }
static void update_controls(IFWSynth *s) {
    memset(s->mod,0,sizeof s->mod);
    for(int i=0;i<s->mod_count;i++) {
        IFWMod *m=&s->mods[i]; if(is_fm(m->dest)) continue;
        double v=source(s,m->source[0],0)*source(s,m->source[1],0)*m->depth;
        *mod_slot(s,m->dest)=nz(*mod_slot(s,m->dest))+v;
    }
    double note=s->pitch+N(Transpose)+s->bend*N(PitchBendRange)+N(MasterTune)/100;
    s->frequency=hz(note,s->p.tuning);
    for(int i=0;i<2;i++) configure_filter(&s->filter[i],FT(i,Type),
        cutoff(F(i,Frequency)+nz(s->mod[P_Filter1Frequency+i*IFW_FILTER_STRIDE]),F(i,Breath),
        breath_source(s,FT(i,BreathSubMod)),F(i,KeyTrack),note,s->sr),
        F(i,Resonance)+nz(s->mod[P_Filter1Resonance+i*IFW_FILTER_STRIDE]),s->sr);
    configure_filter3(s);
}
static double amp_gain(IFWSynth *s,int i) {
    int id=P_Amp1Level+i*3; char u[64]; upper(u,s->p.text[P_Amp1BreathSubMod+i*3]);
    int eg=module_index(u,"EG",4); double level=eg>=0?s->env[eg].level:s->gate;
    double depth=s->p.num[P_Amp1Breath+i*3]/10;
    return (s->p.num[id]/10)*((1-depth)*s->gate+depth*s->breath*level)+nz(s->mod[id])*.05*level;
}
static double model_sample(IFWSynth *s) {
    double sums[2]={0,0};
    for(int sub=0;sub<2;sub++) {
        double bus[2]={0,0},prev_elapsed=0; int prev_wrap=0;
        double mstep=s->frequency/(s->sr*2),before=s->master_phase;
        s->master_phase=fmod(before+mstep,1);
        int mwrap=before<.5 && before+mstep>=.5;
        double melapsed=mwrap?(before+mstep-.5)/mstep:0;
        for(int i=0;i<4;i++) {
            double fm=0;
            for(int j=0;j<s->mod_count;j++) { IFWMod *m=&s->mods[j];
                if(m->dest==IFW_PARAM_COUNT+6+i) fm+=source(s,m->source[0],1)*source(s,m->source[1],1)*(m->depth*.1); }
            double step=clip(s->frequency*pow(2,(O(i,Semi)+O(i,Tune)/100+
                2.4*nz(s->mod[IFW_PARAM_COUNT+10+i]))/12)/(s->sr*2),0,.45);
            before=s->phase[i]; double next=before+step;
            int wrap=before<.5 && next>=.5;
            s->phase[i]=next-floor(next);
            double elapsed=wrap && step>0?(next-.5)/step:0;
            if(is_on(&s->p,P_Osc1Sync+i*IFW_OSC_STRIDE) && (i==0?mwrap:prev_wrap)) {
                elapsed=i==0?melapsed:prev_elapsed; s->phase[i]=.5+step*elapsed; wrap=1;
            }
            double phase=s->phase[i]+fm; const char *wave=OT(i,Waveform2);
            int secondary=!strcmp(wave,"SIN") || !strcmp(wave,"SINE") || !strcmp(wave,"TRI");
            s->raw_saw[i]=ifw_wave(phase,fabs(step),"SAW",.5);
            s->secondary[i]=ifw_wave(phase+(secondary?.5:0),fabs(step),wave,.5);
            s->secondary_cv[i]=s->secondary[i]/(secondary?1.1785:1);
            double width=clip(clip(O(i,PwmWidth)/10,.005,.995)+nz(s->mod[P_Osc1PwmWidth+i*IFW_OSC_STRIDE])/10,.005,.995);
            s->raw_pulse[i]=ifw_wave(phase+width,fabs(step),"PWM",width);
            s->seed^=s->seed<<13; s->seed^=s->seed>>17; s->seed^=s->seed<<5;
            s->noise_value=s->seed/2147483648.0-1;
            if(i==3) { if(s->noise_counter--<=0) { s->noise_held=s->noise_value;
                    s->noise_counter=(int)floor(O(i,NoiseLoFi)*O(i,NoiseLoFi)*2); }
                s->noise_value=s->noise_held; }
            double signal=s->raw_saw[i]*clip(O(i,SawLevel)/10+nz(s->mod[P_Osc1SawLevel+i*IFW_OSC_STRIDE])/10,0,1)+
                s->secondary[i]*clip(O(i,TriLevel)/10+nz(s->mod[P_Osc1TriLevel+i*IFW_OSC_STRIDE])/10,0,1)+
                s->raw_pulse[i]*clip(O(i,PwmLevel)/10+nz(s->mod[P_Osc1PwmLevel+i*IFW_OSC_STRIDE])/10,0,1)+
                s->noise_value*.79*clip(O(i,NoiseLevel)/10+nz(s->mod[P_Osc1NoiseLevel+i*IFW_OSC_STRIDE])/10,0,1);
            if(is_on(&s->p,P_Osc1Output1+i*IFW_OSC_STRIDE)) bus[0]+=signal;
            if(is_on(&s->p,P_Osc1Output2+i*IFW_OSC_STRIDE)) bus[1]+=signal;
            prev_wrap=wrap; prev_elapsed=elapsed;
        }
        sums[0]+=bus[0]; sums[1]+=bus[1];
    }
    double bus1=sums[0]*.5,bus2=sums[1]*.5,b1=0,b2=0;
    if(!strcmp(T(FilterConnection),"Serial")) b1=filter_tick(&s->filter[1],filter_tick(&s->filter[0],bus1+bus2));
    else if(!strcmp(T(FilterConnection),"Parallel 1")) b1=filter_tick(&s->filter[0],bus1)+filter_tick(&s->filter[1],bus2);
    else { b1=filter_tick(&s->filter[0],bus1); b2=filter_tick(&s->filter[1],bus2); }
    return (filter3_tick(&s->filter3,b1)*amp_gain(s,0)+b2*amp_gain(s,1))*.037;
}
int ifw_init(IFWSynth *s,double sr,const IFWPatch *patch) {
    memset(s,0,sizeof *s); s->p=*patch; s->sr=sr;
    s->note=s->pitch=s->pitch_target=s->glide_from=60; s->velocity=1; s->seed=123456789;
    s->noise_value=NAN;
    if(!ifw_effects_init(s)) { ifw_effects_destroy(s); return 0; }
    ifw_set_patch(s,patch); return 1;
}
void ifw_set_patch(IFWSynth *s,const IFWPatch *patch) {
    s->p=*patch; s->mod_count=0; memset(s->mod,0,sizeof s->mod);
    for(int i=0;i<4;i++) { IFWEnvelope *e=&s->env[i];
        e->a=ifw_attack_seconds(E(i,Attack)); e->d=ifw_decay_seconds(E(i,Decay));
        e->r=ifw_decay_seconds(E(i,Release)); e->s=clip(E(i,Sustain)/10,0,1);
        e->retrigger=is_on(patch,P_Eg1Retrigger+i*IFW_EG_STRIDE); }
    for(int i=0;i<8;i++) {
        int b=P_Mod1Source1+i*IFW_MOD_STRIDE; char u[64]; upper(u,patch->text[b]);
        int dest=destination(patch->text[b+2]);
        if(strcmp(u,"NONE") && dest>=0 && patch->num[b+3]!=0) {
            IFWMod *m=&s->mods[s->mod_count++]; m->dest=dest; m->depth=patch->num[b+3];
            strcpy(m->source[0],patch->text[b]); strcpy(m->source[1],patch->text[b+1]); }
    }
    configure_filter3(s);
    ifw_effects_configure(s);
}
void ifw_destroy(IFWSynth *s) { ifw_effects_destroy(s); }
void ifw_set_dry(IFWSynth *s,int dry) { s->dry=!!dry; ifw_effects_configure(s); }
void ifw_breath(IFWSynth *s,double v) {
    v=clip(v,0,1); if(v==s->breath_target) return;
    s->breath_target=v; s->breath_left=(uint32_t)fmax(1,floor(s->sr*N(BreathSmoothTime)*.001+.5));
    s->breath_step=(v-s->breath)/s->breath_left;
}
void ifw_note_on(IFWSynth *s,unsigned note,double velocity) {
    if(note>127) return;
    int legato=ifw_mono_on(&s->mono,note); s->held_velocity[note]=velocity;
    s->note=s->pitch_target=note; s->velocity=velocity; s->glide_from=s->pitch; s->glide_elapsed=0;
    if(!legato && s->gate<.0001) s->pitch=s->glide_from=note;
    s->gate_target=1; s->gate_step=(1-s->gate)/fmax(1,s->sr*N(GateOpenSmoothTime)*.001);
    if(!legato) s->note_time=0;
    for(int i=0;i<4;i++) env_gate(&s->env[i],1,legato);
    for(int i=0;i<2;i++) if(is_on(&s->p,P_Lfo1KeySync+i*IFW_LFO_STRIDE)) s->lfo_phase[i]=0;
}
void ifw_note_off(IFWSynth *s,unsigned note) {
    ifw_mono_off(&s->mono,note); int current=ifw_mono_current(&s->mono);
    if(current>=0) { s->note=s->pitch_target=current; s->velocity=s->held_velocity[current];
        s->glide_from=s->pitch; s->glide_elapsed=0; }
    else { s->gate_target=0; s->gate_step=s->gate/fmax(1,s->sr*N(GateCloseSmoothTime)*.001);
        for(int i=0;i<4;i++) env_gate(&s->env[i],0,0); }
}
void ifw_panic(IFWSynth *s) {
    ifw_mono_clear(&s->mono); s->gate=s->gate_target=s->gate_step=0;
    s->breath=s->breath_target=s->breath_step=0; s->breath_left=0;
    /* Original panic resets effect/filter state, retaining oscillator phase and seed. */
    ifw_effects_destroy(s); if(!ifw_effects_init(s)) abort(); ifw_effects_configure(s);
    for(int i=0;i<2;i++) { memset(s->filter[i].z,0,sizeof s->filter[i].z); s->filter[i].last=0; }
    memset(s->filter3.z,0,sizeof s->filter3.z); s->filter3.last=0;
    for(int i=0;i<4;i++) { s->env[i].stage=0; s->env[i].level=0; }
}
void ifw_process(IFWSynth *s,float *left,float *right,uint32_t n) {
    double bend_k=1-exp(-1/(s->sr*fmax(.0001,N(PitchBendSmoothTime)*.001)));
    for(uint32_t i=0;i<n;i++) {
        if(s->breath_left>0) { s->breath+=s->breath_step; if(--s->breath_left==0) s->breath=s->breath_target; }
        s->bend+=(s->bend_target-s->bend)*bend_k;
        s->gate=s->gate_target?fmin(1,s->gate+s->gate_step):fmax(0,s->gate-s->gate_step);
        double k=1-exp(-1/(s->sr*fmax(.0001,N(GlideSmoothTime)*.001)));
        s->glide_smoothed+=(s->glide-s->glide_smoothed)*k;
        double glide=N(GlideMinimumSpeed)+(is_on(&s->p,P_GlideMidiEnabled)?s->glide_smoothed:1)*N(GlideSpeed);
        double seconds=glide>0?.002*pow(2,glide*1.2):0;
        s->glide_elapsed+=1/s->sr;
        if(seconds>0 && !strcmp(T(GlideMode),"Rate")) s->pitch+=clip(s->pitch_target-s->pitch,-12/(s->sr*seconds),12/(s->sr*seconds));
        else if(seconds>0) s->pitch=s->glide_from+(s->pitch_target-s->glide_from)*clip(s->glide_elapsed/seconds,0,1);
        else s->pitch=s->pitch_target;
        for(int j=0;j<4;j++) env_tick(&s->env[j],s->sr,exp(.8742323893*clip(nz(s->mod[IFW_PARAM_COUNT+j]),-10,10)));
        for(int j=0;j<2;j++) {
            double rate=.02*pow(2,L(j,Speed)+nz(s->mod[IFW_PARAM_COUNT+4+j]));
            if(is_on(&s->p,P_Lfo1BPMSync+j*IFW_LFO_STRIDE) || is_on(&s->p,P_Lfo1BpmSync+j*IFW_LFO_STRIDE)) {
                double a=1,b=4; sscanf(LT(j,Beat),"%lf/%lf",&a,&b); rate=2/((a/b)*4); }
            s->lfo_phase[j]=fmod(s->lfo_phase[j]+rate/s->sr,1);
        }
        if((s->block_count++ & 15)==0) update_controls(s);
        double l,r; ifw_effects_tick(s,model_sample(s),&l,&r);
        double master=N(MasterLevel)/10; master=pow(master,2);
        left[i]=(float)clip(l*master,-1,1); right[i]=(float)clip(r*master,-1,1);
        s->time+=1/s->sr; if(s->gate_target) s->note_time+=1/s->sr;
    }
}
void ifw_trace(const IFWSynth *s,double a[IFW_TRACE_COUNT]) {
    int n=0;
    a[n++]=s->note; a[n++]=s->pitch; a[n++]=s->pitch_target; a[n++]=s->velocity;
    a[n++]=s->gate; a[n++]=s->gate_target; a[n++]=s->breath; a[n++]=s->breath_target;
    a[n++]=s->bend; a[n++]=s->glide; a[n++]=s->glide_smoothed; a[n++]=s->glide_elapsed;
    a[n++]=s->glide_from; a[n++]=s->note_time;
    for(int i=0;i<4;i++) a[n++]=s->env[i].level;
    for(int i=0;i<4;i++) a[n++]=s->env[i].stage;
    for(int i=0;i<2;i++) a[n++]=s->lfo_phase[i];
    for(int i=0;i<4;i++) a[n++]=s->phase[i];
    a[n++]=s->master_phase; a[n++]=s->seed; a[n++]=s->noise_held;
    a[n++]=s->noise_counter; a[n++]=s->mono.count; a[n++]=s->breath_left;
    a[n++]=s->block_count; a[n++]=s->time;
}
