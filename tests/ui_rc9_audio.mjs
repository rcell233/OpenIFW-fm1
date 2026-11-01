// SPDX-License-Identifier: GPL-3.0-only
// UI edits must preserve the released rc9 audio AND control/playing state.
// The baseline can only be recorded from the authenticated published rc9 WASM.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
const referenceSha='accc8ae24fc6b3b99ef7187829a3c6d9e9c5d3bcfd37229de51c52e6734b7428';
const fixture='docs/evidence/ui-rc9-audio.json';
const recording=process.argv[2]==='--record-baseline';
const wasm=readFileSync(recording?process.argv[3]:'build/web/openifw.wasm');
const sha=b=>createHash('sha256').update(b).digest('hex');
if(recording)assert.equal(sha(wasm),referenceSha,'Only published rc9 can establish this baseline');
const module=await WebAssembly.compile(wasm),rows=[];let samples=0,stateChecks=0;
for(const program of [4,18,14,10,8,21])for(const master of [512,1023]) {
 const e=(await WebAssembly.instantiate(module,{})).exports;
 e.web_nor_erase();e.web_boot();e.web_master(master);e.web_midi(0xc0,program,0);
 const audio=createHash('sha256'),state=createHash('sha256'),number=Buffer.alloc(8);
 const add=v=>{number.writeDoubleLE(v);state.update(number);stateChecks++;};
 for(let block=0;block<600;block++) {
  if(block>=10&&block<450&&block%5===0)e.web_midi(0x90,48+(block/5)%25,110);
  if(block>=12&&block<452&&block%5===2)e.web_midi(0x80,48+((block-2)/5)%25,0);
  if(block%30===0)e.web_buttons(1<<[4,2,3,9,0,5,6][Math.floor(block/30)%7]);
  if(block%30===3)e.web_buttons(0);
  if(block%30===15)e.web_enc(0,1);
  if(block%40===18)e.web_enc(3,(block/40)&1?1:-1);
  if(block===470)e.web_midi(0xb0,123,0);
  e.web_render(256);
  for(const f of ['web_out_l','web_out_r']) {
   audio.update(new Uint8Array(e.memory.buffer,e[f](),1024));samples+=256;
  }
  for(const f of ['web_note','web_held','web_page','web_preset','web_output_mode','web_now_ms'])add(e[f]());
  for(let i=0;i<4;i++){add(e.web_env_stage(i));add(e.web_env_level(i));}
  state.update(new Uint8Array(e.memory.buffer,e.web_patch(),916));stateChecks++;
 }
 rows.push({program,master,frames:153600,pcmSha256:audio.digest('hex'),stateSha256:state.digest('hex')});
}
const result={referenceWasmSha256:referenceSha,samples,stateChecks,rows};
if(recording){writeFileSync(fixture,JSON.stringify(result,null,2)+'\n');console.log('Recorded authenticated rc9 baseline');}
else {
 assert.deepEqual(result,JSON.parse(readFileSync(fixture)),'UI-only update changed rc9 PCM or control state');
 mkdirSync('build/ui-refresh',{recursive:true});
 writeFileSync('build/ui-refresh/audio.json',JSON.stringify({...result,currentWasmSha256:sha(wasm),exact:true},null,2)+'\n');
 console.log(`PASS rc9 audio: ${samples} exact channel samples and ${stateChecks} exact control/playing-state checks during dense notes, edits and page changes.`);
}
