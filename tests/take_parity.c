/* SPDX-License-Identifier: GPL-3.0-only */
/* Replays controller data through the production application, not directly
 * into DSP state. Input lines: audio sample index, status, data1, data2. */
#define IFW_HOST
#define main panel_host_main
#include "../firmware/src/app.c"
#undef main
int main(int argc,char **argv){
    if(argc!=5)return 2;
    unsigned frames=(unsigned)strtoul(argv[1],NULL,10),step=(unsigned)strtoul(argv[2],NULL,10);
    unsigned fx=(unsigned)strtoul(argv[3],NULL,10);
    if(!step || !frames || fx>2)return 2;
    FILE *f=fopen(argv[4],"wb");if(!f)return 2;
    app_init();send_breath(0); /* the recording started at lastBreathRaw == 0 */
    if(fx==0)synth.dry=1;else if(fx==1)synth.patch.fx_bypass|=16u;
    unsigned pos,s,a,b;int have=scanf("%u %u %u %u",&pos,&s,&a,&b);
    for(unsigned i=0;i<frames;i++){
        while(have==4 && pos==i){routed_message(1,s,a,b);have=scanf("%u %u %u %u",&pos,&s,&a,&b);}
        if(i%step==0)app_audio_events();
        int32_t l,r; ifw_fixed_sample(&synth,&l,&r);
        int32_t state[]={l,r,synth.pitch,synth.control_note,synth.gate,synth.breath,synth.env[0].level,synth.env[1].level,synth.env[2].level,synth.env[3].level,(int32_t)synth.mono.count,synth.gate_target};
        if(fwrite(state,sizeof state,1,f)!=1)return 3;
    }
    if(have==4)return 4;
    return fclose(f)!=0;
}
