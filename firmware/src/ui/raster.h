/* SPDX-License-Identifier: GPL-3.0-only
 * Pixel-equivalent shortcuts for the platform's strip canvas. In particular,
 * do not decode every glyph row once for every 16-row strip of the screen.
 * Keep the vendor renderer available as an independent pixel oracle.
 */
static int32_t cv_text(int32_t x,int32_t y,const felucca_font_t *f,
                       const char *s,uint16_t c) {
    int32_t top=y+cv_oy,first=top<0?-top:0,last=(int32_t)cv_h-top;
    if(last>f->h)last=f->h;
    if(first>=last)return x+text_w(f,s);
    /* Store the byte-swapped ramp used by the DMA canvas. Colors recur across
     * font sizes and strips; building 48 integer divisions per label is waste. */
    static struct {uint16_t color,pixel[16];uint8_t valid;} cache[8];
    uint32_t slot=(c^(c>>8))&7u;
    if(!cache[slot].valid || cache[slot].color!=c) {
        uint32_t r=c>>11,g=(c>>5)&63u,b=c&31u;
        for(uint32_t a=0;a<16u;a++)
            cache[slot].pixel[a]=swap16(((r*a/15u)<<11)|((g*a/15u)<<5)|(b*a/15u));
        cache[slot].color=c;cache[slot].valid=1;
    }
    const uint16_t *ramp=cache[slot].pixel;
    for(;*s;s++) {
        uint32_t gi=glyph(f,(uint8_t)*s),w=f->bw[gi],bpr=(w+1u)/2u;
        int32_t pen=x-f->pad,lo=pen<0?-pen:0,hi=(int32_t)cv_w-pen;
        if(hi>(int32_t)w)hi=(int32_t)w;
        const uint8_t *gd=f->data+f->off[gi];
        for(int32_t gy=first;gy<last;gy++) {
            for(int32_t gx=lo;gx<hi;gx++) {
                uint32_t v=gd[(uint32_t)gy*bpr+(uint32_t)gx/2u];
                v=(gx&1)?v&15u:v>>4;
                if(v)cv_px[(uint32_t)(top+gy)*cv_w+(uint32_t)(pen+gx)]=ramp[v];
            }
        }
        x+=f->adv[gi];
    }
    return x;
}
static void cv_line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint16_t c) {
    int32_t a=y0+cv_oy,b=y1+cv_oy;
    if((a<0 && b<0) || (a>=(int32_t)cv_h && b>=(int32_t)cv_h) ||
       (x0<0 && x1<0) || (x0>=(int32_t)cv_w && x1>=(int32_t)cv_w))return;
    if(y0==y1 && x0-x1<2000 && x1-x0<2000) {
        int32_t lo=x0<x1?x0:x1,hi=x0>x1?x0:x1;
        cv_rect(lo,y0,hi-lo+1,1,c);return;
    }
    if(x0==x1 && y0-y1<2000 && y1-y0<2000) {
        int32_t lo=y0<y1?y0:y1,hi=y0>y1?y0:y1;
        cv_rect(x0,lo,1,hi-lo+1,c);return;
    }
    /* Retain the original Bresenham phase for partially clipped diagonals. */
    ifw_original_cv_line(x0,y0,x1,y1,c);
}
