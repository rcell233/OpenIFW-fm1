/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C06000 holds the flash driver and hot DSP code. The flash
 * driver remains a separate output section audited for calls into XIP.
 * .audio_text executes only with flash available; normal calls are allowed.
 * crt0 copies both contiguous sections and enables the same PC-guard range.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x6000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : {
        KEEP(*(.text.crt0))
        KEEP(*(.text.audio_entry))
        *dsp.o(.text.ifw_fixed_capture_sources)
        /* Shared UI math stays near XIP callers, within pi32v2's branch range.
         * Event/configuration code stays cold; per-sample DSP runs in RAM. */
        *dsp.o(.text.ifw_udiv64 .text.ifw_sdiv64 .text.ifw_sin20 .text.ifw_wave20
               .text.ifw_fixed_init .text.ifw_fixed_patch_default .text.ifw_fixed_set_patch
               .text.ifw_fixed_note_on .text.ifw_fixed_note_off .text.ifw_fixed_breath .text.ifw_fixed_panic
               .text.ifw_fixed_fx_reset .text.ifw_fixed_fx_configure .text.ifw_fixed_env_configure
               .text.ifw_fixed_env_configure_cv .text.ifw_mono_on .text.ifw_mono_off
               .text.ifw_mono_current .text.ifw_mono_clear)
        KEEP(*(.text.ifw_target_test))
        *(EXCLUDE_FILE (*dsp.o) .text*)
        /* Per-sample oscillator/phaser reads must not contend for Flash with
         * the UI and input IRQ. These immutable tables use normal .data copy. */
        *(EXCLUDE_FILE (*dsp.o) .rodata*)
        *dsp.o(.rodata.ifw_eq_* .rodata.ifw_note_base .rodata.ifw_default_values
               .rodata.ifw_semitone_fraction .rodata.ifw_octave_fraction
               .rodata.ifw_glide_recip .rodata.attack_step_q31 .rodata.decay_step_q22
               .rodata.rate_q18)
        . = ALIGN(4); _etext = .;
    } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); } > RAMTEXT AT > XIP
    .audio_text : { _audio_start = .; *dsp.o(.text*) . = ALIGN(4); _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    _audio_load = LOADADDR(.audio_text);
    .data : { . = ALIGN(4); _data_start = .;
        *dsp.o(.rodata.ifw_sine_table .rodata.ifw_ap_table .rodata.ifw_ap_half_table
               .rodata.ifw_alpha_table .rodata.ifw_f3_table)
        *(.rodata*) *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
