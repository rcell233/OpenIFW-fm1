// SPDX-License-Identifier: GPL-3.0-only
// Freeze pre-bus rc8 PCM from its original source, not a candidate bypass build.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
const referenceCommit='911d90091cb36c8dba2628f5aca8bb2022609ead';
const referenceWasmSha256='7a21e3503fb178098354e873c5c26b128fbd7ba513da07add2215c39cd04b252';
const fixture='docs/evidence/output-rc8-audio.json';
const recording=process.argv[2]==='--record-baseline';
const wasm=readFileSync(recording?process.argv[3]:'build/web/openifw.wasm');
const sha=b=>createHash('sha256').update(b).digest('hex');
if(recording)assert.equal(sha(wasm),referenceWasmSha256,'Only the authenticated original rc8 build can establish this fixture');
const module=await WebAssembly.compile(wasm),rows=[];let samples=0,stateChecks=0;
for(const program of [4,18,14,10,8,21])for(const master of [0,512,1023]) {
 const e=(await WebAssembly.instantiate(module,{})).exports;
 e.web_nor_erase();e.web_boot();e.web_master(master);e.web_midi(0xc0,program,0);
 const audio=createHash('sha256'),state=createHash('sha256'),number=Buffer.alloc(8);
 const add=v=>{number.writeDoubleLE(v);state.update(number);stateChecks++;};
 for(let block=0;block<600;block++) {
  if(block>=10&&block<450&&block%5===0)e.web_midi(0x90,48+(block/5)%25,110);
  if(block>=12&&block<452&&block%5===2)e.web_midi(0x80,48+((block-2)/5)%25,0);
  if(block===100)e.web_midi(0xb0,64,127);
  if(block===160)e.web_midi(0xb0,64,0);
  if(block===210)e.web_midi(0xe0,0,80);
  if(block===250)e.web_midi(0xe0,0,64);
  if(block===470)e.web_midi(0xb0,123,0);
  e.web_render(256);
  for(const f of ['web_out_l','web_out_r']) {
   audio.update(new Uint8Array(e.memory.buffer,e[f](),1024));samples+=256;
  }
  for(const f of ['web_note','web_held','web_preset','web_now_ms'])add(e[f]());
  for(let i=0;i<4;i++){add(e.web_env_stage(i));add(e.web_env_level(i));}
  state.update(new Uint8Array(e.memory.buffer,e.web_patch(),916));stateChecks++;
 }
 rows.push({program,master,frames:153600,pcmSha256:audio.digest('hex'),stateSha256:state.digest('hex')});
}
const result={referenceCommit,referenceWasmSha256,samples,stateChecks,rows};
if(recording){writeFileSync(fixture,JSON.stringify(result,null,2)+'\n');console.log('Recorded authenticated original rc8 output');}
else {
 assert.deepEqual(result,JSON.parse(readFileSync(fixture)),'Output or playing state differs from original rc8');
 mkdirSync('build/output-restore',{recursive:true});
 writeFileSync('build/output-restore/audio.json',JSON.stringify({...result,currentWasmSha256:sha(wasm),exact:true},null,2)+'\n');
 console.log(`PASS restored original rc8: ${samples} exact channel samples; ${stateChecks} exact playing/patch checks; six factory presets, three master settings, dense notes, sustain and bend.`);
}
