#!/usr/bin/env python3
"""Readable Inter Tight UI bitmaps (SIL OFL 1.1), padded for the panel grid."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1]
out=['/* Inter Tight, SIL OFL 1.1; see assets/fonts/OFL-InterTight.txt. */']
for name,size,height,weight in [('UI_S',12,16,550),('UI_M',16,20,600),('UI_L',23,28,600)]:
 f=ImageFont.truetype(str(R/'assets/fonts/InterTight.ttf'),size)
 f.set_variation_by_axes([weight]);adv=[];bw=[];off=[];data=[]
 for c in map(chr,range(32,127)):
  a=round(f.getlength(c));w=a+2;w+=(w%2);im=Image.new('L',(w,height));ImageDraw.Draw(im).text((1,1),c,font=f,fill=255,stroke_width=0)
  adv.append(a);bw.append(w);off.append(len(data))
  for y in range(height):
   for x in range(0,w,2):data.append(((im.getpixel((x,y))*15+127)//255)<<4 | ((im.getpixel((x+1,y))*15+127)//255))
 for n,values,typ in [('DATA',data,'uint8_t'),('ADV',adv,'uint8_t'),('BW',bw,'uint8_t'),('OFF',off,'uint16_t')]:
  out.append(f'static const {typ} {name}_{n}[]={{'+','.join(map(str,values))+'};')
 out.append(f'static const felucca_font_t {name}={{ {height},1,32,126,{name}_ADV,{name}_BW,{name}_OFF,{name}_DATA }};')
(R/'firmware/src/ui/font.h').write_text('\n'.join(out)+'\n');print('Generated Inter Tight UI font bitmaps')
