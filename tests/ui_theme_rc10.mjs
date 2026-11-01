// SPDX-License-Identifier: GPL-3.0-only
// Optional release audit against the authenticated published rc10 WebAssembly.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
const oldBytes=readFileSync(process.argv[2]||'build/sloop-ui/rc10.wasm');
const referenceSha256=createHash('sha256').update(oldBytes).digest('hex');
assert.equal(referenceSha256,'3a288b113685acbf324687a98e06563aae1b4d6d9b312222a3470d2ca7c577e7');
const old=await WebAssembly.compile(oldBytes),current=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
async function make(module,nor){const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();if(nor)new Uint8Array(e.memory.buffer,e.web_nor(),4096).set(nor);e.web_boot();return e;}
function advance(e,ms){const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);}
function button(e,b,ms=24){e.web_buttons(1<<b);advance(e,ms);e.web_buttons(0);advance(e,280);}
function enc(e,k,n){e.web_enc(k,n);advance(e,24);}
const patch=e=>new Uint8Array(e.memory.buffer,e.web_patch(),916).slice();
function string(e,p){const m=new Uint8Array(e.memory.buffer);return new TextDecoder().decode(m.subarray(p,m.indexOf(0,p)));}
for(let theme=0;theme<8;theme++) {
 const e=await make(current);advance(e,40);enc(e,1,theme);const original=patch(e);
 button(e,6,550);enc(e,0,2);enc(e,2,4);enc(e,3,-1);assert.equal(e.web_output_mode(),0);
 enc(e,0,2);enc(e,3,1);assert.equal(e.web_ui_confirm(),5);button(e,13);advance(e,2200);
 const nor=new Uint8Array(e.memory.buffer,e.web_nor(),4096).slice(),legacy=await make(old,nor);
 assert.equal(legacy.web_output_mode(),0,'rc10 must retain NORMAL with an extended theme');
 assert.deepEqual(patch(legacy),original);assert.equal(legacy.web_flash_writes_count(),0);
 button(legacy,6,550);assert.equal(string(legacy,legacy.web_ui_control_value(0)),['BLUE','GREEN','AMBER','MONO'][theme%4]);
}
mkdirSync('build/sloop-ui',{recursive:true});writeFileSync('build/sloop-ui/rc10-compatibility.json',JSON.stringify({referenceSha256,themes:8,rc10PreservesNormal:true,patchExact:true,noFlashWritesOnRead:true},null,2)+'\n');
console.log('PASS published rc10 compatibility: all eight saved themes retain NORMAL output, original patch and valid legacy preferences.');
