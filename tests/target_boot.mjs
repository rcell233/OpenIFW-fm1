// SPDX-License-Identifier: GPL-3.0-only
// Actual pi32v2 package in the interpreter; NOT a hardware boot guarantee.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const [emulator,firmware,symbolFile,out]=process.argv.slice(2);
if(!out)throw Error('emulator.wasm firmware.fwsc symbols.json result.json');
const wasm=readFileSync(emulator),fw=readFileSync(firmware),syms=JSON.parse(readFileSync(symbolFile)).symbols;
const sha=b=>createHash('sha256').update(b).digest('hex');
const wallLimit=Number(process.env.IFW_SIM_WALL_MS||120000);
const ram=syms.ifw_fixed_sample<0x02000000;
for(const n of ['ifw_fixed_sample','ifw_fixed_process','ifw_fixed_fx_tick'])
 assert.ok(ram?syms[n]>=syms._audio_start&&syms[n]<syms._rt_end:syms[n]>=0x02000120&&syms[n]<0x0208e0dc,n+' placement');
async function run(script){
 const x=(await WebAssembly.instantiate(wasm,{env:{fm1_jit_compile:()=>0}})).instance.exports;
 x.fm1_jit(0,0);
 const p=x.fm1_alloc(fw.length);new Uint8Array(x.memory.buffer,p,fw.length).set(fw);
 assert.equal(x.fm1_boot_fwsc(p,fw.length,0,0,240),0);
 x.fm1_audit_div_cycles(48);x.fm1_audit_spi_timing(1);
 const b=new TextEncoder().encode(script),sp=x.fm1_alloc(b.length);
 new Uint8Array(x.memory.buffer,sp,b.length).set(b);assert.equal(x.fm1_bench_script(sp,b.length),0);
 let frames=0;const start=performance.now();
 while(x.fm1_ms()<2000){frames+=x.fm1_render(128,4800000);assert.equal(x.fm1_halted(),0);if(performance.now()-start>wallLimit)throw Error('boot test timed out');}
 const r32=a=>x.fm1_target_read32(a)>>>0;
 if(ram)for(let i=0;i<syms._rt_end-syms._rt_start;i+=4)
   assert.equal(r32(syms._rt_start+i),r32(syms._rt_load+i),'RAM load/run copy at '+i);
 for(const [name,bytes] of Object.entries({ifw_sine_table:4100,ifw_ap_table:5520,ifw_ap_half_table:5520,
   ifw_alpha_table:10244,ifw_f3_table:15364,decay_curve_q31:2404})) {
   const a=syms[name];assert.ok(a>=syms._data_start&&a+bytes<=syms._data_end,name+' RAM bounds');
   for(let i=0;i<bytes;i+=4)assert.equal(r32(a+i),r32(syms._data_load+a-syms._data_start+i),name+' .data copy at '+i);
 }
 return {emulatedMs:x.fm1_ms(),ready:r32(syms.ready),rescue:r32(syms.recovery_active)&255,
   blocks:r32(syms.audio_perf+8),audioFrames:frames,halted:x.fm1_halted()};
}
const normal=await run('');
assert.equal(normal.rescue,0);assert.equal(normal.ready,1);assert.ok(normal.blocks>100&&normal.audioFrames>44100);
const rescue=await run('0:OCTDN,1500:OCTDN^');
assert.equal(rescue.rescue,1);assert.equal(rescue.ready,0);assert.equal(rescue.blocks,0);
const result={kind:'target-interpreter-normal-and-physical-rescue-key-boot',hardwareMeasured:false,
 emulatorSha256:sha(wasm),firmwareSha256:sha(fw),normal,rescue,
 dspAddresses:Object.fromEntries(['ifw_fixed_sample','ifw_fixed_process','ifw_fixed_fx_tick'].map(n=>[n,syms[n]]))};
writeFileSync(out,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
