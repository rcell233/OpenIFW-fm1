#!/usr/bin/env python3
"""Run the production ISR body against a sample-clocked fake DMA.

Render duration is an injected input, not a hardware performance estimate.
This isolates pending-bit clearing, DMA ownership, and synth-clock progression
from oscillator mathematics and from the third-party instruction emulator.
"""
import hashlib,json,math,os,subprocess,sys,wave
from array import array
from pathlib import Path
try:
    import numpy as np  # optional: enrich the report with the strongest FFT bin
except ImportError:
    np=None

ROOT=Path(__file__).resolve().parents[1]
pipeline='--pipeline' in sys.argv
OUT=ROOT/('build/target-audit/dma-pipeline' if pipeline else 'build/target-audit/dma');OUT.mkdir(parents=True,exist_ok=True)
platform=(ROOT/'vendor/dinghy/firmware/src/platform.c').read_text()
body=platform.split('void fm1_alnk0_irq(void)',1)[1].split('extern void isr_alnk0(void);',1)[0]
body='void dut_irq(void)'+body
if pipeline:
    body='''
#define IFW_HOST 1
#include "firmware/src/audio_perf.h"
static uint32_t fm1_ticks(void){return (uint32_t)(now*24000000./44100.);}
#define fm1_alnk0_irq dut_irq
#include "firmware/src/audio_output.h"
'''
source=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math.h"
#define HALF_FRAMES 256u
#define HALF_WORDS 512u
#define FM1_AUDIO_HALF 0x80u
static int32_t abuf[1024],master_q12=4096;
static uint8_t in_audio,pending;
static uint32_t frame,rendered,phase,irqs,raw_dma_words;
static double now,frame_cost;
static FILE *pcm;
static void advance(double until) {
    while((double)(frame+1)<=until) {
        if(abuf[(frame%512u)*2u]%128)raw_dma_words++;
        int16_t v=(int16_t)(abuf[(frame%512u)*2u]/128);
        if(pcm)fwrite(&v,2,1,pcm);
        frame++;
        if(frame%256u==0)pending=FM1_AUDIO_HALF;
    }
    now=until;
}
static uint8_t fm1_audio_pending(void){return pending;}
static void fm1_audio_ack_aux(uint8_t p){(void)p;}
static uint32_t fm1_audio_free_half(void){return ((frame/256u)&1u)^1u;}
static void fm1_audio_ack_half(void){pending=0;}
static void uac_render_start(void){}
static void uac_tap(const int32_t *p,uint32_t n){(void)p;(void)n;}
static void app_audio(int32_t *out,uint32_t n){
    for(uint32_t i=0;i<n;i++){
        advance(now+frame_cost);
        int32_t v=ifw_sin20(phase)/128;
        phase+=42852281u;out[i*2u]=out[i*2u+1u]=v;rendered++;
    }
}
'''+body+r'''
int main(int argc,char **argv){
    (void)argc;frame_cost=strtod(argv[1],0);pcm=fopen(argv[2],"wb");
    while(frame<44100u*4u){
        if(pending){dut_irq();irqs++;}
        else advance((double)((frame/256u+1u)*256u));
    }
    fclose(pcm);
    printf("{\"ratio\":%.4f,\"playedFrames\":%u,\"synthFrames\":%u,\"irqs\":%u,\"rawDmaWords\":%u}",frame_cost,frame,rendered,irqs,raw_dma_words);
    return 0;
}
'''
(OUT/'harness.c').write_text(source)
subprocess.run([os.environ.get('CC','cc'),'-O2','-std=c11','-I.','-Ifirmware/src/fixed',
 str(OUT/'harness.c'),'firmware/src/fixed/math.c','-o',str(OUT/'harness')],cwd=ROOT,check=True)
results=[]
def tone_fraction(samples,hz):
    coefficient=2*math.cos(2*math.pi*hz/44100);s1=s2=energy=0.
    for x in samples:
        s0=x+coefficient*s1-s2;s2=s1;s1=s0;energy+=x*x
    power=max(0.,s1*s1+s2*s2-coefficient*s1*s2)
    return 2*power/(len(samples)*energy) if energy else 0.
for ratio in [.75,.99,1.01,1.25,1.8]:
    raw=OUT/f'{ratio:.2f}.s16'
    result=json.loads(subprocess.check_output([OUT/'harness',str(ratio),str(raw)],cwd=ROOT,text=True))
    pcm=array('h');pcm.frombytes(raw.read_bytes())
    if sys.byteorder!='little':pcm.byteswap()
    window=pcm[44100:132300]
    result['tone440EnergyFraction']=tone_fraction(window,440)
    if np is not None:
        samples=np.array(window,dtype=float)
        spectrum=np.abs(np.fft.rfft(samples*np.hanning(len(samples))))
        freq=np.fft.rfftfreq(len(samples),1/44100)
        candidates=np.flatnonzero((freq>=30)&(freq<=1000))
        result['strongestHz']=float(freq[candidates[np.argmax(spectrum[candidates])]])
    result['effectiveSynthHz']=result['synthFrames']/result['playedFrames']*44100
    result['deadlineMs']=256/44100*1000
    result['injectedRenderMs']=ratio*result['deadlineMs']
    if pipeline:
        expected=44100/max(1,ratio)
        assert abs(result['effectiveSynthHz']-expected)<120
        assert result['rawDmaWords']==0,'DMA observed an intermediate Q15 sample'
        if ratio<1:assert result['tone440EnergyFraction']>.99
    elif ratio<1:
        assert abs(result['effectiveSynthHz']-44100)<100
        assert result['tone440EnergyFraction']>.99
    else:
        assert abs(result['effectiveSynthHz']-22050)<120
        assert result['tone440EnergyFraction']<.01
    with wave.open(str(raw.with_suffix('.wav')),'wb') as wav:
        wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(44100);wav.writeframes(raw.read_bytes())
    results.append(result)
report={'kind':('pipelined' if pipeline else 'vendor')+'-production-ISR-with-controlled-render-duration','hardwareMeasured':False,
 'sourceSha256':hashlib.sha256((ROOT/'firmware/src/audio_output.h').read_bytes() if pipeline else body.encode()).hexdigest(),
 'assumptions':'sample-clocked fake DMA; injected constant render cost; no USB or bus contention; publication cost not simulated',
 'results':results}
(OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
