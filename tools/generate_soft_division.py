#!/usr/bin/env python3
"""Regenerate the 2 KiB seed table and verify its finite divisor domain."""
import sys
from pathlib import Path

p=Path(__file__).resolve().parents[1]/'firmware/src/fixed/soft_division.h'
seeds=[(1<<32)//(d|63) for d in range(32768,65536,64)]
for d in range(32768,65536):
    r=seeds[(d>>6)-512]
    r+=r*((1<<32)-1-d*r)>>32
    assert 0<=(1<<32)//d-r<=1 and 0<d*r<1<<32
    if (1<<32)-d*r>=d:r+=1
    assert r==(1<<32)//d
text=p.read_text();start=text.index('static const uint32_t ifw_recip16_seed[512] = {\n')
start=text.index('\n',start)+1;end=text.index('};',start)
rows=''.join('    '+', '.join(str(x)+'u' for x in seeds[i:i+8])+',\n' for i in range(0,512,8))
expected=text[:start]+rows+text[end:]
if '--check' in sys.argv:assert expected==text,'Reciprocal seed table is stale'
else:p.write_text(expected)
print('PASS reciprocal seeds: 512 entries, every normalized divisor verified')
