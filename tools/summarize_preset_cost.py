#!/usr/bin/env python3
"""Archive valid preset ablations; never interpret model times as device CPU."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'build/preset-cost'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
cases = []
for path in sorted(RUNS.glob('p*.json')):
    if not (path.name.endswith('-A4.json') or path.name.endswith('-108.json')):
        continue
    data = json.loads(path.read_text())
    assert data['actualPreset'] == data['preset']
    assert data['observation']['peak'] > 0, path
    assert data['firmwareSha256'] == '4f27b9f8fbadc935b9f376fdf506794d79d2f7f31d5c4e319d443a502efa9186'
    if 'currentNote' in data:
        assert data['currentNote'] == (69 if data['note'] == 'A4' else 108)
        assert data['monoCount'] == 1
    cases.append(data)

headroom = json.loads((RUNS / 'headroom.json').read_text())
assert headroom == json.loads((RUNS / 'headroom-sanitize.json').read_text())
assert len(headroom['cases']) == 72
assert not any(c['clippedChannelSamples'] for c in headroom['cases'])
baselines = {d['preset']: d for d in cases if d['note'] == 'A4' and d['probe'] == 'baseline'}
assert set(baselines) == {0, 3, 5}
ablations = []
for d in cases:
    if d['note'] != 'A4' or d['probe'] == 'baseline':
        continue
    b = baselines[d['preset']]['dsp']['meanUs']
    us = d['dsp']['meanUs']
    ablations.append(dict(preset=d['preset'],probe=d['probe'],baselineMeanUs=b,
                          ablatedMeanUs=us,savedUs=b-us,savedPercent=100*(1-us/b)))

sources = ['tools/dsp_cost_probe.mjs','tests/preset_headroom.c',
           'firmware/src/fixed/synth.c','firmware/src/fixed/effects.c',
           'firmware/src/fixed/math.c','firmware/src/fixed/envelope.c',
           'firmware/src/control/mono.c','firmware/src/presets.h']
result = dict(kind='preset-cost-analysis',date='2026-10-22',
              hardwareMeasured=False,cycleAccurate=False,
              hardwareFeedback=dict(firmware='0.4.7-rc2',boots=True,
                                    judd='User reports normal without crackles',
                                    daisyAndTresure='User reports remaining crackles'),
              limits=['Instruction model has no real Flash/cache/bus stalls or USB host.',
                      'The valid modeled factory cases did not reproduce output underruns.',
                      'Ablation alters downstream signals; marginal costs are approximate and not additive.',
                      'Final output headroom does not rule out internal nonlinearities or analog distortion.',
                      'No simplified FX implementation or new firmware is claimed by this report.'],
              sourceSha256={p:sha(ROOT/p) for p in sources},
              ablations=ablations,measurements=cases,headroom=headroom,
              validation=dict(headroomAsanUbsan=True,headroomNormalSanitizedIdentical=True,
                              rejectedRunsExcluded='Unsupported panel C8 and absent UART stimulus yielded silence; not measurements.'))
out = ROOT / 'docs/evidence/preset-cost-0.4.7-rc2.json'
out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(f'Archived {len(cases)} valid target-model cases and 72 native headroom cases: {out.relative_to(ROOT)}')
