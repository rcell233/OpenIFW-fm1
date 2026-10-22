/* SPDX-License-Identifier: GPL-3.0-only
 * Short output transitions. The synthesis, envelope and note stack state are
 * untouched. Linear convex blending cannot add peak level or overflow Q15.
 */
#pragma once
#include <stdint.h>
#define IFW_TRANSITION_FRAMES 64u /* 1.451 ms at 44.1 kHz */
static inline int32_t ifw_audio_blend(int32_t from,int32_t to,uint32_t position) {
    /* Q15 endpoints and weights 0..64: the signed sum fits in 32 bits. */
    return (from*(int32_t)(64u-position)+to*(int32_t)position)>>6;
}
#ifndef IFW_NOTE_TRANSITIONS
#define IFW_NOTE_TRANSITIONS 1
#endif
#if IFW_NOTE_TRANSITIONS != 0 && IFW_NOTE_TRANSITIONS != 1
#error "Unsupported note transition mode"
#endif
typedef struct {int32_t last[2],from[2];uint32_t left,armed;} IFWNoteTransition;
static inline void ifw_note_transition_reset(IFWNoteTransition *t) {
    *t=(IFWNoteTransition){.left=0};
}
static inline void ifw_note_transition_apply(IFWNoteTransition *t,int32_t *out,uint32_t frames) {
#if IFW_NOTE_TRANSITIONS
    if(t->armed) {
        t->from[0]=t->last[0];t->from[1]=t->last[1];
        t->left=IFW_TRANSITION_FRAMES;t->armed=0;
    }
    uint32_t n=t->left<frames?t->left:frames;
    for(uint32_t i=0;i<n;i++) {
        uint32_t position=IFW_TRANSITION_FRAMES-t->left--;
        for(unsigned ch=0;ch<2;ch++)out[2u*i+ch]=ifw_audio_blend(t->from[ch],out[2u*i+ch],position);
    }
#endif
    if(frames){t->last[0]=out[2u*frames-2u];t->last[1]=out[2u*frames-1u];}
}
