/* SPDX-License-Identifier: GPL-3.0-only
 * Nonblocking form of Dinghy/Felucca's polled SARADC transaction.
 * Only MASTER is used by this application; conversion ownership is main-only.
 */
#pragma once
#include "fm1_adc.h"
static void ifw_adc_start(uint32_t ch) {
    FM1_ADC_CON=0;
    if(FM1_WLA_CON0&(1u<<14))FM1_WLA_CON0&=~(1u<<14);
    FM1_ADC_CON=0xF04Eu|((ch&15u)<<8);
    FM1_ADC_CON|=0x10u;FM1_ADC_CON|=0x40u;
}
static void ifw_adc_cancel(void) {FM1_ADC_CON=0x40u;FM1_ADC_CON=0;}
static int32_t ifw_adc_take(void) {
    if(!(FM1_ADC_CON&0x80u))return -1;
    int32_t v=(int32_t)(FM1_ADC_RES&0x3ffu);ifw_adc_cancel();return v;
}
