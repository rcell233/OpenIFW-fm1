#!/usr/bin/env python3
"""Bind the tested rc3 normal package, model runs and preserved core regressions."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];run=root/'build/fx-047rc3'
read=lambda p:json.loads(p.read_text())
meta=read(root/'firmware-bin/build.json')
assert meta['version']=='0.4.7-rc3' and meta['reverbSampleRate']==22050
package=root/'firmware-bin'/f"OpenIFW-FM1-{meta['version']}.fwsc"
assert hashlib.sha256(package.read_bytes()).hexdigest()==meta['sha256']
assert package.read_bytes()==(root/'build/firmware'/package.name).read_bytes()
model=[read(p) for p in sorted(run.glob('p*-*.json'))]
assert len(model)==8
assert all(d['firmwareSha256']==meta['sha256'] and d['monoCount']==1 and d['observation']['peak']>0 and d['observation']['underruns']==0 for d in model)
old=read(root/'docs/evidence/preset-cost-0.4.7-rc2.json')['measurements']
compare=[]
for i in [0,3,5]:
    a=next(d for d in old if d['preset']==i and d['note']=='A4' and d['probe']=='baseline')
    b=next(d for d in model if d['preset']==i and d['note']=='A4')
    before=a['dsp']['meanUs'];after=b['dsp']['meanUs']
    compare.append(dict(preset=i,oldMeanUs=before,newMeanUs=after,reductionPercent=100*(1-after/before)))
boot=read(run/'boot-32s.json');stress=read(run/'stress/result.json');rescue=read(run/'rescue.json')
assert boot['firmwareSha256']==meta['sha256'] and boot['actualPreset']==3 and boot['emulatedMs']>=32000
for d in [boot,stress]:assert d['underruns']==0 and d['all']['overruns']==0 and d['all']['dmaLate']==0
assert rescue['boot']['rescue'] and rescue['boot']['entryReason']==2 and rescue['boot']['stage']==3
irq=read(run/'irq/result.json');native=read(root/'build/target-audit/irq-native.json')
assert irq['irqRegression']['done']==1 and irq['irqRegression']['hashes']==native['hashes']
test=read(run/'selftest/result.json')
assert test['targetRegression']['complete'] and not test['targetRegression']['mathErrors']
assert test['targetRegression']['hashes']==read(root/'docs/evidence/dsp-optimization-baseline.json')['liveEditHashes']
headroom=read(root/'build/reverb-quality/headroom.json')
assert headroom==read(root/'build/reverb-quality/headroom-sanitize.json') and len(headroom['cases'])==72
assert not any(c['clippedChannelSamples'] for c in headroom['cases'])
files=['firmware/src/fixed/synth.c','firmware/src/fixed/effects.c','firmware/src/fixed/synth.h',
       'firmware/src/fixed/reverb_half.h','firmware/src/control/mono.c','tools/build_firmware.py','tools/dsp_cost_probe.mjs']
report=dict(version=meta['version'],date='2026-10-22',hardwareTested=False,cycleAccurate=False,
            scope='Authorized approximate FX; original OSC/FM, EG, matrix, mono/Legato and preset values retained.',
            package=meta,sourceSha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in files},
            comparison=compare,targetModels=model,normal32Seconds=boot,stress10Seconds=stress,
            manualRescue=rescue,targetStream=irq,nativeStream=native,fullRateReferenceTarget=test,
            coreProduction=read(root/'build/reverb-production/result.json'),
            wetResponse=read(root/'build/reverb-quality/result.json'),headroom=headroom,
            installer=read(root/'build/installer/results.json'),web=read(root/'build/emu/results.json'),
            tests=dict(makeTest=True,reverbTest=True,makeSanitize=True,originalGoldensUpdated=False,
                       productionLiveEditsAsanUbsan=True,isolatedNormalRebuildByteIdentical=True),
            limits=['Instruction model is uncalibrated: no actual Flash/cache/bus waits or attached USB host.',
                    'The synthetic maximum stress passes the model with little remaining timing margin.',
                    'Smaller half-rate reverb changes high-frequency content and diffusion; it is intentionally approximate.',
                    'Actual device audio acceptance remains pending.'])
out=root/'docs/evidence/fx-0.4.7-rc3.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('PASS rc3 release evidence: package identity and bytes, eight audible model cases, 32-second playback, stress, rescue, target/native stream, core state and 72 sanitized headroom cases')
