/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/src/fixed/synth.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) {return "detect_leaks=0";}
#endif
typedef struct {uint32_t frame,type;int32_t a,b;} Event;
static void trace(IFWFixedSynth *s,uint32_t frame,double a[36]) {
    int n=0;
    a[n++]=s->note;a[n++]=s->pitch/65536.;a[n++]=s->pitch_target/65536.;a[n++]=s->velocity/2147483647.;
    a[n++]=s->gate/2147483647.;a[n++]=s->gate_target;a[n++]=s->breath/2147483647.;a[n++]=s->breath_target/2147483647.;
    a[n++]=s->bend/2147483647.;a[n++]=s->glide/2147483647.;a[n++]=s->glide_smoothed/2147483647.;a[n++]=s->glide_elapsed/44100.;
    a[n++]=s->glide_from/65536.;a[n++]=s->note_samples/44100.;
    for(int i=0;i<4;i++)a[n++]=s->env[i].level/2147483647.;
    for(int i=0;i<4;i++)a[n++]=s->env[i].stage;
    for(int i=0;i<2;i++)a[n++]=s->lfo_phase[i]/4294967296.;
    for(int i=0;i<4;i++)a[n++]=s->phase[i]/4294967296.;
    a[n++]=s->master_phase/4294967296.;a[n++]=s->seed;a[n++]=s->noise_held/1048576.;a[n++]=s->noise_counter;
    a[n++]=s->mono.count;a[n++]=s->breath_left;a[n++]=s->block_count;a[n++]=(frame+1)/44100.;
    assert(n==36);
}
int main(int argc,char **argv) {
    assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f);uint32_t h[5];
    assert(fread(h,sizeof h,1,f)==1 && h[0]==0x32584649 && h[1]<10000000 && h[4]<4096);
    IFWFixedPatch p;assert(sizeof p==916 && fread(&p,sizeof p,1,f)==1);
    Event *events=calloc(h[4]+1,sizeof *events);assert(events);assert(fread(events,sizeof *events,h[4],f)==h[4]);fclose(f);
    IFWFixedSynth *s=calloc(1,sizeof *s);IFWFixedFxBuffers *b=calloc(1,sizeof *b);assert(s&&b);
    ifw_fixed_init(s,&p,b);s->dry=h[2];ifw_fixed_breath(s,(int32_t)h[3]);
    char path[1024];snprintf(path,sizeof path,"%s.f32",argv[2]);FILE *pcm=fopen(path,"wb");assert(pcm);
    snprintf(path,sizeof path,"%s.state.f64",argv[2]);FILE *state=fopen(path,"wb");assert(state);uint32_t j=0;
    for(uint32_t i=0;i<h[1];i++) {
        while(j<h[4] && events[j].frame<=i) {Event *e=&events[j++];switch(e->type) {
        case 0:ifw_fixed_note_on(s,(uint32_t)e->a,e->b);break;
        case 1:ifw_fixed_note_off(s,(uint32_t)e->a);break;
        case 2:ifw_fixed_breath(s,e->a);break;
        case 3:s->bend_target=e->a;break;case 4:s->glide=e->a;break;case 5:s->vibrato=e->a;break;
        case 6:ifw_fixed_panic(s);break;case 7:s->dry=e->a;break;
        case 8:p.v[e->a]=e->b;ifw_fixed_set_patch(s,&p);break;default:abort();}}
        int32_t l,r;ifw_fixed_sample(s,&l,&r);float out[2]={l/1048576.f,r/1048576.f};assert(fwrite(out,sizeof out,1,pcm)==1);
        if((i&15u)==0) {double a[36];trace(s,i,a);assert(fwrite(a,sizeof a,1,state)==1);}
    }
    fclose(pcm);fclose(state);free(events);free(b);free(s);return 0;
}
