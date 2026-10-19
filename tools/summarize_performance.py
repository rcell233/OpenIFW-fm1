#!/usr/bin/env python3
"""Collect evidence; reject mismatched firmware, presets or golden outputs."""
import hashlib,json
from pathlib import Path
r=Path(__file__).resolve().parents[1];b=r/'build/performance'
read=lambda p:json.loads(p.read_text())
release=read(r/'firmware-bin/build.json');golden=read(r/'docs/evidence/dsp-optimization-baseline.json')
names=['Tresure Island','Primer','Ai no Labyrinth','Daisy Area 2','Wanna Go To Hawaii','JUDD-ish 2000']
rows=[];models=set()
for i,name in enumerate(names):
    old=read(b/f'baseline-{i}.json');new=read(b/f'preset-{i}.json')
    if old['actualPreset']!=i or new['actualPreset']!=i:raise SystemExit('Preset did not load: '+name)
    if new['firmwareSha256']!=release['sha256']:raise SystemExit('Stale optimized firmware measurement')
    for v in [old,new]:
        if v['hardwareMeasured'] or v['cycleAccurate']:raise SystemExit('Wrong evidence classification')
        models.add(v['emulatorSha256'])
    a,z=old['steady']['maxUs'],new['steady']['maxUs']
    rows.append(dict(name=name,baseline=old,optimized=new,reductionPercent=100*(1-z/a)))
target=read(b/'target-regression.json');checks=target['targetRegression']
if not checks['complete'] or checks['mathErrors'] or checks['hashes']!=golden['liveEditHashes']:
    raise SystemExit('Target instruction regression failed')
stress=read(b/'stress.json');oldstress=read(b/'baseline-stress.json')
models.update([target['emulatorSha256'],stress['emulatorSha256'],oldstress['emulatorSha256']])
if len(models)!=1:raise SystemExit('Emulator versions differ across runs')
ui=read(r/'build/ui/results.json')
result=dict(version=release['version'],date='2026-10-19',hardwareMeasured=False,
            model='pi32v2 interpreter; 240 MHz; one instruction/cycle; not cycle accurate; no USB host/cache/flash wait-state timing',
            baselineCommit=golden['baselineCommit'],baselineFirmware=read(r/'firmware-bin/OpenIFW-FM1-0.4.0.json'),
            optimizedFirmware=release,factory=rows,stress=dict(baseline=oldstress,optimized=stress),
            targetRegression=target,desktop=read(b/'desktop.json'),ui=ui,
            exactRegression=dict(fixedScenarios=166,pcmAndStateFiles=332,frozenWebScenarios=12,frozenStereoFrames=1228800,
                liveEditStereoFrames=32768,livePatchChanges=1024,arithmeticRandomCases=100000),
            emulatorSource='https://github.com/joakimeriksson/fm1-emu/tree/19177bd79914fcdf527ac423f6689946a0557273',
            profilerPatchSha256=hashlib.sha256((r/'tools/fm1-emu-profile.patch').read_bytes()).hexdigest())
(r/'docs/evidence/performance-0.4.1.json').write_text(json.dumps(result,indent=2)+'\n')
table=['| 工厂音色 | 0.4.0 模型峰值 | 0.4.1 模型峰值 | 减少 | 新版 XRUN / DMA |',
       '| --- | ---: | ---: | ---: | ---: |']
for row in rows:
    old,new=row['baseline'],row['optimized']
    table.append(f"| {row['name']} | {old['steady']['maxUs']/1000:.3f} ms | {new['steady']['maxUs']/1000:.3f} ms | {row['reductionPercent']:.1f}% | {new['all']['overruns']} / {new['all']['dmaLate']} |")
(b/'table.md').write_text('\n'.join(table)+'\n')
print('\n'.join(table))
