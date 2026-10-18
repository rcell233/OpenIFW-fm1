/* SPDX-License-Identifier: GPL-3.0-only */
#ifdef IFW_TARGET
void *memcpy(void *dst,const void *src,unsigned bytes);
void *memset(void *dst,int value,unsigned bytes);
#else
#include <string.h>
#endif
