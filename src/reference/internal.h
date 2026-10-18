/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_REFERENCE_INTERNAL_H
#define IFW_REFERENCE_INTERNAL_H
#include "engine.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#define TAU 6.283185307179586476925286766559
#define N(key) (s->p.num[P_##key])
#define T(key) (s->p.text[P_##key])
#define O(i,key) (s->p.num[P_Osc1##key+(i)*IFW_OSC_STRIDE])
#define OT(i,key) (s->p.text[P_Osc1##key+(i)*IFW_OSC_STRIDE])
#define F(i,key) (s->p.num[P_Filter1##key+(i)*IFW_FILTER_STRIDE])
#define FT(i,key) (s->p.text[P_Filter1##key+(i)*IFW_FILTER_STRIDE])
#define E(i,key) (s->p.num[P_Eg1##key+(i)*IFW_EG_STRIDE])
#define L(i,key) (s->p.num[P_Lfo1##key+(i)*IFW_LFO_STRIDE])
#define LT(i,key) (s->p.text[P_Lfo1##key+(i)*IFW_LFO_STRIDE])
static inline double clip(double x,double a,double b) { return x<a?a:x>b?b:x; }
static inline double nz(double x) { return isnan(x)?0:x; } /* JavaScript x || 0 */
static inline double soft(double x) { return x/(1+0.015*fabs(x)); }
static inline double hz(double n,double tuning) { return tuning*pow(2,(n-69)/12); }
static inline int is_on(const IFWPatch *p,int id) {
    return p->num[id]==1 || !strcmp(p->text[id],"On") || !strcmp(p->text[id],"1");
}
int ifw_effects_init(IFWSynth *s);
void ifw_effects_destroy(IFWSynth *s);
void ifw_effects_configure(IFWSynth *s);
void ifw_effects_tick(IFWSynth *s,double x,double *l,double *r);
#endif
