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
const e=await boot();button(e,5);assert.equal(values(e)[0],'CC2');knob(e,3,5);assert.equal(values(e)[0],'CC7');knob(e,3,-5);assert.equal(values(e)[0],'CC2');assert.equal(values(e)[1],'ALL');
knob(e,0,1);assert.equal(str(e,e.web_ui_title()),'BREATH CURVE');assert.deepEqual(values(e),['0%','33%','66%','100%']);screen(e,'linear');
// Multiple detents on the same knob must redraw the line even when the
// highlighted control did not change. Check the second point's colored edge.
const pixel=(x,y)=>new Uint16Array(e.memory.buffer,e.web_screen(),57600)[y*240+x];
const marker=(pct)=>pixel(38+Math.floor(182*33/100)-3,177-Math.floor(79*pct/100)-3);
const color=marker(33);knob(e,4,10);advance(e,120);assert.equal(marker(43),color);
knob(e,4,20);advance(e,120);assert.equal(marker(63),color);assert.notEqual(marker(43),color);
knob(e,5,19);assert.deepEqual(values(e),['0%','63%','85%','100%']);screen(e,'light');
// Accepted MIDI and rendered pixels, including true zero, quiet holds,
// unrelated CC/channel traffic, and remapping while raw input stays held.
const footer=()=>new Uint16Array(e.memory.buffer,e.web_screen()+224*240*2,240*16).slice();
const xy=(raw,mapped)=>[38+Math.round(182*raw/127),177-Math.round(79*mapped/127)];
const dot=(raw,mapped)=>pixel(...xy(raw,mapped));
const cc=(ch,num,val)=>{assert.equal(e.web_midi(0xb0+ch-1,num,val),1);advance(e,350);};
const empty=footer(),noDot=dot(0,0);
cc(1,3,65);assert.deepEqual(footer(),empty);
knob(e,0,-1);assert.equal(values(e)[0],'CC2'); // CONTROL: filter for MIDI channel 1
knob(e,4,1);assert.equal(values(e)[1],'1');knob(e,0,1);assert.equal(str(e,e.web_ui_title()),'BREATH CURVE');advance(e,350);
cc(2,2,42);assert.deepEqual(footer(),empty);
cc(1,2,0);const liveColor=dot(0,0);assert.notEqual(liveColor,noDot);assert.notDeepEqual(footer(),empty);
cc(1,2,26);assert.equal(dot(26,50),liveColor);const curve26=footer();
assert.equal(e.web_midi(0x90,60,106),1);screen(e,'live-curve26');
button(e,6);screen(e,'live-home26');assert.deepEqual(footer(),curve26);
button(e,5);if(str(e,e.web_ui_title())!=='BREATH CURVE')knob(e,0,1);
cc(1,2,70);assert.equal(dot(70,99),liveColor);screen(e,'live-curve70');
cc(1,2,84);assert.equal(dot(84,108),liveColor);
knob(e,5,-19);advance(e,300);assert.equal(dot(84,84),liveColor);assert.notEqual(dot(84,108),liveColor);
knob(e,5,19);cc(1,2,127);assert.equal(dot(127,127),liveColor);
cc(1,2,64);const held=footer();cc(2,2,0);cc(1,3,0);assert.deepEqual(footer(),held);
assert.equal(e.web_midi(0x80,60,0),1);advance(e,300);
assert.equal(e.web_flash_writes_count(),0);
button(e,6,650);knob(e,0,4);knob(e,3,1);assert.equal(e.web_ui_confirm(),5);button(e,13);advance(e,4000);assert.ok(e.web_flash_writes_count()>0);
const nor=new Uint8Array(e.memory.buffer,e.web_nor(),4096).slice(),r=await boot(nor);button(r,5);assert.equal(values(r)[0],'CC2');knob(r,0,1);assert.deepEqual(values(r),['0%','63%','85%','100%']);
console.log('PASS breath curve: GLO selector, four knobs, visible values, settings storage/reboot, no writes on edit; LCD captures in build/breath.');
