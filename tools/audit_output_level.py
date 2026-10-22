#!/usr/bin/env python3
"""Compare pinned FM-1 sources using native renders, without changing firmware.

Requires the reference clones listed in docs/evidence/fm1-references-2026-10-22.json.
Default: python3 tools/audit_output_level.py
Outputs build/volume-audit/report.json and the unmodified upstream render logs.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

PEER_PROBE = r'''
#define main upstream_host_main
#include "PEER_HOST"
#undef main
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    unsigned engine=(unsigned)atoi(argv[1]),preset=(unsigned)atoi(argv[2]),velocity=(unsigned)atoi(argv[3]);
    if(engine>=NENGINES || preset>=ENGINES[engine]->npresets || velocity>127)return 3;
    host_tracks_init();
    for(unsigned i=0;i<G_COUNT;i++)song.g[i]=GP[i].def;
    host_preset(&trk[0],engine,preset);
    trk_note_on(&trk[0],69,velocity);
    double energy=0,steady_energy=0;
    unsigned peak=0,clips=0;
    int32_t out[2*CTL];
    for(unsigned frame=0;frame<4*FS;frame+=CTL) {
        mix_block(out,CTL);
        for(unsigned i=0;i<CTL && frame+i<4*FS;i++)for(unsigned ch=0;ch<2;ch++) {
            int32_t value=out[2*i+ch];
            unsigned a=(unsigned)(value<0?-(int64_t)value:value);
            if(a>peak)peak=a;
            if(a>=32767)clips++;
            double x=value/32768.;energy+=x*x;
            if(frame+i>=2*FS)steady_energy+=x*x;
        }
    }
    printf("{\"engine\":%u,\"preset\":%u,\"engineName\":\"%s\",\"name\":\"%s\",\"note\":69,\"velocity\":%u,"
           "\"peak\":%.9f,\"rms\":%.9f,\"steadyRms\":%.9f,\"clippedChannelSamples\":%u}\n",
           engine,preset,ENGINES[engine]->name,ENGINES[engine]->presets[preset].name,velocity,
           peak/32768.,sqrt(energy/(8*FS)),sqrt(steady_energy/(4*FS)),clips);
    return peak?0:4;
}
'''

DINGHY_PROBE = r'''
#define DINGHY_HOST 1
#define main upstream_host_main
#include "PEER_HOST"
#undef main
#include <math.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    unsigned velocity=(unsigned)atoi(argv[1]);
    if(velocity>127)return 3;
    play(69,velocity);
    double energy=0,steady_energy=0;
    unsigned peak=0;
    int32_t out[2*32];
    for(unsigned frame=0;frame<4*FS;frame+=32) {
        memset(out,0,sizeof out);app_audio(out,32);
        for(unsigned i=0;i<32 && frame+i<4*FS;i++)for(unsigned ch=0;ch<2;ch++) {
            int32_t value=out[2*i+ch];
            unsigned a=(unsigned)(value<0?-value:value);
            if(a>peak)peak=a;
            double x=value/32768.;energy+=x*x;
            if(frame+i>=2*FS)steady_energy+=x*x;
        }
    }
    printf("{\"name\":\"sine\",\"note\":69,\"velocity\":%u,\"peak\":%.9f,\"rms\":%.9f,\"steadyRms\":%.9f}\n",
           velocity,peak/32768.,sqrt(energy/(8*FS)),sqrt(steady_energy/(4*FS)));
    return peak?0:4;
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def db(value):
    return 20*math.log10(value) if value else None


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--references',type=Path,default=ROOT.parent/'dependencies')
    parser.add_argument('--out',type=Path,default=ROOT/'build/volume-audit')
    args=parser.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    log=(out/'build.log').open('w')
    compiler=os.environ.get('CC','cc')
    def run(command):
        subprocess.run([str(v) for v in command],cwd=ROOT,check=True,stdout=log,stderr=log)
    def capture(command):
        return subprocess.check_output([str(v) for v in command],cwd=ROOT,text=True,stderr=log)
    native=out/'openifw'
    run([compiler,'-O3','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
         '-iquote'+str(ROOT/'firmware/src/fixed'),'-iquote'+str(ROOT/'firmware/src'),
         ROOT/'tools/output_level_probe.c',ROOT/'firmware/src/dsp.c','-lm','-o',native])
    cases=json.loads(capture([native]))
    assert len(cases)==288 and all(c['peak']>0 for c in cases)
    assert not any(c['clippedChannelSamples'] for c in cases)
    (out/'openifw.json').write_text(json.dumps(cases,indent=2)+'\n')
    refs=json.loads((ROOT/'docs/evidence/fm1-references-2026-10-22.json').read_text())
    peers=[]
    for folder in ['dinghy-reference','felucca-reference','ghoulbox-reference']:
        reference=args.references.resolve()/folder
        expected=next(r for r in refs if r['localDirectory']==folder)
        commit=capture(['git','-C',reference,'rev-parse','HEAD']).strip()
        if commit!=expected['commit']:
            raise SystemExit(f'{folder}: expected {expected["commit"]}, got {commit}')
        if capture(['git','-C',reference,'diff','HEAD','--']).strip():
            raise SystemExit(f'{folder}: upstream sources were modified')
        directory=out/folder;directory.mkdir(exist_ok=True)
        if folder=='dinghy-reference':
            source=reference/'firmware/src/dinghy.c'
            body=DINGHY_PROBE.replace('PEER_HOST',str(source))
        else:
            source=reference/'tests/hostsim.c'
            body=PEER_PROBE.replace('PEER_HOST',str(source))
            generated=directory/'gen';generated.mkdir(exist_ok=True)
            for generator,header in [('gen_tables.py','felucca_tables.h'),('gen_fm6_patches.py','felucca_fm6.h'),
                                     ('gen_samples.py','felucca_samples.h')]:
                run(['python3',reference/'tools'/generator,generated/header])
        probe=directory/'probe.c';probe.write_text(body)
        binary=directory/'probe'
        command=[compiler,'-O2','-std=gnu11','-w','-DFELUCCA_CPU2=0',
                 '-I'+str(reference/'firmware/src'),'-I'+str(reference/'firmware/gen')]
        if folder!='dinghy-reference':command+=['-I'+str(generated)]
        command += [probe,'-lm','-o',binary]
        run(command)
        # Different factory sounds: these establish representative electrical
        # levels, not equal-timbre / equal-LUFS loudness rankings.
        selection=[] if folder=='dinghy-reference' else [(0,0),(0,4),(0,5),(12,0),(12,4)]
        if folder=='ghoulbox-reference':selection += [(12,11),(5,0)]
        rendered=[]
        for velocity in [110,127]:
            commands=[[binary,velocity]] if not selection else [[binary,e,p,velocity] for e,p in selection]
            rendered += [json.loads(capture(command)) for command in commands]
        source_files=['firmware/hal/fm1_audio.h','firmware/src/platform.c','firmware/src/dinghy.c'] if folder=='dinghy-reference' else [
            'firmware/hal/fm1_audio.h','firmware/src/audio.c','firmware/src/fx.c']
        peers.append(dict(reference=folder,commit=commit,url=expected['url'],cases=rendered,
                          harnessSha256=digest(probe),sourceSha256={f:digest(reference/f) for f in source_files}))
    # Hardware full-scale is signed 24-bit; all three peers and OpenIFW use
    # Q15 * 128, hence nominal Q15 unity is 0.5 of DAC full scale.
    for group in [cases,*[p['cases'] for p in peers]]:
        for c in group:
            c['q15PeakDbFS']=db(c['peak']);c['q15RmsDbFS']=db(c['rms'])
            c['q15SteadyRmsDbFS']=db(c['steadyRms'])
            c['dacPeakDbFSAtUnityMaster']=db(c.get('outputPeak',c['peak'])/2)
            c['dacRmsDbFSAtUnityMaster']=db(c.get('outputRms',c['rms'])/2)
    report=dict(schema=1,kind='native-arithmetic-output-level-comparison',hardwareMeasured=False,
        measuredSeconds=4,steadyWindowSeconds=[2,4],sampleRate=44100,
        caveats=['No speaker/headphone voltage, SPL, or LUFS measurements.',
                 'Different factory sounds cannot establish a universal perceptual loudness difference.',
                 'ADC endpoints and the analog output stage have not been measured on the device.',
                 'Gain projections apply only to the rendered factory cases, not arbitrary edits.'],
        currentFirmware=json.loads((ROOT/'firmware-bin/build.json').read_text())['version'],
        sourceSha256={str(p.relative_to(ROOT)):digest(p) for p in
            [ROOT/'tools/output_level_probe.c',ROOT/'firmware/src/dsp.c',ROOT/'firmware/src/fixed/synth.c',
             ROOT/'firmware/src/fixed/effects.c',ROOT/'firmware/src/fixed/reverb_half.h',
             ROOT/'firmware/src/audio_output.h',ROOT/'firmware/src/platform.c',ROOT/'firmware/src/presets.h',
             ROOT/'vendor/dinghy/firmware/hal/fm1_audio.h',ROOT/'web/emu/openifw_web.c',ROOT/'firmware/src/output_level.h']},
        masterCurve=[dict(adc=v,q12=(v*v)>>8,gain=((v*v)>>8)/4096,db=db(((v*v)>>8)/4096))
                     for v in [0,256,512,768,1023]],openifw=cases,peers=peers)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(report=str(out/'report.json'),openifwCases=len(cases),peerCases=sum(len(p['cases']) for p in peers),
                          peakDbFS=max(c['q15PeakDbFS'] for c in cases),
                          gain2ClippedCases=sum(bool(c['gain2ClippedChannelSamples']) for c in cases),
                          gain4ClippedCases=sum(bool(c['gain4ClippedChannelSamples']) for c in cases))))


if __name__=='__main__':
    main()
