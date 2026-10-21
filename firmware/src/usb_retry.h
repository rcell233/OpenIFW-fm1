/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
enum {USB_RETRY_IDLE,USB_RETRY_RESET,USB_RETRY_ATTACH};
typedef struct {uint32_t last,reset_at,waiting;} UsbRetryState;
static uint32_t usb_retry_step(UsbRetryState *s,uint32_t now,int up,int detached) {
    if(up || detached){s->waiting=0;return USB_RETRY_IDLE;}
    if(s->waiting) {
        if(now-s->reset_at<25u)return USB_RETRY_IDLE;
        s->waiting=0;return USB_RETRY_ATTACH;
    }
    if(now-s->last<1000u)return USB_RETRY_IDLE;
    s->last=now;s->reset_at=now;s->waiting=1;return USB_RETRY_RESET;
}
