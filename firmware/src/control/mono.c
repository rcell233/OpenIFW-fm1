/* SPDX-License-Identifier: GPL-3.0-only */
#include "mono.h"
void ifw_mono_clear(IFWMono *m) { m->count = 0; }
void ifw_mono_off(IFWMono *m, uint32_t note) {
    uint32_t r, w = 0;
    for (r = 0; r < m->count; r++)
        if (m->held[r] != note) m->held[w++] = m->held[r];
    m->count = w;
}
int ifw_mono_on(IFWMono *m, uint32_t note) {
    int legato = m->count != 0;
    if (note > 127) return legato;
    ifw_mono_off(m,note);
    m->held[m->count++] = (uint8_t)note;
    return legato;
}
int ifw_mono_current(const IFWMono *m) { return m->count ? m->held[m->count-1] : -1; }
