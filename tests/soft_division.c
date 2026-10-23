/* SPDX-License-Identifier: GPL-3.0-only
 * Independent original divide-based reference. The optional full sweep checks
 * every int32 input; normal CI covers every normalized divisor and boundaries.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "soft_division.h"
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void){return "detect_leaks=0";}
#endif
static int32_t original_soft20(int32_t x) {
    if(!x)return 0;
    uint32_t a=x<0?(uint32_t)(-(int64_t)x):(uint32_t)x;
    uint32_t d=65536u+(uint32_t)(((uint64_t)a*983u)>>20);
    uint32_t whole=a/d,rem=a-whole*d,den=d,shift=0;
    while(den>65535u){den>>=1;shift++;}
    uint32_t fraction=rem>=d?65536u:(rem<<(16u-shift))/den;
    int32_t y=(int32_t)((whole<<16)+fraction);return x<0?-y:y;
}
int main(int argc,char **argv) {
    uint64_t divisions=0,soft=0;uint32_t rng=0x92370abdu,hash=2166136261u;
    for(uint32_t d=32768;d<65536;d++) {
        uint32_t reciprocal=ifw_recip16_exact(d);
        assert(reciprocal==UINT64_C(4294967296)/d);
        for(unsigned k=0;k<96;k++) {
            uint32_t q=k<32?k:k<64?(UINT32_C(1)<<(k-32)):UINT32_MAX/d-(k-64);
            uint64_t edge=(uint64_t)q*d;
            if(edge>UINT32_MAX)continue;
            for(int delta=-1;delta<=1;delta++) {
                if((delta<0&&!edge)||edge+(unsigned)(delta>0)>UINT32_MAX)continue;
                uint32_t n=(uint32_t)(edge+delta);
                assert(ifw_div_recip16(n,d,reciprocal)==n/d);divisions++;
            }
        }
        assert(ifw_div_recip16(UINT32_MAX,d,reciprocal)==UINT32_MAX/d);divisions++;
    }
    uint64_t count=argc>1&&!strcmp(argv[1],"--full")?UINT64_C(4294967296):UINT64_C(2097152);
    for(uint64_t i=0;i<count;i++) {
        uint32_t bits;
        if(count>UINT32_MAX)bits=(uint32_t)i;
        else {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;bits=rng;}
        int32_t x=(int32_t)bits,a=ifw_soft20(x),b=original_soft20(x);
        if(a!=b){fprintf(stderr,"mismatch %d: %d vs %d\n",x,a,b);return 1;}
        hash=(hash^(uint32_t)a)*16777619u;soft++;
        if(count>UINT32_MAX && !(i&UINT32_C(268435455)))
            fprintf(stderr,"checked %llu / 4294967296 int32 inputs\n",(unsigned long long)i);
    }
    printf("{\"divisors\":32768,\"divisionChecks\":%llu,\"softChecks\":%llu,\"hash\":%u,\"fullInt32Domain\":%s}\n",
           (unsigned long long)divisions,(unsigned long long)soft,hash,count>UINT32_MAX?"true":"false");
    return 0;
}
