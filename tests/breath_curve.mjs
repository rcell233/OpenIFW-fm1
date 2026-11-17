// SPDX-License-Identifier: GPL-3.0-only
// Exercise the actual shared firmware UI, render and settings sector.
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
const mod=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
const out='build/breath';mkdirSync(out,{recursive:true});
const str=(e,p)=>{const m=new Uint8Array(e.memory.buffer);return new TextDecoder().decode(m.subarray(p,m.indexOf(0,p)));};
async function boot(nor){const e=(await WebAssembly.instantiate(mod,{})).exports;e.web_nor_erase();if(nor)new Uint8Array(e.memory.buffer,e.web_nor(),4096).set(nor);e.web_boot();advance(e,80);return e;}
function advance(e,ms){const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);}
function button(e,k,ms=30){e.web_buttons(1<<k);advance(e,ms);e.web_buttons(0);advance(e,300);}
function knob(e,k,d){e.web_enc(k,d);advance(e,64);}
const values=e=>Array.from({length:4},(_,k)=>str(e,e.web_ui_control_value(k)));
function screen(e,name){advance(e,1500);const m=new Uint16Array(e.memory.buffer,e.web_screen(),57600),pixels=Buffer.alloc(m.length*3);for(let i=0;i<m.length;i++){const p=(m[i]<<8|m[i]>>8)&65535;pixels[3*i]=(p>>11)*255/31;pixels[3*i+1]=(p>>5&63)*255/63;pixels[3*i+2]=(p&31)*255/31;}writeFileSync(out+'/'+name+'.ppm',Buffer.concat([Buffer.from('P6\n240 240\n255\n'),pixels]));}
const e=await boot();button(e,5);assert.equal(values(e)[0],'CC7');knob(e,3,-5);assert.equal(values(e)[0],'CC2');assert.equal(values(e)[1],'ALL');
knob(e,0,1);assert.equal(str(e,e.web_ui_title()),'BREATH CURVE');assert.deepEqual(values(e),['0%','33%','66%','100%']);screen(e,'linear');
// Multiple detents on the same knob must redraw the line even when the
// highlighted control did not change. Check the second point's colored edge.
const pixel=(x,y)=>new Uint16Array(e.memory.buffer,e.web_screen(),57600)[y*240+x];
const marker=(pct)=>pixel(38+Math.floor(182*33/100)-3,177-Math.floor(79*pct/100)-3);
const color=marker(33);knob(e,4,10);advance(e,120);assert.equal(marker(43),color);
knob(e,4,20);advance(e,120);assert.equal(marker(63),color);assert.notEqual(marker(43),color);
knob(e,5,19);assert.deepEqual(values(e),['0%','63%','85%','100%']);screen(e,'light');
assert.equal(e.web_flash_writes_count(),0);
button(e,6,650);knob(e,0,4);knob(e,3,1);assert.equal(e.web_ui_confirm(),5);button(e,13);advance(e,4000);assert.ok(e.web_flash_writes_count()>0);
const nor=new Uint8Array(e.memory.buffer,e.web_nor(),4096).slice(),r=await boot(nor);button(r,5);assert.equal(values(r)[0],'CC2');knob(r,0,1);assert.deepEqual(values(r),['0%','63%','85%','100%']);
console.log('PASS breath curve: GLO selector, four knobs, visible values, settings storage/reboot, no writes on edit; LCD captures in build/breath.');
