/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef IFW_FIXED_SYNTH_H
#define IFW_FIXED_SYNTH_H
#include "param_ids.h"
#include "math.h"
#include "envelope.h"
#include "../control/mono.h"
enum { IFW_FX_Bypass,IFW_FX_Stereo,IFW_FX_Speed,IFW_FX_Feedback,IFW_FX_Lowpass,IFW_FX_WetDry,
       IFW_FX_Rate,IFW_FX_Depth,IFW_FX_Mix,IFW_FX_Time,IFW_FX_Highpass,IFW_FX_Size,IFW_FX_HighDamp,IFW_FX_Width,IFW_FX_COUNT };
enum { IFW_SRC_NONE,IFW_SRC_BREATH,IFW_SRC_VELOCITY,IFW_SRC_NOTE,IFW_SRC_BEND,IFW_SRC_VIBRATO,
       IFW_SRC_GLIDE,IFW_SRC_GATE,IFW_SRC_EG,IFW_SRC_LFO=12,IFW_SRC_SAW=14,IFW_SRC_WAVE=18,
       IFW_SRC_PWM=22,IFW_SRC_NOISE=26,IFW_SRC_BIPOLAR=128 };
#define IFW_MOD_EG IFW_PARAM_COUNT
#define IFW_MOD_LFO (IFW_PARAM_COUNT+4)
#define IFW_MOD_FM (IFW_PARAM_COUNT+6)
#define IFW_MOD_PITCH (IFW_PARAM_COUNT+10)
#define IFW_MOD_COUNT (IFW_PARAM_COUNT+14)
typedef struct {
    int32_t v[IFW_PARAM_COUNT]; /* numbers Q16, textual choices encoded as small enums */
    int32_t fx[5][IFW_FX_COUNT];
    int32_t tuning,chorus_phase; /* Q16 Hz / Q16 radians */
    uint32_t fx_bypass;
} IFWFixedPatch;
typedef struct { int32_t z[4],last,alpha,res;uint8_t type; } IFWFixedFilter;
typedef struct { int32_t z[24],last,a,mix,feedback;uint8_t enabled,stages; } IFWFixedFilter3;
typedef struct { uint8_t a,b;int16_t dest;int32_t depth; } IFWFixedMod;
#define IFW_DELAY_SAMPLES 22052u
#define IFW_SHORT_SAMPLES 4410u
#define IFW_REVERB_SIDE 12900u
typedef struct {
    int16_t delay[2][2][IFW_DELAY_SAMPLES];
    int32_t enhancer[IFW_SHORT_SAMPLES],chorus[IFW_SHORT_SAMPLES],exciter[2][25];
    int16_t reverb[2][IFW_REVERB_SIDE];
} IFWFixedFxBuffers;
typedef struct {
    IFWFixedFxBuffers *buffers;
    uint32_t enhancer_head,chorus_head,exciter_head[2],delay_head[2],delay_half[2];
    int32_t delay_acc[2][2],delay_lp[4],delay_hp[4],delay_wet[4];
    int32_t phaser[12],phaser_fb[2],phaser_low[2];
    uint32_t reverb_head[2][12];int32_t reverb_store[2][8];
    int32_t eq_coeff[2][5],eq_state[2][2][2];
    uint8_t eq_identity[2];
    uint64_t time_phase;uint32_t time_remainder;
    int32_t phaser_lowpass,delay_lp_coeff[2],delay_hp_coeff[2];
    /* Patch-only coefficients; recomputed by set_patch, never by a sample. */
    uint32_t enhancer_delay[2],phaser_spread,chorus_offset,delay_time[2];
    int32_t phaser_rate,phaser_wet,phaser_feedback;
    int32_t chorus_rate,chorus_amount,chorus_base,chorus_mix;
    int32_t delay_mix[2],delay_feedback[2],reverb_damp,reverb_feedback;
    int32_t reverb_w1,reverb_w2,reverb_dry;
    uint32_t exciter_delay;int32_t exciter_gain,enhancer_mix;
    int32_t sample_left,sample_right;
} IFWFixedEffects;
typedef struct {
    IFWFixedPatch patch;IFWMono mono;
    IFWFixedEnvelope env[4];IFWFixedFilter filter[2];IFWFixedFilter3 filter3;
    IFWFixedEffects effects;IFWFixedMod mods[8];uint32_t mod_count;
    uint8_t fm_index[4][8],fm_count[4];
    int32_t source_cache[28];uint32_t source_valid;
    int32_t mod[IFW_MOD_COUNT],raw_saw[4],raw_pulse[4],secondary[4],secondary_cv[4];
    uint32_t phase[4],master_phase,step[4],lfo_phase[2],lfo_step[2],env_rate[4];uint64_t master_step;
    uint32_t width[4];int32_t saw_level[4],wave_level[4],pulse_level[4],noise_level[4];
    int32_t control_note,pitch,pitch_target,glide_from,velocity,note,held_velocity[128]; /* pitch Q16, levels Q31 */
    int32_t breath,breath_target,breath_step,gate,gate_step,bend,bend_target,glide,glide_smoothed,vibrato;
    uint32_t breath_left,bend_k,glide_k,glide_elapsed,glide_fraction,gate_target;
    uint32_t seed,block_count,note_samples;int32_t noise_value,noise_held,noise_counter;
    uint8_t dry,noise_ready;
    uint32_t saturation_count;
    /* Exact memoization keys: a changed input always runs the original math. */
    int32_t step_pitch[5],step_tuning[5],filter_log[2],filter3_frequency,recip_speed,master_gain;
    uint64_t step_value[5],glide_recip;
    uint8_t step_valid[5],filter_valid[2],filter3_valid,recip_valid;
} IFWFixedSynth;
void ifw_fixed_patch_default(IFWFixedPatch *p);
void ifw_fixed_init(IFWFixedSynth *s,const IFWFixedPatch *p,IFWFixedFxBuffers *buffers);
void ifw_fixed_set_patch(IFWFixedSynth *s,const IFWFixedPatch *p);
void ifw_fixed_note_on(IFWFixedSynth *s,uint32_t note,int32_t velocity_q31);
void ifw_fixed_note_off(IFWFixedSynth *s,uint32_t note);
void ifw_fixed_breath(IFWFixedSynth *s,int32_t breath_q31);
void ifw_fixed_panic(IFWFixedSynth *s);
void ifw_fixed_sample(IFWFixedSynth *s,int32_t *left,int32_t *right); /* Q20 */
void ifw_fixed_process(IFWFixedSynth *s,int32_t *stereo,uint32_t frames); /* Q15 output */
void ifw_fixed_fx_reset(IFWFixedSynth *s);
void ifw_fixed_fx_configure(IFWFixedSynth *s);
void ifw_fixed_fx_mod_configure(IFWFixedSynth *s);
void ifw_fixed_fx_tick(IFWFixedSynth *s,int32_t input);
#endif
