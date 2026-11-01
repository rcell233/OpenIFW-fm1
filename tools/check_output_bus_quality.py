#!/usr/bin/env python3
"""Coherent digital sine THD+N of the optional bus (not analog DAC quality)."""
import ctypes as c
import json
from pathlib import Path
import subprocess
import numpy as np

root=Path(__file__).resolve().parents[1]
out=root/'build/output-bus';out.mkdir(parents=True,exist_ok=True)
library=out/'bus.so'
subprocess.run(['cc','-O3','-shared','-fPIC','-std=c11','-Wall','-Wextra','-Werror',
                str(root/'firmware/src/output_bus.c'),'-o',str(library)],check=True)
class Bus(c.Structure):
    _fields_=[(n,c.c_uint32) for n in ['envelope','mix','mode','min_gain','soft_samples','peak']]
lib=c.CDLL(str(library))
lib.ifw_output_bus_process.argtypes=[c.POINTER(Bus),c.POINTER(c.c_int32),c.c_uint32,c.c_int32]
rows=[]
for period in [441,1024]:
    for amp in [4000,12000,30000]:
        n=period*200
        raw=np.rint(amp*np.sin(np.arange(n)*2*np.pi/period)).astype(np.int32)
        pcm=np.column_stack([raw,raw]).copy()
        b=Bus(24576,256,1,16384,0,0)
        lib.ifw_output_bus_process(c.byref(b),pcm.ctypes.data_as(c.POINTER(c.c_int32)),n,4096)
        # Discard 160 periods, then measure an integer number of cycles without
        # a window. Includes the original Q15 quantization, excludes DC.
        spectrum=np.fft.rfft(pcm[-period*40:,0].astype(float))
        fundamental=abs(spectrum[40])**2
        energy=(np.abs(spectrum)**2).sum()-abs(spectrum[0])**2
        thdn=(max(0,energy-fundamental)/fundamental)**.5
        row=dict(frequency=44100/period,rawAmplitude=amp,
                 thdPlusNoisePercent=100*thdn,softChannelSamples=b.soft_samples,
                 peakQ15=b.peak,linearExact=bool(np.array_equal(pcm[:,0],raw*4)))
        if amp==4000:assert row['linearExact']
        assert b.peak<=30720
        rows.append(row)
(out/'tone-quality.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS low-level linearity; recorded six digital sine THD+N measurements (not hardware).')
