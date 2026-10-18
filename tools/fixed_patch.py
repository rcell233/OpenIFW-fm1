#!/usr/bin/env python3
"""Typed fixed-point patch encoding shared by tests and the offline importer."""
import json,math,re,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SPEC=json.loads((ROOT/'docs/parameters.json').read_text())
IDS={p['name']:p['id'] for p in SPEC}
DEST={re.sub(r'\s+',' ',x['label'].strip()).upper():x['key'] for x in json.loads((ROOT/'docs/modulation-destinations.json').read_text())}
FX_KEYS=['Bypass','Stereo','Speed','Feedback','Lowpass','WetDry','Rate','Depth','Mix','Time','Highpass','Size','HighDamp','Width']
Q16=65536
def number(v,default=0):
    try: x=float(v if v not in ['',None] else 0)
    except (ValueError,TypeError):return default
    return x if math.isfinite(x) else default
def q16(v):return max(-2147483648,min(2147483647,round(number(v)*65536)))
def source(text):
    t=str(text).upper();flag=128 if '+-' in t or '-+' in t else 0
    if t=='NONE':return 0
    for name,id in [('BREATH',1),('VELOCITY',2),('NOTE',3),('PITCH BEND',4),('VIBRATO',5),('GLIDE',6),('GATE',7)]:
        if name in t or (name=='BREATH' and t=='BR'):return id+flag
    m=re.search(r'EG\s*(\d)',t)
    if m and 1<=int(m[1])<=4:return 8+int(m[1])-1+flag
    m=re.search(r'LFO\s*(\d)',t)
    if m and 1<=int(m[1])<=2:return 12+int(m[1])-1+flag
    m=re.search(r'OSC\s*(\d)',t)
    if m and 1<=int(m[1])<=4:
        i=int(m[1])-1
        return ((14+i) if 'SAW' in t else (22+i) if 'PWM' in t else 26 if 'NOISE' in t else (18+i))+flag
    return 127
def destination(text):
    key=DEST.get(re.sub(r'\s+',' ',str(text).strip()).upper())
    if not key:return -1
    if key in IDS:return IDS[key]
    for i in range(1,5):
        if key==f'Eg{i}Rate':return len(SPEC)+i-1
        if key==f'Osc{i}FM':return len(SPEC)+6+i-1
        if key==f'Osc{i}Pitch':return len(SPEC)+10+i-1
    for i in range(1,3):
        if key==f'Lfo{i}Rate':return len(SPEC)+4+i-1
    return -1
def text_value(key,value):
    t=str(value)
    if key.endswith('Source1') or key.endswith('Source2'):return source(t)
    if key.endswith('Destination'):return destination(t)
    if key.endswith('BreathSubMod'):
        m=re.search(r'EG\s*([1-4])',t,re.I);return int(m[1]) if m else 0
    if key.endswith('Waveform2') or key.endswith('Waveform'):
        return {'SIN':0,'SINE':0,'TRI':1,'SAW':2,'SQU':3,'PWM':3,'PULSE':3,'Ext In':4}.get(t,4)
    if key.endswith('Type'):return {'LPF24':0,'LPF12':1,'HPF12':2,'BPF12':3}.get(t,1)
    if key=='FilterConnection':return {'Parallel 2':0,'Serial':1,'Parallel 1':2}.get(t,0)
    if key=='GlideMode':return int(t=='Rate')
    if key.endswith('Beat'):
        try:a,b=t.split('/');return q16(float(a)/float(b))
        except (ValueError,ZeroDivisionError):return 16384
    return int(value is True or value==1 or t in ['On','1'])
def encode_patch(patch):
    p=patch.get('parameters',{});v=[]
    for s in SPEC:
        x=p.get(s['name'],s['default'])
        v.append(text_value(s['name'],x) if s['kind']=='text' else q16(number(x,s['default'])))
    effects=[]
    for i in range(5):
        defaults=dict(Bypass='On',Stereo=0,Speed=1,Feedback=0,Lowpass=4 if i==0 else 15000,WetDry=2 if i==4 else 0,
                      Rate=.3,Depth=7 if i==1 else 0,Mix=2.7,Time=.2,Highpass=10,Size=6.6,HighDamp=3,Width=6.4)
        fs=patch.get('fx',[]);q=fs[i].get('parameters',{}) if i<len(fs) else {}
        effects.extend(int(q.get(k,'On')!='Off') if k=='Bypass' else q16(q.get(k,defaults[k])) for k in FX_KEYS)
    settings=patch.get('settings',{});mask=0
    for i,k in enumerate(['Phaser','Chorus','Delay1','Delay2','Reverb']):
        x=settings.get('FxBypass'+k,0)
        if x is True or x==1 or x in ['On','1']:mask|=1<<i
    return v+effects+[q16(settings.get('Tuning',440)),q16(patch.get('modelTuning',{}).get('chorusPhase',0)),mask]
def patch_bytes(patch):return struct.pack('<'+'i'*229,*encode_patch(patch))
def preset_file(patch):
    data=patch_bytes(patch)
    return struct.pack('<4sHHII',b'OFM2',2,len(data),zlib.crc32(data),0)+data
if __name__=='__main__':
    values=encode_patch({'parameters':{},'fx':[],'settings':{}})
    dest=ROOT/'firmware/src/fixed/patch_defaults.h'
    dest.write_text('/* Generated typed defaults, no factory preset data. */\nstatic const int32_t ifw_default_values[] = {\n'+
        ',\n'.join('    '+', '.join(str(n) for n in values[i:i+8]) for i in range(0,len(values),8))+'\n};\n')
    print('generated',len(values),'fixed patch words')
