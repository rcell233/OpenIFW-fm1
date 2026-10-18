// SPDX-License-Identifier: GPL-3.0-only
// Compare only audio: a new screen is expected, a new sound is not.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
const baseline='docs/evidence/ui-audio-baseline.json',oldPath=process.env.IFW_UI_OLD_WASM;
const scenarios=[];
async function render(bytes,program,navigate){
 const e=(await WebAssembly.instantiate(bytes,{})).instance.exports;e.web_nor_erase();e.web_boot();e.web_master(800);e.web_midi(0xc0,program,0);
 const hash=createHash('sha256');let mask=0;
 for(let block=0;block<400;block++){
  if(block===4)e.web_midi(0x90,60,100);
  if(block===70)e.web_midi(0x90,67,90);
  if(block===135)e.web_midi(0x80,67,0);
  if(block===180)e.web_midi(0xb0,64,127);
  if(block===190)e.web_midi(0x80,60,0);
  if(block===220)e.web_midi(0xb0,64,0);
  if(navigate){if(block%24===0&&block<180){mask=1<<[4,2,3,9,0,5,6][Math.floor(block/24)%7];e.web_buttons(mask);}else if(mask){e.web_buttons(0);mask=0;}}
  e.web_render(256);
  hash.update(new Uint8Array(e.memory.buffer,e.web_out_l(),1024));hash.update(new Uint8Array(e.memory.buffer,e.web_out_r(),1024));
 }
 return hash.digest('hex');
}
const bytes=readFileSync('build/web/openifw.wasm'),old=oldPath?readFileSync(oldPath):null;
for(const program of [4,18,14,10,8,21])for(const navigate of [false,true]){
 const current=await render(bytes,program,navigate),name=`program-${program}-${navigate?'navigation':'play'}`;
 if(old)assert.equal(current,await render(old,program,navigate),name+' changed PCM from 0.3.0');
 scenarios.push({name,program,navigate,frames:102400,sha256:current});
}
if(old){writeFileSync(baseline,JSON.stringify({baselineVersion:'0.3.0',baselineWasmSha256:createHash('sha256').update(old).digest('hex'),sampleRate:44100,channels:2,format:'IEEE754 float32 LE planar per 256 frames',scenarios},null,2)+'\n');}
assert.ok(existsSync(baseline),'run once with IFW_UI_OLD_WASM to establish the frozen 0.3 PCM fixture');
assert.deepEqual(scenarios,JSON.parse(readFileSync(baseline)).scenarios,'PCM differs from frozen audio fixture');
console.log('PASS frozen audio: 12 scenarios / 1,228,800 stereo frames match 0.3.0 exactly, including page navigation during legato and sustain.');
