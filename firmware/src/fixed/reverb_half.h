/* SPDX-License-Identifier: GPL-3.0-only
 * A shared 4-comb/2-allpass room tank, at 22.05 kHz. The two diffuser
 * outputs provide different stereo taps; Width still controls their blend.
 * Production signal state is Q23 in int32 delay memory. The Q15 mode is
 * test-only: it reproduces rc3/rc4, including their low-level quantization.
 * Keep the original normalized headroom, coefficients and wet/dry mapping.
 * Products truncate towards zero so a negative LSB cannot ring forever.
 * No heap, division, coefficients or variable topology in this loop.
 */
static inline int32_t reverb_mul15(int32_t value,int32_t coefficient) {
#if IFW_REVERB_BITS == 15
    /* |value| <= 65535, coefficient <= 32768: signed 32-bit product fits. */
    return ifw_shr0(value*coefficient,15);
#else
    /* The damp difference is <=2^24, feedback input <=2^23.
     * A wide multiply retains all Q23 state bits; no wide division is used. */
    int64_t product=(int64_t)value*coefficient;
    return (int32_t)((product+((product>>63)&32767))>>15);
#endif
}
static inline int32_t reverb_from20(int32_t value) {
#if IFW_REVERB_BITS == 23
    /* The bounded stereo downmix is <2^26, so x8 fits int32_t. */
    return value*8;
#else
    return ifw_shr0(value,5);
#endif
}
static inline int32_t reverb_to20(int32_t value) {
#if IFW_REVERB_BITS == 23
    return ifw_shr0(value,3);
#else
    return value*32;
#endif
}
#define REVERB_LIMIT (1<<IFW_REVERB_BITS)
static void reverb_half_tick(IFWFixedEffects *e,IFWFixedFxBuffers *b,int32_t input) {
    int32_t wet[2]={0,0},damp=e->reverb_damp15,feedback=e->reverb_feedback15;
#define HALF_COMB(CH,I,N,OFF) do { \
    uint32_t head=e->reverb_head[CH][I];IFWReverbSample *line=b->reverb[CH]+(OFF); \
    int32_t value=line[head]; \
    int32_t store=value+reverb_mul15(e->reverb_store[CH][I]-value,damp); \
    e->reverb_store[CH][I]=store; \
    line[head]=(IFWReverbSample)ifw_clamp(input+reverb_mul15(store,feedback),-REVERB_LIMIT,REVERB_LIMIT-1); \
    wet[CH]+=value;e->reverb_head[CH][I]=advance(head,N); \
    } while(0)
#define HALF_ALLPASS(CH,I,N,OFF) do { \
    uint32_t head=e->reverb_head[CH][I];IFWReverbSample *line=b->reverb[CH]+(OFF); \
    int32_t value=line[head],in=wet[CH]; \
    wet[CH]=value-in;line[head]=(IFWReverbSample)ifw_clamp(in+ifw_shr0(value,1),-REVERB_LIMIT,REVERB_LIMIT-1); \
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
#undef REVERB_LIMIT
_Static_assert(2968u==IFW_REVERB_SIDE,"Room reverb differs from its pool allocation");
