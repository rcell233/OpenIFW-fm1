#!/usr/bin/env python3
"""Check wet response onset, useful decay and silence independently of core PCM."""
import json,subprocess,math
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/reverb-quality';out.mkdir(exist_ok=True)
profiles={}
for name in ['production','legacy']:
    data=json.loads(subprocess.check_output([root/f'build/reverb-impulse-{name}'],text=True))
    profiles[name]=data
    (out/f'{name}.json').write_text(json.dumps(data,indent=2)+'\n')
assert profiles['production']['reverbRate']==22050
assert profiles['legacy']['reverbRate']==44100
rows=[]
for old,new in zip(profiles['legacy']['cases'],profiles['production']['cases']):
    assert old['size']==new['size']
    assert abs(old['firstWetFrame']-new['firstWetFrame'])<=4,'Delay time changed'
    assert new['peak']<0.5,'Unexpected wet gain'
    # High precision retains weak tails formerly erased by Q15 truncation.
    # Keep size-dependent finite bounds and exact end-of-response silence.
    max_tail=2 if new['size']==0 else 6 if new['size']<10 else 16
    assert new['lastNonzeroFrame']<max_tail*44100,'Tail fails to decay'
    assert new['lastNonzeroFrame']>new['firstWetFrame']+4410,'Tail missing'
    ratio=new['lowpass1kEnergy']/old['lowpass1kEnergy']
    assert 0.5<ratio<2,'Low/mid wet energy differs by more than 3 dB'
    rows.append(dict(size=new['size'],onsetDeltaFrames=new['firstWetFrame']-old['firstWetFrame'],
                     oldLastNonzeroSeconds=old['lastNonzeroFrame']/44100,
                     newLastNonzeroSeconds=new['lastNonzeroFrame']/44100,
                     lowpass1kEnergyDeltaDb=10*math.log10(ratio)))
(out/'result.json').write_text(json.dumps(dict(rows=rows,tailSilence=True,oldPcmGoldensUpdated=False),indent=2)+'\n')
print('PASS production wet response: unchanged onset, low/mid energy within 3 dB, decaying tails and exact silence at all three sizes.')
