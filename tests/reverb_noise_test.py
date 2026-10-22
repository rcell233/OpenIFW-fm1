#!/usr/bin/env python3
"""Require a measured precision improvement without calling it hardware SNR."""
import json
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'build/reverb-noise'
read=lambda n:json.loads((out/(n+'.json')).read_text())
old,new,sanitized=read('q15'),read('q23'),read('sanitize')
assert old['bits']==15 and new['bits']==23
assert new['arithmeticChecks']==old['arithmeticChecks']==196608
assert new==sanitized,'Native and ASan/UBSan precision results differ'
rows=[]
for a,b in zip(old['cases'],new['cases']):
    assert a['preReverbLevel']==b['preReverbLevel']
    assert a['referenceWetRms']==b['referenceWetRms']
    assert not a['zeroInputNonzeroFrames'] and not b['zeroInputNonzeroFrames']
    improvement=a['errorRelativeDb']-b['errorRelativeDb']
    assert improvement>40,'Less than 40 dB reduction in tank residual'
    assert b['errorRms']<2e-6 and 8<b['lastNonzeroSeconds']<20
    rows.append(dict(level=a['preReverbLevel'],previousErrorRelativeDb=a['errorRelativeDb'],
                     newErrorRelativeDb=b['errorRelativeDb'],residualReductionDb=improvement))
assert len(rows)==3
(out/'result.json').write_text(json.dumps(dict(kind='isolated-tank-residual-not-hardware-SNR',rows=rows,
    arithmeticChecks=new['arithmeticChecks'],silenceExact=True,sanitizeExact=True),indent=2)+'\n')
print('PASS high-precision reverb: >40 dB residual reduction at all three levels; exact zero input/output and decaying tails; 196,608 arithmetic cases; identical ASan/UBSan results')
