/* SPDX-License-Identifier: GPL-3.0-only
 * Final output calibration, shared by the board and its browser host.
 * The synth/FX supply bounded Q15 PCM. Combine the +6.02 dB calibration
 * with MASTER before the existing output clamp, so turning MASTER down
 * retains headroom. No oscillator, effect input or patch value is changed.
 */
#pragma once
#include <stdint.h>
#ifndef IFW_OUTPUT_GAIN
#define IFW_OUTPUT_GAIN 2
#endif
#if IFW_OUTPUT_GAIN != 1 && IFW_OUTPUT_GAIN != 2
#error "Only the production x2 and test-only legacy x1 output levels are supported"
#endif
static inline int32_t ifw_output_sample(int32_t pcm,int32_t master_q12) {
    /* |pcm| <= 32768 and MASTER 0..4096: the product fits int32_t.
     * x2 is one less right-shift, with no extra per-sample arithmetic.
     * x1 exists only for the unpublished historical PCM reference. */
    int32_t v=(pcm*master_q12)>>(IFW_OUTPUT_GAIN==2?11:12);
    return v>32767?32767:v< -32767? -32767:v;
}
