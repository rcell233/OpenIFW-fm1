#!/usr/bin/env python3
"""VT323 UI bitmaps, matching Ghoulbox's pixel-font choice (SIL OFL 1.1)."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1]
out=['/* VT323, Peter Hull, SIL OFL 1.1; see assets/fonts/OFL-VT323.txt. */']
for name,size in [('UI_S',16),('UI_M',20),('UI_L',28)]:
 f=ImageFont.truetype(str(R/'assets/fonts/VT323-Regular.ttf'),size);adv=[];bw=[];off=[];data=[]
 for c in map(chr,range(32,127)):
  a=round(f.getlength(c));w=a+2;w+=(w%2);im=Image.new('L',(w,size));ImageDraw.Draw(im).text((1,-2),c,font=f,fill=255,stroke_width=0)
  adv.append(a);bw.append(w);off.append(len(data))
  for y in range(size):
   for x in range(0,w,2):data.append((240 if im.getpixel((x,y))>=128 else 0)|(15 if im.getpixel((x+1,y))>=128 else 0))
 for n,values,typ in [('DATA',data,'uint8_t'),('ADV',adv,'uint8_t'),('BW',bw,'uint8_t'),('OFF',off,'uint16_t')]:
  out.append(f'static const {typ} {name}_{n}[]={{'+','.join(map(str,values))+'};')
 out.append(f'static const felucca_font_t {name}={{ {size},1,32,126,{name}_ADV,{name}_BW,{name}_OFF,{name}_DATA }};')
(R/'firmware/src/ui/font.h').write_text('\n'.join(out)+'\n');print('Generated VT323 UI font bitmaps')
