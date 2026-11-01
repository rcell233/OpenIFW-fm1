#!/usr/bin/env python3
"""Validate and collect rc9 output-bus evidence without modifying old fixtures."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1];out=root/'build/output-bus'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
native=read(out/'native.json');san=read(out/'sanitize.json');wasm=read(out/'wasm.json')
assert native['arithmeticChecks']==san['arithmeticChecks']==1104971
assert len(native['scenes'])==294
assert all(s['gainDb']>=0 and s['loudDacPeak']<=.46875 and not s['rawClips'] for s in native['scenes'])
assert wasm['normalSamples']==wasm['loudSamples']==3686400 and wasm['stateChecks']==216000
stream=read(out/'stream.json');target=read(out/'stream-target.json')
assert all(stream[k]==target['outputBusRegression'][k] for k in ['blocks','pcm','state'])
assert stream['frames']==196608 and stream['blocks']==768
assert stream['engineHashes']==target['irqRegression']['hashes']
assert target['irqRegression']['done']==1
assert stream['engineHashes']==read(root/'build/target-audit/irq-native.json')['hashes']
build=read(root/'firmware-bin/build.json');filename=f"OpenIFW-FM1-{build['version']}.fwsc"
assert build['version']=='0.4.7-rc9'
assert build['sha256']==sha(root/'build/firmware'/filename)==sha(root/'firmware-bin'/filename)
boot=read(out/'boot.json');assert boot['firmwareSha256']==build['sha256']
assert boot['normal']['guardPending']==0 and boot['normal']['ready']==1 and boot['normal']['rescue']==0
assert boot['normalDurationMs']==32000 and boot['rescue']['rescue']==1
old=read(root/'firmware-bin/OpenIFW-FM1-0.4.7-rc8.json')
runs={};timing=[]
for p in range(6):
    a=read(out/f'rc8-p{p}.json');b=read(out/f'rc9-p{p}.json')
    for v,r,meta in [('rc8',a,old),('rc9',b,build)]:
        assert r['actualPreset']==p and r['firmwareSha256']==meta['sha256']
        assert not r['underruns'] and not r['all']['overruns'] and not r['all']['dmaLate']
        runs[f'{v}-p{p}']=r
    before=a['dspSteady']['p50Us'];after=b['dspSteady']['p50Us']
    timing.append(dict(preset=p,rc8P50Us=before,rc9P50Us=after,extraUs=after-before,
                       extraPercent=100*(after/before-1),rc9P99Us=b['dspSteady']['p99Us']))
for p in [0,3]:
    for v,meta in [('rc8',old),('rc9',build)]:
        r=read(out/f'{v}-dense{p}.json')
        assert r['firmwareSha256']==meta['sha256'] and r['actualPreset']==p
        assert not r['underruns'] and not r['all']['overruns'] and not r['all']['dmaLate']
        runs[f'{v}-dense{p}']=r
frozen=read(root/'docs/evidence/dsp-kernels-0.4.7-rc8.json')['sourceSha256']
preserved={p:h for p,h in frozen.items() if p!='firmware/src/audio_output.h'}
assert all(sha(root/p)==h for p,h in preserved.items())
sources=['firmware/src/output_bus.c','firmware/src/output_bus.h','firmware/src/output_bus_table.h',
         'firmware/src/app.c','firmware/src/audio_output.h','firmware/src/platform.c',
         'firmware/src/ui/state.h','firmware/src/ui/controls.c','web/emu/openifw_web.c']
full=(out/'full-test.log').read_text();ref=(out/'reference-sanitize.log').read_text()
assert 'PASS output bus: 1,104,971 arithmetic checks' in full
assert 'PASS destination-aliases' in ref
assert not any(token in full for token in ['AssertionError','runtime error:','ERROR: AddressSanitizer','make: ***'])
report=dict(version='0.4.7-rc9',date='2026-11-01',hardwareMeasured=False,
    purpose='optional post-FX loudness processing, not an underrun repair',
    referenceCommit='911d90091cb36c8dba2628f5aca8bb2022609ead',build=build,
    outputModeDefault='LOUD',normalModeExactRc8=True,
    preservedSourceSha256=preserved,sourceSha256={p:sha(root/p) for p in sources},
    native=native,sanitizer=san,wasm=wasm,toneQuality=read(out/'tone-quality.json'),
    targetStream=dict(native=stream,target=target),boot=boot,
    modelTimingSummary=timing,modelRuns=runs,
    fullSuitePassed=True,referenceSanitizerPassed=True,independentRebuildIdentical=True,
    installer=read(root/'build/installer/results.json'),
    limitations=['Limiter and peak guard intentionally change strong-signal dynamics and harmonics.',
                 'No lookahead, oversampling or analog/inter-sample true-peak guarantee.',
                 'Model timings are uncalibrated instruction comparisons, not device CPU percentages.',
                 'Existing core/FX clipping cannot be repaired by a downstream output stage.',
                 'Physical noise, loudness, distortion, USB-host load and underruns require device testing.'])
dest=root/'docs/evidence/output-bus-0.4.7-rc9.json'
dest.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
print('PASS rc9 evidence:',dest.relative_to(root))
