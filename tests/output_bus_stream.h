/* SPDX-License-Identifier: GPL-3.0-only
 * Unpublished target/native output regression. Runs after the existing raw
 * engine checksums, so those frozen checks remain independent and unchanged.
 */
static volatile struct {uint32_t magic,blocks,pcm,state;} output_bus_check;
static int32_t output_stream_prepare(IFWOutputBus *b,int32_t *out) {
    if(!output_bus_check.magic)output_bus_check.magic=0x42574649u;
    if(irq_check.blocks==128u || irq_check.blocks==512u)ifw_output_bus_mode(b,0);
    if(irq_check.blocks==384u || irq_check.blocks==640u)ifw_output_bus_mode(b,1);
    /* Overload and sign edges in addition to the original 12 factory scenes. */
    if(!(irq_check.blocks&15u)){out[0]=-32768;out[1]=32767;}
    static const int32_t masters[]={4096,2048,1,0};
    return masters[(irq_check.blocks/32u)&3u];
}
static void output_stream_hash(const IFWOutputBus *b,const int32_t *out,uint32_t n) {
    if(output_bus_check.blocks>=768u)return;
    uint32_t pcm=output_bus_check.pcm,state=output_bus_check.state;
    for(uint32_t i=0;i<n*2u;i++)pcm=(pcm^(uint32_t)out[i])*16777619u;
    const uint32_t words[]={b->envelope,b->mix,b->mode,b->min_gain,b->soft_samples,b->peak};
    for(unsigned i=0;i<6;i++)state=(state^words[i])*16777619u;
    output_bus_check.pcm=pcm;output_bus_check.state=state;output_bus_check.blocks++;
}
