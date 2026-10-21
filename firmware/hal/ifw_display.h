/* SPDX-License-Identifier: GPL-3.0-only
 * Nonblocking readiness check for the existing platform LCD DMA. No clock,
 * GPIO, transfer format or buffer ownership changes.
 */
#ifndef IFW_DISPLAY_H
#define IFW_DISPLAY_H
static inline int ifw_display_ready(void) {
    return (FM1_LCD_SPI_CON & 0x8000u)!=0;
}
#endif
