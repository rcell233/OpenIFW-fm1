/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "output_level.h"
#ifndef IFW_OUTPUT_BUS
#define IFW_OUTPUT_BUS 1
#endif
enum { IFW_OUTPUT_NORMAL=0, IFW_OUTPUT_LOUD=1 };
enum { IFW_BUS_THRESHOLD=24576, IFW_BUS_KNEE=28672,
       IFW_BUS_CEILING=30720, IFW_BUS_UNITY=16384 };
typedef struct {
    uint32_t envelope, mix, mode;
    /* Read-only lifetime meters, separate from raw engine/underrun counters. */
    uint32_t min_gain, soft_samples, peak;
} IFWOutputBus;
static inline void ifw_output_bus_init(IFWOutputBus *b,uint32_t mode) {
    b->envelope=IFW_BUS_THRESHOLD;
    b->mode=mode==IFW_OUTPUT_LOUD;
    b->mix=b->mode*256u;
    b->min_gain=IFW_BUS_UNITY;b->soft_samples=0;b->peak=0;
}
static inline void ifw_output_bus_mode(IFWOutputBus *b,uint32_t mode) {
    /* Only an explicit OUTPUT menu change moves this ramp. Never notes. */
    b->mode=mode==IFW_OUTPUT_LOUD;
}
void ifw_output_bus_process(IFWOutputBus *b,int32_t *stereo,uint32_t frames,int32_t master_q12);
