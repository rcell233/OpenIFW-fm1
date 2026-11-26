#!/usr/bin/env python3
"""Report spectrum and dynamics from tests/take_parity.mjs outputs."""
import json
import sys
import numpy as np
from scipy.signal import resample_poly
from pathlib import Path
d=Path(sys.argv[2] if len(sys.argv)>2 else 'build/parity'); fs=44100
def db(a):return 20*np.log10(max(a,1e-14))
def rms(x):return np.sqrt(np.mean(x*x))
def read(name,ext,i):
    return np.fromfile(d/(name+ext),np.int32 if 'i32' in ext else np.float32).reshape(-1,i).astype(float)
def bands(x):
    fx=np.abs(np.fft.rfft(x*np.hanning(len(x))[:,None],axis=0))**2
    hz=np.fft.rfftfreq(len(x),1/fs)
    return np.sqrt([np.sum(fx[(hz>=a)&(hz<b)]) for a,b in zip([20,100,250,500,1000,2000,4000,8000,16000],[100,250,500,1000,2000,4000,8000,16000,22051])])
for k in ['dry-rc17','dry-32','no-reverb','wet']:
    u=read(k,'-up.f32',2);f=read(k,'-fw.i32',12)[:,:2]/2**20
    dt=441
    eu=np.sqrt(np.mean(u[:len(u)//dt*dt].reshape(-1,dt,2)**2,axis=(1,2)))
    ef=np.sqrt(np.mean(f[:len(f)//dt*dt].reshape(-1,dt,2)**2,axis=(1,2)))
    ok=eu>=.003
    print(k,'peak',abs(u).max(),abs(f).max(),'RMS dB',db(rms(f)/rms(u)),'difference rel dB',db(rms(f-u)/rms(u)),'sample RMSE',rms(f-u),'max',abs(f-u).max(),'p95/peak active window delta dB',np.round(np.percentile(abs(20*np.log10(ef[ok]/eu[ok])),[95,100]),3),'band delta dB',np.round(20*np.log10(bands(f)/bands(u)),3))
for k in ['upstream-default','upstream-cc2-with-cc5']:
    u=read(k,'-up.f32',2);print(k, 'rms',rms(u),'peak',abs(u).max())
u48 = read('upstream-matched-live-48k','-up.f32',2)
u48 = resample_poly(u48,147,160,axis=0)
f = read('dry-32','-fw.i32',12)[:,:2]/2**20
n = min(len(u48),len(f))
print('upstream live event timestamps at 48 kHz, resampled to 44.1 kHz for comparison:', 'RMS delta dB',db(rms(f[:n])/rms(u48[:n])),'band delta dB',np.round(20*np.log10(bands(f[:n])/bands(u48[:n])),3))
v=json.loads(Path(sys.argv[1]).read_bytes());e=v['events']
n=[x for x in e if x['bytes'][0]>>4==9 and x['bytes'][2]>0]
print('note on measured notice to render us',np.percentile([x['dmaNoticeToDispatchUs']+x.get('dispatchToNextSliceUs',x.get('dispatchToNextBlockUs')) for x in n],[0,50,100]))
