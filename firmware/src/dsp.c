/* SPDX-License-Identifier: GPL-3.0-only
 * A target-only compilation unit: let the compiler inline exact integer math
 * across the DSP modules. Native/WASM builds also test the separate modules.
 */
#include "fixed/math.c"
#define advance ifw_envelope_advance
#include "fixed/envelope.c"
#undef advance
#define advance ifw_effects_advance
#include "fixed/effects.c"
#undef advance
#undef V
#undef M
#include "fixed/synth.c"
#include "control/mono.c"
