/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include "output_bus.h"
#include "dsp_irq_stream.c"
#include "output_bus_stream.h"
static IFWFixedSynth s;
static IFWFixedFxBuffers fx;
int main(void) {
    IFWFixedPatch p;make_preset(&p,0);ifw_fixed_init(&s,&p,&fx);
    IFWOutputBus b;ifw_output_bus_init(&b,IFW_OUTPUT_LOUD);irq_stream_init();
    int32_t out[512];
    while(!irq_check.done) {
        irq_stream_render(&s,out,256);
        int32_t master=output_stream_prepare(&b,out);
        ifw_output_bus_process(&b,out,256,master);output_stream_hash(&b,out,256);
    }
    printf("{\"blocks\":%u,\"frames\":%u,\"pcm\":%u,\"state\":%u,\"engineHashes\":[",
           output_bus_check.blocks,irq_check.frames,output_bus_check.pcm,output_bus_check.state);
    for(unsigned i=0;i<24;i++)printf("%s%u",i?",":"",irq_check.hashes[i]);
    puts("]}");return 0;
}
