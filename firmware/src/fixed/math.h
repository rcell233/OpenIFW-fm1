/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_FIXED_MATH_H
#define IFW_FIXED_MATH_H
#include <stdint.h>
#include <limits.h>
#define IFW_Q16 65536
#define IFW_Q20 1048576
#define IFW_PHASE_HALF 2147483648u
#define IFW_MAX_STEP 1932735283u
static inline int32_t ifw_clamp(int32_t x,int32_t lo,int32_t hi) { return x<lo?lo:x>hi?hi:x; }
static inline int32_t ifw_sat(int64_t x) { return x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x; }
static inline int32_t ifw_mul20(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>20); }
static inline int32_t ifw_mul16(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>16); }
static inline int32_t ifw_mul30(int32_t a,int32_t b) { return ifw_sat(((int64_t)a*b)>>30); }
/* Exact software 64/32 division with a 32-bit quotient. Configuration/events only. */
uint32_t ifw_udiv64(uint64_t n,uint32_t d);
int32_t ifw_sdiv64(int64_t n,uint32_t d);
uint32_t ifw_ratio16(uint32_t numerator,uint32_t denominator);
int32_t ifw_sin20(uint32_t phase);
int32_t ifw_triangle20(uint32_t phase);
int32_t ifw_wave20(uint32_t phase,uint32_t step,int type,uint32_t width);
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
