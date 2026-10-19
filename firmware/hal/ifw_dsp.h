/* SPDX-License-Identifier: GPL-3.0-only
 * pi32v2 arithmetic only. No device registers or clock changes.
 */
#ifndef IFW_TARGET_DSP_H
#define IFW_TARGET_DSP_H
#include <stdint.h>
static inline int32_t ifw_target_addsat(int32_t a,int32_t b) {
    int32_t result;
    __asm__("%0 = %1 + %2 (ssat)" : "=r"(result) : "r"(a),"r"(b) : "cc");
    return result;
}
static inline int32_t ifw_target_subsat(int32_t a,int32_t b) {
    int32_t result;
    __asm__("%0 = %1 - %2 (ssat)" : "=r"(result) : "r"(a),"r"(b) : "cc");
    return result;
}
#endif
