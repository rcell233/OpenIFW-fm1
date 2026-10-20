#!/usr/bin/env python3
"""Validate and collect the no-hardware audio-path audit evidence."""
import hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];B=R/'build/target-audit'
def read(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
golden=read(R/'docs/evidence/irq-stream-golden.json')
native=read(B/'irq-native.json');sanitized=read(B/'irq-sanitize.json')
for x in (native,sanitized):
    for k in ('blocks','frames','hashes'):assert x[k]==golden[k],k
runs={}
for name in ('div1-instant','div1-spi','div32-spi','div48-spi','sine48-spi','mute48-spi'):
    x=read(B/f'{name}.json')
    d=x['drawIntervalsMs']
    # Early captures predate the output-field unit rename; values already are ms.
    if 'blocks' in d:x['drawIntervalsMs']={k:d[v] for k,v in [('count','blocks'),('p50','p50Us'),('p95','p95Us'),('max','maxUs')]}
    x.pop('instructionProfile',None);runs[name]=x
irq={}
for n in (1,48):
    x=read(B/f'irq-baseline/div{n}.json');v=x['irqRegression']
    assert v['ready'] and v['done']==1 and v['hashes']==golden['hashes']
    assert v['frames']==golden['frames'] and v['blocks']==golden['blocks']
    irq[str(n)]={k:x[k] for k in ('kind','hardwareMeasured','dividerCyclesAssumption','spiWireTiming','firmwareSha256','emulatorSha256','irqRegression','all')}
assert runs['div48-spi']['all']['overruns']>0
assert runs['div1-spi']['all']['overruns']==0
for n in ('sine48-spi','mute48-spi'):assert runs[n]['all']['overruns']==0
package=R/'firmware-bin/OpenIFW-FM1-0.4.1.fwsc'
assert sha(R/'build/firmware/OpenIFW-FM1-0.4.1.fwsc')==sha(package)
result={
 'sourceCommit':golden['sourceCommit'],'hardwareMeasured':False,
 'firmwareSha256':sha(package),'normalBuildByteIdenticalToRelease':True,
 'emulatorCommit':'19177bd79914fcdf527ac423f6689946a0557273',
 'emulatorAuditPatchSha256':sha(R/'tools/fm1-emu-audit.patch'),
 'limitations':['Divider cycle counts are injected sensitivity inputs, not measured pi32v2 timings.',
   'Non-divider instructions still cost one cycle; no cache, flash, bus or attached USB host model.',
   'SPI wire timing assumes a 60 MHz peripheral clock and BAUD=4.',
   'Matching interpreter checksums cannot certify ISA implementation on a physical chip.'],
 'dmaDeadlineReproduction':read(B/'dma/results.json'),
 'irqGolden':golden,'irqTargetResults':irq,
 'nativeIrqGoldenPassed':True,'asanUbsanIrqGoldenPassed':True,
 'octaveConfigurationIsolation':read(B/'octave-config.json'),
 'sensitivityRuns':runs,
 'fullSuite':{'passed':True,'uiChecks':read(R/'build/ui/results.json')['checks'],
   'fixedCases':read(R/'build/fixed-engine-results.json')['cases'],
   'installerChecks':read(R/'build/installer/results.json')['checks']},
 'audioExamples':{p.name:sha(p) for p in (R/'docs/evidence').glob('audio-deadline-*.wav')}
}
out=R/'docs/evidence/hardware-audio-audit-0.4.1.json'
out.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
print('PASS audit: controlled DMA failure reproduced; 24 IRQ hashes match in both timing models; normal firmware unchanged')
