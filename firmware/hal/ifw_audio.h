/* SPDX-License-Identifier: GPL-3.0-only
 * Publish CPU stores before a consumer or peripheral can observe ownership.
 * The compiler barrier alone used by the event queue is not a DMA fence.
 */
#pragma once
static inline void ifw_audio_release(void) {__asm__ volatile("csync" ::: "memory");}
static inline void ifw_audio_acquire(void) {__asm__ volatile("csync" ::: "memory");}
