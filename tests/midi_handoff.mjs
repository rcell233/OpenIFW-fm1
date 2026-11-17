// SPDX-License-Identifier: GPL-3.0-only
// Feed real parsed MIDI to independent instances of the published C emulator.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const module=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
async function boot(program){
  const e=(await WebAssembly.instantiate(module,{})).exports;
  e.web_nor_erase();e.web_boot();e.web_master(1023);
  e.web_midi(0xc0,program,0);e.web_midi(0xb0,7,89);e.web_midi(0x90,60,96);
  for(let i=0;i<38;i++)e.web_render(256);
  return e;
}
for(const pg of [4,18,14,10,8,21])for(const sustain of [false,true]){
  const a=await boot(pg),b=await boot(pg);
  if(sustain)for(const x of [a,b]){
    x.web_midi(0xb0,64,127);x.web_midi(0x80,60,0);x.web_render(256);
    assert.equal(x.web_held(),1);
  }
  const off=sustain?[0xb0,64,0]:[0x80,60,0];
  a.web_midi(...off);a.web_midi(0x90,67,77);
  b.web_midi(0x90,67,77);b.web_midi(...off);
  let nonzero=false;
  for(let i=0;i<32;i++){
    a.web_render(256);b.web_render(256);
    assert.equal(a.web_note(),67);assert.equal(a.web_held(),1);
    for(const ch of ['web_out_l','web_out_r']){
      const samples=x=>new Float32Array(x.memory.buffer,x[ch](),256);
      assert.deepEqual(samples(a),samples(b));
      if(samples(a).some(v=>v!==0))nonzero=true;
    }
  }
  assert.ok(nonzero);
}
console.log('PASS USB MIDI application path: six presets, direct handoffs and sustain pedal handoffs, both channel streams exact.');
