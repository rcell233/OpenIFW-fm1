/* SPDX-License-Identifier: GPL-3.0-only
 * Incremental golden stream. The target build runs this through the real ALNK
 * C/assembly IRQ path while TIMER5 and the UI keep running. No hardware timing
 * claim follows from matching the hashes in an instruction emulator.
 */
#include "synth.h"
#ifndef IFW_DSP_ISR_TEST
#include <string.h>
#include "presets.h"
#endif
typedef struct {
    uint32_t magic, blocks, frames, done, hashes[24];
} IFWIrqCheck;
static volatile IFWIrqCheck irq_check;
static uint32_t irq_case, irq_block, irq_hash[2];
static void irq_stream_init(void) {
    irq_case=irq_block=0;
    irq_check=(IFWIrqCheck){.magic=0x4b435249u};
}
static void irq_hash_word(uint32_t x) {
    irq_hash[0]=(irq_hash[0]^x)*16777619u;
    irq_hash[1]=(irq_hash[1]+x+0x9e3779b9u)*2246822519u;
    irq_hash[1]=(irq_hash[1]<<13)|(irq_hash[1]>>19);
}
static void irq_stream_render(IFWFixedSynth *s,int32_t *out,uint32_t n) {
    if(irq_check.done) {for(uint32_t i=0;i<n*2;i++)out[i]=0;return;}
    uint32_t root=60u+irq_case;
    if(!irq_block) {
        IFWFixedPatch p;make_preset(&p,irq_case%6u);
        ifw_fixed_set_patch(s,&p);s->dry=(uint8_t)(irq_case>=6u);
        ifw_fixed_breath(s,INT32_MAX);ifw_fixed_note_on(s,root,1800000000);
        irq_hash[0]=2166136261u;irq_hash[1]=0x12345678u;
    }
    if(irq_block==16)ifw_fixed_note_on(s,root+4u,1500000000);
    if(irq_block==24)ifw_fixed_breath(s,1073741824);
    if(irq_block==32)ifw_fixed_note_off(s,root+4u);
    if(irq_block==48)ifw_fixed_note_off(s,root);
    ifw_fixed_process(s,out,n);
    for(uint32_t i=0;i<n*2;i++)irq_hash_word((uint32_t)out[i]);
    irq_hash_word((uint32_t)s->pitch);irq_hash_word(s->master_phase);
    for(unsigned i=0;i<4;i++){irq_hash_word(s->phase[i]);irq_hash_word((uint32_t)s->env[i].level);}
    irq_check.frames+=n;irq_check.blocks++;
    if(++irq_block==64u) {
        irq_check.hashes[irq_case*2u]=irq_hash[0];
        irq_check.hashes[irq_case*2u+1u]=irq_hash[1];
        irq_block=0;if(++irq_case==12u)irq_check.done=1;
    }
}
#ifdef IFW_IRQ_STREAM_MAIN
#include <stdio.h>
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static IFWFixedSynth stream_synth;
static IFWFixedFxBuffers stream_buffers;
int main(void) {
    IFWFixedPatch p;make_preset(&p,0);ifw_fixed_init(&stream_synth,&p,&stream_buffers);
    irq_stream_init();int32_t out[512];
    while(!irq_check.done)irq_stream_render(&stream_synth,out,256);
    printf("{\"blocks\":%u,\"frames\":%u,\"hashes\":[",irq_check.blocks,irq_check.frames);
    for(unsigned i=0;i<24;i++)printf("%s%u",i?",":"",irq_check.hashes[i]);
    puts("]}");return 0;
}
#endif
