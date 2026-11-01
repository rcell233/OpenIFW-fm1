#!/usr/bin/env python3
"""Render rc8 and pinned current FM-1 peers for an output-volume review.

This tool does not modify DSP, factory patches, or published firmware. Source
checkouts go beside OpenIFW-FM1; generated files and renders go under build/.
Requires cc, Python 3, and the peer generators' dependencies. The Felucca
checkout needs its two pinned submodules (`git submodule update --init`).
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PEERS = {
    'Felucca': ('hugelton/Felucca', '406b328ee43ffb8fd3d2eb4cce1a741db4a4cea1'),
    'Ghoulbox': ('jasonpersinger/ghoulbox-fm1-dungeon-synth', '9f2b9a22e456d7fe9552b40f7a1d0c13200913bb'),
    'Melodee': ('keremimo/melodee', 'c14912ba911d4f4fefaa07120eb068a91d7ccb78'),
}
# Different factory sounds are level examples, not an equal-timbre ranking.
# Ghoulbox's retired SAW LEAD / SINE KEY are deliberately excluded.
SELECTION = {
    'Felucca': [(0, 'SAW LEAD'), (0, 'ACID'), (0, 'SINE KEY'), (0, 'SOFT PAD'),
                (12, 'PAD'), (12, 'TINE EP'), (12, 'BRASS')],
    'Ghoulbox': [(0, 'SOFT PAD'), (12, 'DARK STRINGS'), (12, 'PAD'),
                 (14, 'HURDY GURDY'), (14, 'DRONE WHEEL')],
    'Melodee': [(12, 'TINE EP'), (12, 'SOLID BASS'), (12, 'PAD'), (12, 'STRINGS'),
                (19, "It's a Prophet 5"), (19, 'Stabby Brass'),
                (15, 'BRASS 1'), (15, 'SYNTH.LEAD 1'), (3, 'BREADBOX')],
}

PROBE = r'''
#define main upstream_host_main
#include "PEER_HOST"
#undef main
static const preset_t *selected_preset(unsigned e,unsigned p) {
    PRESET_GET
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"list")) {
        for(unsigned e=0;e<NENGINES;e++)for(unsigned p=0;p<ENGINES[e]->npresets;p++)
            printf("%u\t%u\t%s\t%s\n",e,p,ENGINES[e]->name,selected_preset(e,p)->name);
        return 0;
    }
    if(argc!=5)return 2;
    unsigned e=atoi(argv[1]),p=atoi(argv[2]),note=atoi(argv[3]),vel=atoi(argv[4]);
    if(e>=NENGINES || p>=ENGINES[e]->npresets || note>127 || vel>127)return 3;
    host_tracks_init();
    /* host_tracks_init freezes old ROOM goldens; restore real current global
     * defaults (including HALL) before loading the actual factory preset. */
    for(unsigned i=0;i<G_COUNT;i++)song.g[i]=GP[i].def;
    host_preset(&trk[0],e,p);
    trk_note_on(&trk[0],note,vel);
    double energy=0,steady=0,mono=0,dc=0;
    unsigned peak=0,clips=0,blocks=0,limited=0,max_env=0;
    int32_t out[2*CTL];
    for(unsigned frame=0;frame<4*FS;frame+=CTL) {
        mix_block(out,CTL);blocks++;
        if(lim_env>LIM_T)limited++;
        if((unsigned)lim_env>max_env)max_env=lim_env;
        for(unsigned i=0;i<CTL && frame+i<4*FS;i++) {
            double m=(out[2*i]+out[2*i+1])/65536.;mono+=m*m;
            for(unsigned ch=0;ch<2;ch++) {
                int32_t v=out[2*i+ch];unsigned a=v<0?-(int64_t)v:v;
                if(a>peak)peak=a;if(a>=32767)clips++;
                double x=v/32768.;energy+=x*x;dc+=x;
                if(frame+i>=2*FS)steady+=x*x;
            }
        }
    }
    printf("{\"engine\":%u,\"preset\":%u,\"engineName\":\"%s\",\"name\":\"%s\","
           "\"note\":%u,\"velocity\":%u,\"peak\":%.9f,\"rms\":%.9f,\"steadyRms\":%.9f,"
           "\"monoMeanRms\":%.9f,\"mean\":%.9f,\"boundaryChannelSamples\":%u,"
           "\"blocks\":%u,\"limiterActiveBlockEnds\":%u,\"limiterMaxEnvelope\":%u}\n",
           e,p,ENGINES[e]->name,selected_preset(e,p)->name,note,vel,
           peak/32768.,sqrt(energy/(8*FS)),sqrt(steady/(4*FS)),sqrt(mono/(4*FS)),
           dc/(8*FS),clips,blocks,limited,max_env);
    return peak?0:4;
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def db(x):
    return 20*math.log10(x) if x else None


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--references',type=Path,default=ROOT.parent)
    ap.add_argument('--out',type=Path,default=ROOT/'build/volume-review')
    args=ap.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    # Reuse the separately run production suite, only with matching sources.
    validation_path=ROOT/'build/output-level/result.json'
    if not validation_path.exists():raise SystemExit('Run make output-level-test first.')
    validation=json.loads(validation_path.read_text())
    for mapping in ['sourceSha256','validatedSourceSha256']:
        for name,digest in validation[mapping].items():assert sha(ROOT/name)==digest,name
    assert validation['factoryCases']==288 and validation['outputBoundaryChannelSamples']==0
    assert validation['longScenes']['arithmeticChecks']==524288
    log=(out/'build.log').open('w')
    compiler=os.environ.get('CC','cc')
    def run(cmd):
        subprocess.run([str(s) for s in cmd],cwd=ROOT,check=True,stdout=log,stderr=log)
    def capture(cmd):
        return subprocess.check_output([str(s) for s in cmd],cwd=ROOT,stderr=log,text=True)
    native=out/'factory'
    run([compiler,'-O3','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
         '-iquote'+str(ROOT/'firmware/src/fixed'),'-iquote'+str(ROOT/'firmware/src'),
         ROOT/'tools/output_level_probe.c',ROOT/'firmware/src/dsp.c','-lm','-o',native])
    cases=json.loads(capture([native]));assert len(cases)==288
    assert all(r['outputGain']==2 and not r['outputBoundaryChannelSamples'] for r in cases)
    previous=json.loads((ROOT/'build/output-level/factory.json').read_text())
    assert len(previous)==len(cases)
    for current,old in zip(cases,previous):
        assert all(current[k]==v for k,v in old.items()),'Previous production measurement changed'
        assert (current['gain2ClippedChannelSamples']<=current['gainSqrt8ClippedChannelSamples']
                <=current['gain3ClippedChannelSamples']<=current['gain4ClippedChannelSamples'])
    (out/'factory-rc8.json').write_text(json.dumps(cases,indent=2)+'\n')
    peers=[]
    for name,(repo,commit) in PEERS.items():
        ref=args.references.resolve()/name
        assert capture(['git','-C',ref,'rev-parse','HEAD']).strip()==commit,name
        assert not capture(['git','-C',ref,'diff','HEAD','--']).strip(),name+' modified sources'
        dest=out/name;dest.mkdir(exist_ok=True)
        gen=dest/'gen';gen.mkdir(exist_ok=True)
        prefix='melodee' if name=='Melodee' else 'felucca'
        generators=[('gen_tables.py',prefix+'_tables.h'),('gen_fm6_patches.py',prefix+'_fm6.h')]
        if name=='Melodee':
            generators += [('gen_cz1_factory.py','melodee_cz1.h'),
                           ('gen_prophet_factory.py','melodee_prophet_factory.h'),
                           ('gen_909_tables.py','x0x_drum_tables.h')]
        else:generators += [('gen_samples.py','felucca_samples.h')]
        for tool,header in generators:
            extra=['--web','check'] if tool=='gen_prophet_factory.py' else []
            run([sys.executable,ref/'tools'/tool,gen/header,*extra])
        getter='return eng_preset(ENGINES[e],p);' if name=='Melodee' else 'return &ENGINES[e]->presets[p];'
        body=PROBE.replace('PEER_HOST',str(ref/'tests/hostsim.c')).replace('PRESET_GET',getter)
        probe=dest/'probe.c';probe.write_text(body)
        binary=dest/'probe'
        run([compiler,'-O2','-std=gnu11','-ffp-contract=off','-w','-DFELUCCA_CPU2=0',
             '-I'+str(gen),'-I'+str(ref/'firmware/src'),'-I'+str(ref/'firmware/hal'),
             '-I'+str(ref/'firmware/bsp/chip'),'-I'+str(ref/'firmware/bsp/boards/fm1'),
             probe,'-lm','-o',binary])
        catalog=[line.split('\t') for line in capture([binary,'list']).splitlines()]
        (dest/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
        rows=[]
        for e,preset in SELECTION[name]:
            found=[r for r in catalog if int(r[0])==e and r[3]==preset]
            if len(found)!=1:raise SystemExit(f'{name}: preset {e}/{preset} not found; see {dest}/catalog.json')
            p=int(found[0][1])
            for note in [48,69]:
                for vel in [110,127]:
                    rows.append(json.loads(capture([binary,e,p,note,vel])))
        audio=ref/'firmware/src/audio.c'
        assert '#define OUT_SHIFT 7 ' in audio.read_text(),name+' DAC scale changed'
        sources=['firmware/src/audio.c','firmware/src/fx.c','firmware/src/params.c',
                 'firmware/src/engines.c','tests/hostsim.c']
        if name=='Melodee':sources+=['firmware/src/mix.c']
        hal='firmware/bsp/boards/fm1/fm1_audio.h' if name=='Felucca' else 'firmware/hal/fm1_audio.h'
        sources += [hal]
        peers.append(dict(name=name,repository=repo,commit=commit,cases=rows,
            i2sScale=128,sourceSha256={s:sha(ref/s) for s in sources},
            submodules=capture(['git','-C',ref,'submodule','status']).splitlines(),
            harnessSha256=sha(probe)))
        print(name,len(rows),'renders complete',flush=True)
    for group in [cases,*[p['cases'] for p in peers]]:
        for r in group:
            peak=r.get('outputPeak',r['peak']);rms=r.get('outputRms',r['rms'])
            r['dacPeakDbFS']=db(peak/2);r['dacRmsDbFS']=db(rms/2)
            r['usbPeakDbFS']=db(peak);r['usbRmsDbFS']=db(rms)
    report=dict(schema=1,kind='native-output-volume-review',date='2026-11-01',
        sourceCommit=capture(['git','rev-parse','HEAD']).strip(),hardwareMeasured=False,
        firmwareVersion='0.4.7-rc8',
        firmwareSha256=sha(ROOT/'firmware-bin/OpenIFW-FM1-0.4.7-rc8.fwsc'),
        sampleRate=44100,seconds=4,steadyWindowSeconds=[2,4],masterQ12=4096,
        factoryCases=len(cases),peerCases=sum(len(p['cases']) for p in peers),
        scope=['Single held note; native original C, factory patch and current global FX defaults.',
               'Different sounds, envelopes and spectral balance; RMS is not LUFS or sound pressure.',
               'The nominal ADC=1023 square-law map gives 4088/4096 (-0.017 dB); smoothing quantization can be slightly lower. Actual ADC endpoints are not measured.',
               'Peer master limiter and softclip remain active; track level is its factory default.',
               'USB follows MASTER here; FIXED mode, host input gain and analog output are not measured.',
               'Ghoulbox selected sounds exclude presets retired from its current UI.'],
        sourceSha256={s:sha(ROOT/s) for s in ['tools/review_output_volume.py',
            'tools/output_level_probe.c','firmware/src/output_level.h','firmware/src/audio_output.h',
            'firmware/src/platform.c','web/emu/openifw_web.c','firmware/src/presets.h',
            'firmware/src/fixed/synth.c','firmware/src/fixed/effects.c']},
        openifw=cases,peers=peers,outputValidation=validation,
        projections=dict(currentMaxQ15Peak=max(r['outputPeak'] for r in cases),
            extra3DbBeforeQ15Clamp=dict(gainDb=db(math.sqrt(2)),
                affectedCases=sum(r['gainSqrt8ClippedChannelSamples']>0 for r in cases),
                exceededChannelSamples=sum(r['gainSqrt8ClippedChannelSamples'] for r in cases)),
            extra3p5DbBeforeQ15Clamp=dict(gainDb=db(1.5),
                affectedCases=sum(r['gain3ClippedChannelSamples']>0 for r in cases),
                exceededChannelSamples=sum(r['gain3ClippedChannelSamples'] for r in cases)),
            extra6DbBeforeQ15Clamp=dict(affectedCases=sum(r['gain4ClippedChannelSamples']>0 for r in cases),
                exceededChannelSamples=sum(r['gain4ClippedChannelSamples'] for r in cases)),
            i2sScale181=dict(relativeGain=181/128,gainDb=db(181/128),
                maxDacPeak=max(r['outputPeak'] for r in cases)*181/256),
            i2sScale192=dict(relativeGain=1.5,gainDb=db(1.5),
                maxDacPeak=max(r['outputPeak'] for r in cases)*.75),
            i2sScale256=dict(relativeGain=2,gainDb=db(2),
                maxDacPeak=max(r['outputPeak'] for r in cases))))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'report':str(out/'report.json'),'cases':report['factoryCases'],
                      'peerCases':report['peerCases'],'projections':report['projections']},indent=2))


if __name__=='__main__':main()
