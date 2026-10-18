/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_FIXED_ENVELOPE_H
#define IFW_FIXED_ENVELOPE_H
#include <stdint.h>
/* 44.1 kHz only. CV values use hundredths (0..1000), levels Q31.
 * No heap, float, transcendental functions or 64-bit division.
 * Approximation is independently compared against the JS envelope.
 */
typedef struct {
    uint32_t attack_step,decay_step,release_step,progress,fraction;
    int32_t level,sustain,release_start;
    uint8_t stage,retrigger;
} IFWFixedEnvelope;
void ifw_fixed_env_configure(IFWFixedEnvelope *e,int attack_cv,int decay_cv,
                             int32_t sustain_q31,int release_cv,int retrigger);
void ifw_fixed_env_gate(IFWFixedEnvelope *e,int open,int legato);
void ifw_fixed_env_configure_cv(IFWFixedEnvelope *e,int32_t a,int32_t d,int32_t sustain,int32_t r,int retrigger);
uint32_t ifw_fixed_env_rate(int rate_cv); /* -1000..1000 -> Q18 */
uint32_t ifw_fixed_env_rate_cv(int32_t rate_q16);
int32_t ifw_fixed_env_tick(IFWFixedEnvelope *e,uint32_t rate_q18);
#endif
