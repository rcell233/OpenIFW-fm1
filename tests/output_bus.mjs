// SPDX-License-Identifier: GPL-3.0-only
// Real C menu/storage/event bridge and loud/bypass state comparison in WASM.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
const current=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
const rc8=await WebAssembly.compile(readFileSync('build/test-web/openifw-half-rate-rc8.wasm'));
async function make(module=current,nor){
  const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();
  if(nor)new Uint8Array(e.memory.buffer,e.web_nor(),4096).set(nor);
  e.web_boot();return e;
}
function advance(e,ms){const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);}
function button(e,b,ms=24){e.web_buttons(1<<b);advance(e,ms);e.web_buttons(0);advance(e,40);}
function enc(e,k,n){e.web_enc(k,n);advance(e,24);}
function string(e,p){const mem=new Uint8Array(e.memory.buffer);return new TextDecoder().decode(mem.subarray(p,mem.indexOf(0,p)));}
const patch=e=>Buffer.from(new Uint8Array(e.memory.buffer,e.web_patch(),916));
const e=await make();advance(e,50);assert.equal(e.web_output_mode(),1);
const original=patch(e);button(e,6,550);assert.equal(e.web_ui_menu(),1);
enc(e,0,2);enc(e,2,4);
assert.equal(string(e,e.web_ui_control_name(0)),'OUTPUT');
assert.equal(string(e,e.web_ui_control_value(0)),'LOUD');
enc(e,3,-1);assert.equal(e.web_output_mode(),0);
assert.equal(string(e,e.web_ui_control_value(0)),'NORMAL');
assert.deepEqual(patch(e),original);assert.equal(e.web_flash_writes_count(),0);
enc(e,0,2);enc(e,3,1);assert.equal(e.web_ui_confirm(),5);button(e,13);advance(e,2500);
assert.ok(e.web_flash_writes_count()>0);
const normalNor=new Uint8Array(e.memory.buffer,e.web_nor(),4096).slice();
const reboot=await make(current,normalNor);assert.equal(reboot.web_output_mode(),0);
assert.deepEqual(patch(reboot),original);
// Old preferences used the same byte as zero-filled padding. Preserve their
// exact storage layout/CRC and migrate only that byte to the new default.
function crc(bytes){let c=0xffffffff;for(const v of bytes){c^=v;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (~c)>>>0;}
const legacy=normalNor.slice(),at=3840,size=60;
legacy[at+19]=0;legacy[at+8]=2; // legacy AMBER palette must survive migration
new DataView(legacy.buffer).setUint32(at+4,crc(legacy.subarray(at+8,at+size)),true);
const migrated=await make(current,legacy);assert.equal(migrated.web_output_mode(),1);
assert.deepEqual(patch(migrated),original);assert.equal(migrated.web_flash_writes_count(),0);
button(migrated,6,550);assert.equal(string(migrated,migrated.web_ui_control_value(0)),'AMBER');
// An invalid preference CRC must not be accepted as a saved NORMAL mode.
const corrupt=normalNor.slice();corrupt[at+4]^=1;
assert.equal((await make(current,corrupt)).web_output_mode(),1);
let stateChecks=0,normalSamples=0,loudSamples=0;
for(const mode of [0,1])for(const program of [4,18,14,10,8,21])for(const master of [0,512,1023]){
  const nor=mode?undefined:normalNor,a=await make(current,nor),b=await make(rc8,nor);
  for(const x of [a,b]){x.web_master(master);x.web_midi(0xc0,program,0);}
  for(let block=0;block<400;block++){
    for(const x of [a,b]){
      if(block>=8&&block<260&&block%5===0){const n=48+(block/5)%25;x.web_midi(0x90,n,110);}
      if(block>=10&&block<262&&block%5===2){const n=48+((block-2)/5)%25;x.web_midi(0x80,n,0);}
      if(block===270)x.web_midi(0xb0,123,0);
      x.web_render(256);
    }
    for(const name of ['web_note','web_held','web_preset','web_now_ms','web_page','web_home']){
      assert.equal(a[name](),b[name](),name);stateChecks++;
    }
    for(let i=0;i<4;i++){
      assert.equal(a.web_env_stage(i),b.web_env_stage(i));
      assert.equal(a.web_env_level(i),b.web_env_level(i));stateChecks+=2;
    }
    assert.deepEqual(patch(a),patch(b));stateChecks++;
    for(const f of ['web_out_l','web_out_r']){
      const l=new Float32Array(a.memory.buffer,a[f](),256),r=new Float32Array(b.memory.buffer,b[f](),256);
      if(!mode){assert.deepEqual(l,r,'NORMAL differs from rc8');normalSamples+=256;}
      else for(const v of l){assert.ok(Number.isFinite(v)&&Math.abs(v)<=30720/65536);loudSamples++;}
    }
  }
}
mkdirSync('build/output-bus',{recursive:true});
writeFileSync('build/output-bus/wasm.json',JSON.stringify({stateChecks,normalSamples,loudSamples,
  menuTested:true,settingsRoundTrip:true,rc8PreferencesMigrated:true,patchesUnchanged:true},null,2)+'\n');
console.log(`PASS output bus: ${normalSamples} exact rc8 bypass samples; ${loudSamples} bounded loud samples; ${stateChecks} exact playing/patch state checks; OUTPUT menu and settings migration.`);
