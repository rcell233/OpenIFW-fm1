/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_MONO_H
#define IFW_MONO_H
#include <stdint.h>
/* One pitch, ordered held MIDI notes. No voice allocation. */
typedef struct { uint8_t held[128]; uint32_t count; } IFWMono;
void ifw_mono_clear(IFWMono *m);
int ifw_mono_on(IFWMono *m, uint32_t note); /* legato BEFORE duplicate removal */
void ifw_mono_off(IFWMono *m, uint32_t note);
int ifw_mono_current(const IFWMono *m);
#endif
