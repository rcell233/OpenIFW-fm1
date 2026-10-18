#!/usr/bin/env python3
"""Encode the unedited OpenIFW factory files for the board and browser together."""
from pathlib import Path
import hashlib,json
from fixed_patch import encode_patch
R=Path(__file__).resolve().parents[1]
D=R/'vendor/openifw/presets'
files=['tresure-island.json','primer.json','ai-no-labyrinth.json','daisy-area-2.json','wanna-go-to-hawaii.json','judd-ish-2000.json']
patches=[json.loads((D/f).read_text()) for f in files]
names=[p['name'] for p in patches]+['USER 1','USER 2','USER 3']
out=['/* Generated from unmodified OpenIFW programs. Factory parameters: STUDIO-R.',
     ' * Original authorship and notices are retained in vendor/openifw/presets/NOTICE.md. */',
     '#define FACTORY_COUNT 6u', '#define PRESET_COUNT (FACTORY_COUNT+3u)',
     'static const char *const preset_names[]={' + ','.join(json.dumps(n) for n in names) + '};',
     'static const uint8_t preset_programs[]={' + ','.join(p['settings']['ProgramIndex'] for p in patches) + '};',
     'static const int32_t factory_words[FACTORY_COUNT][229]={']
audit=[]
for f,p in zip(files,patches):
    words=encode_patch(p)
    out+=['  { /* '+p['name']+' */']
    out+=['    '+','.join(map(str,words[i:i+10]))+',' for i in range(0,len(words),10)]
    out+=['  },']
    audit.append(dict(file=f,name=p['name'],program=int(p['settings']['ProgramIndex']),sha256=hashlib.sha256((D/f).read_bytes()).hexdigest(),sourceParameters=len(p['parameters']),encodedWords=len(words)))
out+=['};','static void make_preset(IFWFixedPatch *p,uint32_t index) {',
      '    memcpy(p,factory_words[index%FACTORY_COUNT],sizeof *p);','}']
(R/'firmware/src/presets.h').write_text('\n'.join(out)+'\n')
(R/'docs/evidence/factory-parameter-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
print('Generated six original OpenIFW factory programs, no parameter overrides.')
