/* SPDX-License-Identifier: GPL-3.0-only
 * A shared 4-comb/2-allpass room tank, at 22.05 kHz. The two diffuser
 * outputs provide different stereo taps; Width still controls their blend.
 * Signal state is Q15. A comb read/store is within int16 range, so
 * |(store-value)*damp| <= 65535*32768 < INT32_MAX. Feedback is <=32767.
 * Truncate feedback towards zero so an inaudible negative LSB cannot ring
 * forever. No heap, division, coefficients or variable topology in this loop.
 */
static void reverb_half_tick(IFWFixedEffects *e,IFWFixedFxBuffers *b,int32_t input) {
    int32_t wet[2]={0,0},damp=e->reverb_damp15,feedback=e->reverb_feedback15;
#define HALF_COMB(CH,I,N,OFF) do { \
    uint32_t head=e->reverb_head[CH][I];int16_t *line=b->reverb[CH]+(OFF); \
    int32_t value=line[head]; \
    int32_t store=value+ifw_shr0((e->reverb_store[CH][I]-value)*damp,15); \
    e->reverb_store[CH][I]=store; \
    line[head]=(int16_t)ifw_clamp(input+ifw_shr0(store*feedback,15),-32768,32767); \
    wet[CH]+=value;e->reverb_head[CH][I]=advance(head,N); \
    } while(0)
#define HALF_ALLPASS(CH,I,N,OFF) do { \
    uint32_t head=e->reverb_head[CH][I];int16_t *line=b->reverb[CH]+(OFF); \
    int32_t value=line[head],in=wet[CH]; \
    wet[CH]=value-in;line[head]=(int16_t)ifw_clamp(in+ifw_shr0(value,1),-32768,32767); \
    e->reverb_head[CH][I]=advance(head,N); \
    } while(0)
    HALF_COMB(0,0,558,0);
    HALF_COMB(0,1,594,558);
    HALF_COMB(0,2,639,1152);
    HALF_COMB(0,3,678,1791);
    HALF_ALLPASS(0,8,278,2469);
    wet[1]=wet[0];
    HALF_ALLPASS(0,9,221,2747);
#undef HALF_COMB
#undef HALF_ALLPASS
    /* Calibrate the smaller tank's wet level. The first diffuser tap has
     * less energy than the second, hence its 1.5x correction. Dry gain and
     * patch WetDry/Width retain their existing mapping. */
    e->reverb_wet[0]=wet[0]*4;e->reverb_wet[1]=wet[1]*6;
}
_Static_assert(2968u==IFW_REVERB_SIDE,"Room reverb differs from its pool allocation");
