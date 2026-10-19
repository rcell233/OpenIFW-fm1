/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_FIXED_MATH_H
#define IFW_FIXED_MATH_H
#include <stdint.h>
#include <limits.h>
#ifdef IFW_TARGET
#include "ifw_dsp.h"
#endif
#define IFW_Q16 65536
#define IFW_Q20 1048576
#define IFW_PHASE_HALF 2147483648u
#define IFW_MAX_STEP 1932735283u
static inline int32_t ifw_clamp(int32_t x,int32_t lo,int32_t hi) { return x<lo?lo:x>hi?hi:x; }
static inline int32_t ifw_sat(int64_t x) {
    /* A signed 64-bit value fits iff its upper word is the lower word's sign
     * extension. Avoid two multiword relational comparisons on pi32v2. */
    int32_t lo=(int32_t)(uint32_t)x,hi=(int32_t)((uint64_t)x>>32);
    return hi==(lo>>31)?lo:hi<0?INT32_MIN:INT32_MAX;
}
static inline int32_t ifw_addsat(int32_t a,int32_t b) {
#ifdef IFW_TARGET
    return ifw_target_addsat(a,b);
#else
    return ifw_sat((int64_t)a+b);
#endif
}
static inline int32_t ifw_half(int64_t x) {
    return (int32_t)((x+(int64_t)((uint64_t)x>>63))>>1);
}
static inline int32_t ifw_subsat(int32_t a,int32_t b) {
#ifdef IFW_TARGET
    return ifw_target_subsat(a,b);
#else
    return ifw_sat((int64_t)a-b);
#endif
}
static inline int32_t ifw_mul20(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>20); }
static inline int32_t ifw_mul16(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>16); }
static inline int32_t ifw_mul30(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>30); }
/* Only for a proven bounded product (e.g. |b| <= Q20). */
static inline int32_t ifw_mul20_bounded(int32_t a,int32_t b) { return (int32_t)(((int64_t)a*b)>>20); }
static inline int32_t ifw_mul30_bounded(int32_t a,int32_t b) { return (int32_t)(((int64_t)a*b)>>30); }
/* Exact software 64/32 division with a 32-bit quotient. Configuration/events only. */
uint32_t ifw_udiv64(uint64_t n,uint32_t d);
int32_t ifw_sdiv64(int64_t n,uint32_t d);
uint32_t ifw_ratio16(uint32_t numerator,uint32_t denominator);
int32_t ifw_sin20(uint32_t phase);
int32_t ifw_triangle20(uint32_t phase);
int32_t ifw_wave20(uint32_t phase,uint32_t step,int type,uint32_t width);
void ifw_waves20(uint32_t phase,uint32_t step,int type,uint32_t width,
                 int32_t *saw,int32_t *secondary,int32_t *pulse);
uint64_t ifw_note_step(int32_t note_q16,int32_t tuning_q16);
uint32_t ifw_lfo_step(int32_t log2_cv);
uint32_t ifw_smooth31(int32_t milliseconds_q16);
uint32_t ifw_linear_step31(int32_t milliseconds_q16);
uint32_t ifw_glide_recip31(int32_t speed_q16);
uint64_t ifw_glide_recip39(int32_t speed_q16);
uint64_t ifw_glide_recip39_fine(int32_t speed_q24);
int32_t ifw_filter_alpha30(int32_t log_hz_q16);
int32_t ifw_filter3_a30(int32_t cv_q16);
int32_t ifw_allpass_a30(int32_t hz_q16,int half_rate);
int32_t ifw_soft20(int32_t x);
void ifw_eq_coeff(int32_t gain_q16,int high,int32_t coefficients[5]);
#endif
