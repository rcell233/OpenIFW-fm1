/* SPDX-License-Identifier: GPL-3.0-only
 * Read-only boot evidence. Progress lives in .noinit, beside the original
 * boot guard; it never changes the guard decision or writes Flash. */
typedef struct {uint32_t magic,version,stage;} IFWBootProgress;
volatile IFWBootProgress ifw_boot_progress __attribute__((section(".noinit")));
static IFWBootProgress ifw_previous_boot;
static uint32_t ifw_boot_reason;
static void ifw_boot_begin(void) {
    if(ifw_boot_progress.magic==0x42574649u)ifw_previous_boot=ifw_boot_progress;
    ifw_boot_progress.magic=0x42574649u;
    ifw_boot_progress.version=40707u;ifw_boot_progress.stage=1;
}
static void ifw_boot_mark(uint32_t stage) {ifw_boot_progress.stage=stage;}
static void diagnostics_service(void);
