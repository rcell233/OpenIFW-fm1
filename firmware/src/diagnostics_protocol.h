/* SPDX-License-Identifier: GPL-3.0-only
 * Read-only OpenIFW performance snapshot, noncommercial SysEx manufacturer 7D.
 * Request F0 7D I F W 01 00 sequence F7; response command 01, word count,
 * then little-endian uint32 words packed into five 7-bit bytes each.
 */
#ifndef IFW_DIAGNOSTICS_PROTOCOL_H
#define IFW_DIAGNOSTICS_PROTOCOL_H
#include <stdint.h>
#define IFW_DIAG_WORDS 32u
#define IFW_DIAG_BYTES (10u+5u*IFW_DIAG_WORDS)
#define IFW_BOOT_WORDS 20u
#define IFW_BOOT_BYTES (10u+5u*IFW_BOOT_WORDS)
static int ifw_diag_sequence(const uint8_t *p,uint32_t n) {
    return n==9u && p[0]==0xf0 && p[1]==0x7d && p[2]=='I' && p[3]=='F' &&
        p[4]=='W' && p[5]==1 && (p[6]==0 || p[6]==2) && p[7]<128u && p[8]==0xf7?p[7]:-1;
}
static inline void ifw_diag_encode_words(uint8_t *p,uint8_t sequence,uint8_t command,uint8_t count,const uint32_t *words) {
    const uint8_t h[]={0xf0,0x7d,'I','F','W',1,command,sequence,count};
    for(unsigned i=0;i<9;i++)p[i]=h[i];
    for(unsigned i=0;i<count;i++)for(unsigned k=0;k<5;k++)p[9+i*5+k]=(uint8_t)((words[i]>>(7*k))&127u);
    p[9u+5u*count]=0xf7;
}
static void ifw_diag_encode(uint8_t *p,uint8_t sequence,const uint32_t words[IFW_DIAG_WORDS]) {
    ifw_diag_encode_words(p,sequence,1,IFW_DIAG_WORDS,words);
}
#endif
