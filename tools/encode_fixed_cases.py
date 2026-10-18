#!/usr/bin/env python3
import json,struct,sys,math
from pathlib import Path
from fixed_patch import patch_bytes,IDS,SPEC,text_value,q16
def q31(x):return max(-2147483647,min(2147483647,round(float(x)*2147483647)))
types=['noteOn','noteOff','breath','bend','glide','vibrato','panic','dry','edit']
def main():
    for c in json.loads(Path(sys.argv[1]).read_text()):
        events=[]
        for e in sorted(c['events'],key=lambda x:x['time']):
            t=types.index(e['type']);a=e.get('note',e.get('value',0));b=e.get('velocity',1)
            if t==8:
                a=IDS[e['key']];b=text_value(e['key'],e['value']) if SPEC[a]['kind']=='text' else q16(e['value'])
            elif t==0:b=q31(b)
            elif 2<=t<=5:a=q31(a);b=0
            else:a=int(a);b=0
            events.append(struct.pack('<IIii',math.floor(e['time']*44100+.5),t,a,b))
        data=struct.pack('<5I',0x32584649,__import__('math').ceil(c['duration']*44100),int(c['dry']),q31(c['breath']),len(events))
        Path(sys.argv[2],c['id']+'.fixed').write_bytes(data+patch_bytes(c['patch'])+b''.join(events))

if __name__=="__main__":main()
