#!/usr/bin/env python3
"""Read pi32v2 ELF32 symbols without needing its proprietary objdump at runtime."""
import json,struct,sys
from pathlib import Path
p=Path(sys.argv[1]);b=p.read_bytes()
if b[:6]!=b'\x7fELF\x01\x01':raise SystemExit('Expected little-endian ELF32')
h=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
shoff,shentsize,shnum=h[6],h[11],h[12]
sections=[struct.unpack_from('<10I',b,shoff+i*shentsize) for i in range(shnum)]
symbols={};functions=[]
for s in sections:
    if s[1]!=2:continue
    strings=sections[s[6]];names=b[strings[4]:strings[4]+strings[5]]
    for off in range(s[4],s[4]+s[5],s[9]):
        n,addr,size,info,other,ndx=struct.unpack_from('<IIIBBH',b,off)
        name=names[n:names.find(b'\0',n)].decode('utf8',errors='replace')
        if not name or not ndx:continue
        symbols[name]=addr
        if info&15==2 and size:functions.append(dict(name=name,address=addr,bytes=size))
print(json.dumps(dict(elf=p.name,symbols=symbols,functions=functions),indent=2))
