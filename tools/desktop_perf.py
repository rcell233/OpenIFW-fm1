#!/usr/bin/env python3
"""Interleaved relative desktop benchmark. Never reports device CPU usage."""
import hashlib,json,platform,statistics,subprocess,sys
from pathlib import Path
old,new,out=sys.argv[1:4];rounds=int(sys.argv[4]) if len(sys.argv)>4 else 5
samples=[[],[]]
for r in range(rounds):
    for i in ([0,1] if r%2==0 else [1,0]):
        samples[i].append(json.loads(subprocess.check_output([old,new][i],text=True)))
    print(f'Completed desktop A/B round {r+1}/{rounds}',flush=True)
rows=[]
for c in range(14):
    a=samples[0][0]['cases'][c];b=samples[1][0]['cases'][c]
    for group in samples:
        for trial in group:
            case=trial['cases'][c]
            if [case[k] for k in ('name','dry','note','checksum')]!=[a[k] for k in ('name','dry','note','checksum')]:
                raise SystemExit('Benchmark input/PCM mismatch: '+a['name'])
    med=lambda side,key:statistics.median(v['cases'][c][key] for v in samples[side])
    before,after=med(0,'p50Us'),med(1,'p50Us')
    rows.append(dict(name=a['name'],dry=a['dry'],note=a['note'],oldP50Us=before,newP50Us=after,
                     reductionPercent=100*(1-after/before),oldP95Us=med(0,'p95Us'),newP95Us=med(1,'p95Us'),checksum=a['checksum']))
result=dict(kind='desktop-relative-only',hardwareMeasured=False,rounds=rounds,
            clock='CLOCK_THREAD_CPUTIME_ID, DSP only',architecture=platform.machine(),
            binarySha256={label:hashlib.sha256(Path(p).read_bytes()).hexdigest() for label,p in [('baseline',old),('optimized',new)]},cases=rows)
Path(out).write_text(json.dumps(result,indent=2)+'\n')
