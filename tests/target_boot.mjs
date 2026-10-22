// SPDX-License-Identifier: GPL-3.0-only
// Actual pi32v2 package in the interpreter; NOT a hardware boot guarantee.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const [emulator,firmware,symbolFile,out]=process.argv.slice(2);
if(!out)throw Error('emulator.wasm firmware.fwsc symbols.json result.json');
const wasm=readFileSync(emulator),fw=readFileSync(firmware),syms=JSON.parse(readFileSync(symbolFile)).symbols;
const sha=b=>createHash('sha256').update(b).digest('hex');
const wallLimit=Number(process.env.IFW_SIM_WALL_MS||360000);
const duration=Number(process.env.IFW_SIM_BOOT_MS||2000);
const ram=syms.ifw_fixed_sample<0x02000000;
for(const n of ['ifw_fixed_sample','ifw_fixed_process','ifw_fixed_fx_tick'])
 assert.ok(ram?syms[n]>=syms._audio_start&&syms[n]<syms._rt_end:syms[n]>=0x02000120&&syms[n]<0x0208e0dc,n+' placement');
async function run(script){
 const x=(await WebAssembly.instantiate(wasm,{env:{fm1_jit_compile:()=>0}})).instance.exports;
 x.fm1_jit(0,0);
 const p=x.fm1_alloc(fw.length);new Uint8Array(x.memory.buffer,p,fw.length).set(fw);
 assert.equal(x.fm1_boot_fwsc(p,fw.length,0,0,240),0);
 if(syms.ifw_boot_progress){
  for(const [offset,value] of [[0,0x42574649],[4,40701],[8,11]])x.fm1_target_write32(syms.ifw_boot_progress+offset,value);
 }
 x.fm1_audit_div_cycles(48);x.fm1_audit_spi_timing(1);
 const b=new TextEncoder().encode(script),sp=x.fm1_alloc(b.length);
 new Uint8Array(x.memory.buffer,sp,b.length).set(b);assert.equal(x.fm1_bench_script(sp,b.length),0);
 let frames=0;const start=performance.now();
 while(x.fm1_ms()<(script?2000:duration)){frames+=x.fm1_render(128,4800000);assert.equal(x.fm1_halted(),0);if(performance.now()-start>wallLimit)throw Error('boot test timed out');}
 const r32=a=>x.fm1_target_read32(a)>>>0;
 if(ram)for(let i=0;i<syms._rt_end-syms._rt_start;i+=4)
   assert.equal(r32(syms._rt_start+i),r32(syms._rt_load+i),'RAM load/run copy at '+i);
 for(const [name,bytes] of Object.entries({ifw_sine_table:4100,ifw_ap_table:5520,ifw_ap_half_table:5520,
   ifw_alpha_table:10244,ifw_f3_table:15364,decay_curve_q31:2404})) {
   const a=syms[name];assert.ok(a>=syms._data_start&&a+bytes<=syms._data_end,name+' RAM bounds');
   for(let i=0;i<bytes;i+=4)assert.equal(r32(a+i),r32(syms._data_load+a-syms._data_start+i),name+' .data copy at '+i);
 }
 if(syms.ifw_boot_progress){
  for(const n of ['fm1_timer5_irq','isr_timer5','isr_alnk0'])assert.ok(syms[n]>=0x02000120,n+' original Flash placement');
  assert.equal(r32(syms.ifw_boot_progress+8),script?3:11);
  assert.equal(r32(syms.ifw_previous_boot+4),40701);
  assert.equal(r32(syms.ifw_previous_boot+8),11);
  assert.equal(r32(syms.ifw_boot_reason),script?2:0);
  if(!script&&duration>=31000)assert.equal(r32(syms.bootguard+8),0,'main loop must clear pending guard after 30 seconds');
 }else if(syms._irq_start) {
  for(const name of ['fm1_timer5_irq','isr_timer5','isr_alnk0'])
   assert.ok(syms[name]>=syms._irq_start&&syms[name]<syms._irq_end,name+' RAM IRQ bounds');
  const r8=a=>(r32(a&~3)>>>((a&3)*8))&255;
  for(const [name,bytes] of Object.entries({FM1_KEYMAP:66,FM1_ENC:28})) {
   const a=syms[name];assert.ok(a>=syms._data_start&&a+bytes<=syms._data_end,name+' RAM map bounds');
   for(let i=0;i<bytes;i++)assert.equal(r8(a+i),r8(syms._data_load+a-syms._data_start+i),name+' data byte '+i);
  }
 }
 return {emulatedMs:x.fm1_ms(),ready:r32(syms.ready),rescue:r32(syms.recovery_active)&255,
   blocks:r32(syms.audio_perf+8),audioFrames:frames,halted:x.fm1_halted(),
   mainMaxGapMs:syms.ui_poll_max_gap?r32(syms.ui_poll_max_gap):null,
   guardPending:r32(syms.bootguard+8),bootEvidence:syms.ifw_boot_progress?{stage:r32(syms.ifw_boot_progress+8),previousStage:r32(syms.ifw_previous_boot+8),previousVersion:r32(syms.ifw_previous_boot+4),entryReason:r32(syms.ifw_boot_reason)}:null};
}
const normal=await run('');
assert.equal(normal.rescue,0);assert.equal(normal.ready,1);assert.ok(normal.blocks>100&&normal.audioFrames>44100);
const rescue=await run('0:OCTDN,1500:OCTDN^');
assert.equal(rescue.rescue,1);assert.equal(rescue.ready,0);assert.equal(rescue.blocks,0);
const result={kind:'target-interpreter-normal-and-physical-rescue-key-boot',hardwareMeasured:false,
 watchdogResetEmulated:false,normalDurationMs:duration,priorStageInjectedForRetentionTest:!!syms.ifw_boot_progress,
 emulatorSha256:sha(wasm),firmwareSha256:sha(fw),normal,rescue,
 dspAddresses:Object.fromEntries(['ifw_fixed_sample','ifw_fixed_process','ifw_fixed_fx_tick'].map(n=>[n,syms[n]]))};
writeFileSync(out,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
