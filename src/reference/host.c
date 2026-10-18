/* SPDX-License-Identifier: GPL-3.0-only */
#include "engine.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LSan requires /proc thread access, unavailable in the execution sandbox.
 * Address/undefined-behavior instrumentation remains enabled. */
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) { return "detect_leaks=0"; }
#endif

typedef struct { uint32_t frame; char type[16],key[64],text[64]; double a,b; } Event;
static void fail(const char *message) { fprintf(stderr,"ifw-reference: %s\n",message); exit(2); }
static void dispatch(IFWSynth *s,const Event *e) {
    if(!strcmp(e->type,"noteOn")) ifw_note_on(s,(unsigned)e->a,e->b);
    else if(!strcmp(e->type,"noteOff")) ifw_note_off(s,(unsigned)e->a);
    else if(!strcmp(e->type,"breath")) ifw_breath(s,e->a);
    else if(!strcmp(e->type,"bend")) s->bend_target=e->a;
    else if(!strcmp(e->type,"glide")) s->glide=e->a;
    else if(!strcmp(e->type,"vibrato")) s->vibrato=e->a;
    else if(!strcmp(e->type,"panic")) ifw_panic(s);
    else if(!strcmp(e->type,"dry")) ifw_set_dry(s,(int)e->a);
    else if(!strcmp(e->type,"editnum") || !strcmp(e->type,"editstr")) {
        IFWPatch p=s->p;
        int ok=!strcmp(e->type,"editnum")?ifw_patch_number(&p,e->key,e->a):ifw_patch_text(&p,e->key,e->text);
        if(!ok) fail("invalid live parameter edit");
        ifw_set_patch(s,&p);
    }
    else fail("unknown event type");
}
int main(int argc,char **argv) {
    if(argc!=4 && argc!=5) fail("usage: ifw-reference SCENARIO OUTPUT_PREFIX BLOCK_SIZE [trace]");
    int block=atoi(argv[3]),trace=argc==5;
    if(block<1 || block>1024) fail("block size must be 1..1024");
    IFWPatch patch; ifw_patch_default(&patch);
    FILE *f=fopen(argv[1],"r"); if(!f) fail("cannot open scenario");
    char line[512],key[64],text[256]; double sr=44100,breath=1; unsigned frames=0,dry=1;
    Event events[4096]; unsigned count=0;
    while(fgets(line,sizeof line,f)) {
        double v; int index; unsigned u;
        if(line[0]=='#' || line[0]=='\n') continue;
        if(sscanf(line,"sr %lf",&v)==1) sr=v;
        else if(sscanf(line,"frames %u",&u)==1) frames=u;
        else if(sscanf(line,"dry %u",&u)==1) dry=u;
        else if(sscanf(line,"breath %lf",&v)==1) breath=v;
        else if(sscanf(line,"num %63s %lf",key,&v)==2) { if(!ifw_patch_number(&patch,key,v)) fail("unknown or invalid numeric parameter"); }
        else if(sscanf(line,"str %63s %255[^\n]",key,text)==2) { if(!ifw_patch_text(&patch,key,text)) fail("unknown or overlong text parameter"); }
        else if(sscanf(line,"fx %d %63s %lf",&index,key,&v)==3) { if(!ifw_patch_fx(&patch,index,key,v)) fail("unknown effect parameter"); }
        else if(sscanf(line,"bypass %d %lf",&index,&v)==2) { if(index<0||index>4) fail("effect bypass index"); patch.fx_bypass[index]=v!=0; }
        else if(sscanf(line,"tuning %lf",&v)==1) patch.tuning=v;
        else if(sscanf(line,"chorusPhase %lf",&v)==1) patch.chorus_phase=v;
        else if(!strncmp(line,"editnum ",8) || !strncmp(line,"editstr ",8)) {
            if(count==4096) fail("too many events");
            Event *e=&events[count]; e->a=0; e->b=0;
            if(!strncmp(line,"editnum ",8)) {
                strcpy(e->type,"editnum");
                if(sscanf(line,"editnum %u %63s %lf",&e->frame,e->key,&e->a)!=3 || !isfinite(e->a)) fail("bad live edit");
            } else {
                strcpy(e->type,"editstr");
                if(sscanf(line,"editstr %u %63s %63[^\n]",&e->frame,e->key,e->text)!=3) fail("bad live edit");
            }
            if(count && e->frame<events[count-1].frame) fail("events must be stably sorted by frame");
            count++;
        }
        else if(!strncmp(line,"event ",6)) {
            if(count==4096) fail("too many events");
            Event *e=&events[count]; e->a=0; e->b=1;
            if(sscanf(line,"event %u %15s %lf %lf",&e->frame,e->type,&e->a,&e->b)<2) fail("bad event");
            if(!isfinite(e->a) || !isfinite(e->b)) fail("non-finite event value");
            if(count && e->frame<events[count-1].frame) fail("events must be stably sorted by frame");
            if((!strcmp(e->type,"noteOn") || !strcmp(e->type,"noteOff")) &&
                (e->a<0 || e->a>127 || floor(e->a)!=e->a)) fail("MIDI note outside 0..127");
            count++;
        } else fail("invalid scenario line");
    }
    fclose(f);
    if(!isfinite(sr) || !isfinite(breath) || !isfinite(patch.tuning) || !isfinite(patch.chorus_phase) ||
       sr<22050 || sr>96000 || frames==0 || frames>10000000) fail("invalid rate or frame count");
    IFWSynth *s=calloc(1,sizeof *s); if(!s || !ifw_init(s,sr,&patch)) fail("allocation failed");
    ifw_set_dry(s,(int)dry); ifw_breath(s,breath);
    char path[4096]; if(strlen(argv[2])>4000) fail("output prefix too long");
    snprintf(path,sizeof path,"%s.f32",argv[2]); FILE *pcm=fopen(path,"wb"); if(!pcm) fail("open PCM output");
    FILE *state=NULL;
    if(trace) { snprintf(path,sizeof path,"%s.state.f64",argv[2]); state=fopen(path,"wb"); if(!state) fail("open trace output"); }
    float l[1024],r[1024],stereo[2048]; uint32_t offset=0; unsigned event=0;
    while(offset<frames) {
        while(event<count && events[event].frame<=offset) dispatch(s,&events[event++]);
        uint32_t n=trace?1:(uint32_t)block;
        if(n>frames-offset) n=frames-offset;
        if(event<count && n>events[event].frame-offset) n=events[event].frame-offset;
        ifw_process(s,l,r,n);
        for(unsigned i=0;i<n;i++) { stereo[2*i]=l[i]; stereo[2*i+1]=r[i]; }
        if(fwrite(stereo,sizeof(float),n*2,pcm)!=n*2) fail("PCM write failed");
        if(trace && ((offset&15)==0)) { double values[IFW_TRACE_COUNT]; ifw_trace(s,values);
            if(fwrite(values,sizeof(double),IFW_TRACE_COUNT,state)!=IFW_TRACE_COUNT) fail("trace write failed"); }
        offset+=n;
    }
    if(fclose(pcm)) fail("PCM close failed");
    if(state && fclose(state)) fail("trace close failed");
    ifw_destroy(s); free(s); return 0;
}
