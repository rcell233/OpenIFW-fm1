/* SPDX-License-Identifier: GPL-3.0-only
 * Desktop relative DSP benchmark. These timings are NOT measurements of pi32v2.
 * Clock sampling surrounds DSP only; checksum and reporting are outside it.
 */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "synth.h"
#include "presets.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
enum { N=256, BLOCKS=256, REPEATS=3 };
static IFWFixedSynth synth;
static IFWFixedFxBuffers buffers;
static int32_t output[2*N];
static uint64_t ns(void) {struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;}
static int cmp(const void *a,const void *b) {uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return x<y?-1:x>y;}
#include "stress_patch.h"
int main(void) {
    printf("{\"kind\":\"desktop-relative-only\",\"framesPerBlock\":%d,\"blocksPerCase\":%d,\"cases\":[",N,BLOCKS*REPEATS);
    for(unsigned c=0;c<14;c++) {
        IFWFixedPatch p;unsigned factory=c/2;int dry=c&1;
        if(factory<6)make_preset(&p,factory);else perf_stress_patch(&p);
        uint64_t times[REPEATS*BLOCKS],sum=0;unsigned j=0;uint32_t checksum=2166136261u;
        for(int r=0;r<REPEATS;r++) {
            ifw_fixed_init(&synth,&p,&buffers);synth.dry=(uint8_t)dry;
            ifw_fixed_breath(&synth,INT32_MAX);ifw_fixed_note_on(&synth,factory<6?69:108,INT32_MAX);
            for(int k=0;k<64;k++)ifw_fixed_process(&synth,output,N);
            for(int k=0;k<BLOCKS;k++) {
                uint64_t start=ns();ifw_fixed_process(&synth,output,N);uint64_t elapsed=ns()-start;
                times[j++]=elapsed;sum+=elapsed;
                for(int i=0;i<2*N;i++)checksum=(checksum^(uint32_t)output[i])*16777619u;
            }
        }
        qsort(times,j,sizeof *times,cmp);
        printf("%s{\"name\":\"%s\",\"dry\":%s,\"note\":%u,\"meanUs\":%.3f,\"p50Us\":%.3f,\"p95Us\":%.3f,\"p99Us\":%.3f,\"maxUs\":%.3f,\"checksum\":%u}",c?",":"",factory<6?preset_names[factory]:"stress-8FM-filter3-24-allFX",dry?"true":"false",factory<6?69:108,(double)sum/j/1000.,times[j/2]/1000.,times[j*95/100]/1000.,times[j*99/100]/1000.,times[j-1]/1000.,checksum);
    }
    puts("]}");return 0;
}
