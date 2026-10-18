/* SPDX-License-Identifier: GPL-3.0-only
 * Desktop double-precision oracle port of OpenIFW a478abd (MIT).
 * This API deliberately cannot be linked into the FM-1 target.
 */
#ifndef IFW_REFERENCE_ENGINE_H
#define IFW_REFERENCE_ENGINE_H
#ifdef IFW_TARGET_FM1
#error "The desktop reference uses double and heap memory. Use the fixed-point target."
#endif
#include <stdint.h>
#include "params.h"
#include "../../firmware/src/control/mono.h"
enum { FX_Bypass, FX_Stereo, FX_Speed, FX_Feedback, FX_Lowpass, FX_WetDry,
       FX_Rate, FX_Depth, FX_Mix, FX_Time, FX_Highpass, FX_Size, FX_HighDamp, FX_Width, FX_COUNT };
typedef struct {
    double num[IFW_PARAM_COUNT];
    char text[IFW_PARAM_COUNT][64];
    double fx[5][FX_COUNT];
    int fx_bypass[5];
    double tuning, chorus_phase;
} IFWPatch;
typedef struct { double a,d,r,s,level,time,release_start; int stage,retrigger; } IFWEnvelope;
typedef struct { double z[4],last,a,res; int type; } IFWFilter;
typedef struct { double z[24],last,a,mix,feedback; int enabled,stages; } IFWFilter3;
typedef struct { float *data; uint32_t length,head; double store; } IFWDelay;
typedef struct { double b0,b1,b2,a1,a2,z1,z2; } IFWBiquad;
typedef struct {
    IFWDelay chorus,enhancer,delay[4],comb[2][8],diff[2][4],exciter[2];
    double phase,delay_lp[4],delay_hp[4],phaser[12],phaser_fb[2],phaser_low[2];
    IFWBiquad low[2],high[2];
} IFWEffects;
typedef struct {
    int dest; char source[2][64]; double depth;
} IFWMod;
typedef struct {
    IFWPatch p;
    double sr, phase[4],secondary[4],secondary_cv[4],raw_saw[4],raw_pulse[4];
    IFWFilter filter[2]; IFWFilter3 filter3; IFWEnvelope env[4]; IFWEffects effects;
    IFWMono mono; double held_velocity[128];
    double note,pitch,pitch_target,velocity,breath,breath_target,bend,bend_target;
    double vibrato,glide,gate,gate_target,gate_step,breath_step;
    double time,note_time,lfo_phase[2],master_phase,frequency,mod[IFW_PARAM_COUNT+14];
    double noise_value,noise_held,glide_elapsed,glide_from,glide_smoothed;
    uint32_t seed,block_count,breath_left;
    int noise_counter,dry,mod_count; IFWMod mods[8];
} IFWSynth;
void ifw_patch_default(IFWPatch *p);
int ifw_patch_number(IFWPatch *p,const char *key,double v);
int ifw_patch_text(IFWPatch *p,const char *key,const char *v);
int ifw_patch_fx(IFWPatch *p,int index,const char *key,double v);
int ifw_init(IFWSynth *s,double sr,const IFWPatch *p);
void ifw_destroy(IFWSynth *s);
void ifw_set_patch(IFWSynth *s,const IFWPatch *patch); /* preserve phases, gate and envelope stages */
void ifw_set_dry(IFWSynth *s,int dry);
void ifw_breath(IFWSynth *s,double v);
void ifw_note_on(IFWSynth *s,unsigned note,double velocity);
void ifw_note_off(IFWSynth *s,unsigned note);
void ifw_panic(IFWSynth *s);
void ifw_process(IFWSynth *s,float *left,float *right,uint32_t n);
double ifw_wave(double phase,double dt,const char *type,double width);
double ifw_attack_seconds(double v);
double ifw_decay_seconds(double v);
double ifw_native_decay(double u);
/* State layout shared with tests: 36 doubles per sample. */
#define IFW_TRACE_COUNT 36
void ifw_trace(const IFWSynth *s,double state[IFW_TRACE_COUNT]);
#endif
