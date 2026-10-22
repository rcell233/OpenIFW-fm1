// SPDX-License-Identifier: GPL-3.0-only
// Compare production x2 with the same DSP/UI at x1; only the final PCM changes.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
const production=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
const unity=await WebAssembly.compile(readFileSync('build/test-web/openifw-half-rate-x1.wasm'));
const ceiling=32767/65536,lsb=1/65536;
let samples=0,stateChecks=0,roundingSteps=0;
for(const program of [4,18,14,10,8,21])for(const master of [0,512,1023]) {
  const make=async module=>{
    const e=(await WebAssembly.instantiate(module,{})).exports;
    e.web_nor_erase();e.web_boot();e.web_master(master);e.web_midi(0xc0,program,0);return e;
  };
  const a=await make(production),b=await make(unity);
  for(let block=0;block<300;block++) {
    for(const e of [a,b]) {
      if(block===4)e.web_midi(0x90,60,110);
      if(block===50)e.web_midi(0x90,67,127);
      if(block===70)e.web_midi(0x90,67,90);
      if(block===90)e.web_midi(0x80,67,0);
      if(block===110)e.web_midi(0xb0,2,64);
      if(block===130)e.web_midi(0xb0,2,127);
      if(block===150)e.web_midi(0xb0,64,127);
      if(block===170)e.web_midi(0x80,60,0);
      if(block===190)e.web_midi(0xb0,64,0);
      e.web_render(256);
    }
    for(const output of ['web_out_l','web_out_r']) {
      const actual=new Float32Array(a.memory.buffer,a[output](),256);
      const reference=new Float32Array(b.memory.buffer,b[output](),256);
      for(let i=0;i<256;i++) {
        const expected=Math.max(-ceiling,Math.min(ceiling,reference[i]*2));
        const delta=actual[i]-expected;
        // Combining MASTER and x2 removes one truncation. The difference is
        // exactly zero or one output LSB; the same final bound still applies.
        assert.ok(delta===0||delta===lsb,`${program}/${master}/${block}: ${delta}`);
        if(delta)roundingSteps++;
        assert.ok(Math.abs(actual[i])<=ceiling);samples++;
      }
    }
    for(const name of ['web_note','web_held','web_preset','web_now_ms','web_page','web_home']) {
      assert.equal(a[name](),b[name](),name);stateChecks++;
    }
    for(let i=0;i<4;i++) {
      assert.equal(a.web_env_stage(i),b.web_env_stage(i));
      assert.equal(a.web_env_level(i),b.web_env_level(i));stateChecks+=2;
    }
    assert.deepEqual(new Uint8Array(a.memory.buffer,a.web_patch(),916),new Uint8Array(b.memory.buffer,b.web_patch(),916));stateChecks++;
  }
}
mkdirSync('build/output-level',{recursive:true});
writeFileSync('build/output-level/wasm.json',JSON.stringify({scenes:18,samples,stateChecks,roundingSteps,
  gain:2,maxRoundingDifference:lsb,coreAndPatchStateExact:true},null,2)+'\n');
console.log(`PASS production output x2: ${samples} channel samples in 18 scenes, ${stateChecks} exact patch/state checks; at most one output LSB from combined MASTER rounding.`);
