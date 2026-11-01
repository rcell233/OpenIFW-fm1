// SPDX-License-Identifier: GPL-3.0-only
// Real input, actual PCM and flash. ALGORITHM may only change palette/notice.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
const module=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
const names=['SLOOP','FOREST','AMBER','MONO','OCEAN','VIOLET','EMBER','ICE'];
async function make(nor){const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();if(nor)new Uint8Array(e.memory.buffer,e.web_nor(),4096).set(nor);e.web_boot();return e;}
const patch=e=>new Uint8Array(e.memory.buffer,e.web_patch(),916).slice();
function advance(e,ms){const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);}
function button(e,b,ms=24){e.web_buttons(1<<b);advance(e,ms);e.web_buttons(0);advance(e,280);}
function enc(e,k,n){e.web_enc(k,n);advance(e,24);}
function string(e,p){const m=new Uint8Array(e.memory.buffer);return new TextDecoder().decode(m.subarray(p,m.indexOf(0,p)));}
let samples=0,checks=0;
const eq=(a,b,m)=>{assert.deepEqual(a,b,m);checks++;};
for(const program of [4,18,14,10,8,21]) {
 const a=await make(),b=await make();eq(b.web_ui_palette_count(),8);let theme=0;
 for(const e of [a,b]){e.web_master(1023);e.web_midi(0xc0,program,0);}
 for(let i=0;i<800;i++) {
  for(const e of [a,b]) {
   if(i%9===0&&i<650)e.web_midi(0x90,48+(i/9)%24,110);
   if(i%9===7&&i<650)e.web_midi(0x80,48+((i-7)/9)%24,0);
   if(i%55===0)e.web_buttons(1<<[4,2,3,9,0,5,6][Math.floor(i/55)%7]);
   if(i%55===5)e.web_buttons(0);
   if(i%55===15)e.web_enc(0,1);
   if(i%55===25)e.web_enc(3,1);
   if(i===660)e.web_midi(0xb0,123,0);
  }
  if(i%3===0){const d=i%2?17:-9;b.web_enc(1,d);theme=((theme+d)%8+8)%8;}
  a.web_render(256);b.web_render(256);eq(b.web_ui_palette(),theme);
  for(const f of ['web_out_l','web_out_r']) {
   eq(new Uint8Array(a.memory.buffer,a[f](),1024),new Uint8Array(b.memory.buffer,b[f](),1024),'palette changed PCM');samples+=256;
  }
  for(const f of ['web_note','web_held','web_preset','web_page','web_output_mode','web_ui_layer','web_ui_slot','web_now_ms'])eq(a[f](),b[f](),f);
  for(let k=0;k<4;k++){eq(a.web_env_stage(k),b.web_env_stage(k));eq(a.web_env_level(k),b.web_env_level(k));eq(a.web_ui_control_id(k),b.web_ui_control_id(k));}
  eq(patch(a),patch(b));eq(b.web_flash_writes_count(),0,'palette detent wrote Flash');
 }
}
// Menu classification uses SELECT; PRESETS moves rows. All 8 values save and
// survive the existing CRC/quiet gate, without touching the patch/output mode.
for(let theme=0;theme<8;theme++) {
 const e=await make(),before=patch(e);advance(e,40);button(e,6,550);
 enc(e,1,theme);eq(e.web_ui_palette(),theme);eq(string(e,e.web_ui_control_value(0)),names[theme]);
 eq(patch(e),before);eq(e.web_flash_writes_count(),0);
 enc(e,0,4);eq(string(e,e.web_ui_control_name(0)),'SAVE SETUP');enc(e,3,1);eq(e.web_ui_confirm(),5);
 enc(e,1,1);eq(e.web_ui_confirm(),5,'theme knob dismissed confirmation');eq(patch(e),before);
 button(e,13);advance(e,2200);
 const nor=new Uint8Array(e.memory.buffer,e.web_nor(),4096).slice();
 eq(nor[3840+8],((theme+1)%8)%4,'rc10 base palette remains valid');
 eq(nor[3840+59],0xb0|((theme+1)%8),'versioned extended theme');
 const reboot=await make(nor);
 eq(reboot.web_ui_palette(),(theme+1)%8);eq(patch(reboot),before);eq(reboot.web_output_mode(),1);
}
// Naming: PRESETS picks letters, SELECT moves cursor; ALGORITHM is still color.
const e=await make();advance(e,40);button(e,7);button(e,13);eq(e.web_ui_naming(),1);
const before=string(e,e.web_ui_control_value(0));enc(e,1,7);eq(string(e,e.web_ui_control_value(0)),before);eq(e.web_ui_naming(),1);
enc(e,2,1);assert.notEqual(string(e,e.web_ui_control_value(0)),before);enc(e,0,1);enc(e,2,1);eq(e.web_ui_naming(),1);eq(e.web_flash_writes_count(),0);
mkdirSync('build/sloop-ui',{recursive:true});writeFileSync('build/sloop-ui/themes.json',JSON.stringify({checks,exactChannelSamples:samples,themes:names,allThemesPersist:true,colorKnobWritesFlash:false,hardwareMeasured:false},null,2)+'\n');
console.log(`PASS themes: ${checks} checks, ${samples} exact PCM samples during dense notes/page/knob changes; all eight CRC settings restored; confirm/naming isolation.`);
