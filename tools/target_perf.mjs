// SPDX-License-Identifier: GPL-3.0-only
// Run the actual .fwsc/CPU instructions. The clock model is NOT cycle accurate.
// node tools/target_perf.mjs emulator.wasm firmware.fwsc symbols.json results.json [preset=0] [ms=4500]
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const [emulator,firmware,symbolFile,outFile,presetArg='0',durationArg='4500']=process.argv.slice(2);
if(!outFile)throw Error('emulator.wasm firmware.fwsc symbols.json results.json [preset] [ms]');
const preset=Number(presetArg),duration=Number(durationArg),mhz=Number(process.env.IFW_SIM_MHZ||240);
const wasm=readFileSync(emulator),fw=readFileSync(firmware),syms=JSON.parse(readFileSync(symbolFile));
const sha=b=>createHash('sha256').update(b).digest('hex');
const x=(await WebAssembly.instantiate(wasm,{env:{fm1_jit_compile:()=>0}})).instance.exports;
if(!x.fm1_target_read32)throw Error('Use the pinned fm1-emu source with tools/fm1-emu-profile.patch (see docs/PERFORMANCE.md).');
x.fm1_jit(0,0); // JIT is intentionally excluded from firmware correctness/performance tests.
const p=x.fm1_alloc(fw.length);new Uint8Array(x.memory.buffer,p,fw.length).set(fw);
if(x.fm1_boot_fwsc(p,fw.length,0,0,mhz))throw Error('Firmware package failed to boot');
const divCycles=Number(process.env.IFW_SIM_DIV_CYCLES||1);
if(divCycles!==1&&!x.fm1_audit_div_cycles)throw Error('Division sensitivity requires the audit emulator patch');
x.fm1_audit_div_cycles?.(divCycles);
if(process.env.IFW_SIM_SPI_TIMING){if(!x.fm1_audit_spi_timing)throw Error('SPI timing requires the audit emulator patch');x.fm1_audit_spi_timing(1);}
const script=process.env.IFW_SIM_SCRIPT??`${preset?`500:e6:${preset},`:''}1200:C4,1800:E4,2300:E4^,2700:C4^`;
const bytes=new TextEncoder().encode(script),sp=x.fm1_alloc(bytes.length);new Uint8Array(x.memory.buffer,sp,bytes.length).set(bytes);
if(x.fm1_bench_script(sp,bytes.length))throw Error('Invalid script');
if(process.env.IFW_SIM_PROFILE||process.env.IFW_SIM_PC_PROFILE)x.fm1_profile(1);
const r32=a=>x.fm1_target_read32(a)>>>0,perfAddr=syms.symbols.audio_perf;
if(!perfAddr)throw Error('Firmware must include the full audio IRQ instrumentation');
function perf(){
 const a=Array.from({length:13},(_,i)=>r32(perfAddr+i*4));
 return {ready:a[0]===0x50465749&&a[1]===0x31524550,blocks:a[2],lastUs:a[3]/24,maxUs:a[4]/24,overruns:a[5],dmaLate:a[6],
   dspLastUs:a[7]/24,dspMaxUs:a[8]/24,peak:a[9],clips:a[10],meanUs:a[2]?(a[11]+a[12]*4294967296)/a[2]/24:0};
}
const measured=[],dsp=[],audioHash=createHash('sha256');let lastBlock=0,short=0,frames=0;
const pcm=[],drawIntervals=[],underrunAt=[];let oldUnderruns=0;let modeSet=false,lastDraw=0;
const start=performance.now();
while(x.fm1_ms()<duration){
 const got=x.fm1_render(128,Math.ceil(mhz*1e6*.02));frames+=got;if(got<128)short++;
 if(got){audioHash.update(new Uint8Array(x.memory.buffer,x.fm1_out_l(),got*4));audioHash.update(new Uint8Array(x.memory.buffer,x.fm1_out_r(),got*4));}
 if(got&&process.env.IFW_SIM_PCM){const l=new Float32Array(x.memory.buffer,x.fm1_out_l(),got),r=new Float32Array(x.memory.buffer,x.fm1_out_r(),got),v=new Float32Array(got*2);
  for(let i=0;i<got;i++){v[2*i]=l[i];v[2*i+1]=r[i];}pcm.push(new Uint8Array(v.buffer));}
 if(x.fm1_halted()){
  const ep=x.fm1_last_error?.(),mem=new Uint8Array(x.memory.buffer);
  throw Error(`CPU halted at ${x.fm1_ms()} ms: ${ep?new TextDecoder().decode(mem.subarray(ep,mem.indexOf(0,ep))):''}`);
 }
 if(syms.symbols.audio_underruns){const u=r32(syms.symbols.audio_underruns);if(u!==oldUnderruns){underrunAt.push([x.fm1_ms(),u]);oldUnderruns=u;}}
 const v=perf();if(v.ready&&v.blocks!==lastBlock){lastBlock=v.blocks;if(x.fm1_ms()>=1000){measured.push(v.lastUs);dsp.push(v.dspLastUs);}}
 if(v.ready&&!modeSet&&process.env.IFW_SIM_MODE){if(!x.fm1_target_write32)throw Error('Audit emulator write export required');
  const mode={sine:1,mute:2}[process.env.IFW_SIM_MODE];if(!mode)throw Error('Unknown diagnostic mode');
  x.fm1_target_write32(syms.symbols.perf_mode,mode);modeSet=true;}
 if(syms.symbols.last_draw){const d=r32(syms.symbols.last_draw);if(d!==lastDraw){if(lastDraw>=1000)drawIntervals.push(d-lastDraw);lastDraw=d;}}
 if(performance.now()-start>Math.max(120000,duration*100))throw Error('Emulation timed out');
}
function percentiles(a){const s=[...a].sort((a,b)=>a-b),p=f=>s[Math.max(0,Math.ceil(s.length*f)-1)]??null;
 return {blocks:s.length,p50Us:p(.5),p95Us:p(.95),p99Us:p(.99),maxUs:p(1)};}
const result={kind:'pi32v2-instruction-model-not-hardware',cycleAccurate:false,hardwareMeasured:false,
 assumptions:`one cycle per non-divide instruction; ${divCycles} cycles per divide is an uncalibrated input; no cache/flash/bus model; SPI wire timing ${process.env.IFW_SIM_SPI_TIMING?'enabled at assumed 60 MHz peripheral clock':'instant'}; no attached USB host`,
 emulatorSha256:sha(wasm),firmwareSha256:sha(fw),mhz,preset,actualPreset:r32(syms.symbols.preset),script,requestedMs:duration,emulatedMs:x.fm1_ms(),
 dividerCyclesAssumption:divCycles,
 spiWireTiming:!!process.env.IFW_SIM_SPI_TIMING,
 irqBudgetUs:256/44100*1e6,all:perf(),steady:percentiles(measured),dspSteady:percentiles(dsp),
 audioFrames:frames,audioSha256:audioHash.digest('hex'),shortQuanta:short,wallSeconds:(performance.now()-start)/1000};
{const d=percentiles(drawIntervals);result.drawIntervalsMs={count:d.blocks,p50:d.p50Us,p95:d.p95Us,max:d.maxUs};}
if(syms.symbols.ui_poll_max_gap){const now=r32(syms.symbols.fm1_ms),last=r32(syms.symbols.ui_poll_last);
 result.uiService={maxGapMs:r32(syms.symbols.ui_poll_max_gap),turns:r32(syms.symbols.ui_poll_turns),nowMs:now,lastAtMs:last,sinceLastMs:(now-last)>>>0};}
if(syms.symbols.audio_yields)result.overloadYields=r32(syms.symbols.audio_yields);
if(syms.symbols.audio_underruns){result.underruns=r32(syms.symbols.audio_underruns);result.underrunAt=underrunAt;}
const widgetCount=syms.symbols.ui_widget_draw_at?6:4;
if(syms.symbols.ui_widget_max_ticks)result.widgetMaxUs=Array.from({length:widgetCount},(_,i)=>r32(syms.symbols.ui_widget_max_ticks+i*4)/24);
if(syms.symbols.ui_widget_updates)result.widgetUpdates=Array.from({length:widgetCount},(_,i)=>r32(syms.symbols.ui_widget_updates+i*4));
if(syms.symbols.note_transition_events)result.noteTransitionEvents=r32(syms.symbols.note_transition_events);
if(syms.symbols.audio_write)result.audioQueue={produced:r32(syms.symbols.audio_write),consumed:r32(syms.symbols.audio_read),partialFrames:r32(syms.symbols.audio_offset)};
if(syms.symbols.ui_frame_count)result.uiRender={completedScans:r32(syms.symbols.ui_frame_count),lcdPixels:r32(syms.symbols.ui_lcd_pixels)};
if(process.env.IFW_SIM_MODE)result.diagnosticMode=process.env.IFW_SIM_MODE;
if(process.env.IFW_SIM_PCM)writeFileSync(process.env.IFW_SIM_PCM,Buffer.concat(pcm));
if(syms.symbols.dsp_check){const a=syms.symbols.dsp_check;result.targetRegression={complete:r32(a)===0x54535749,
 hashes:Array.from({length:16},(_,i)=>r32(a+8+i*4)),mathErrors:r32(a+72)};}
if(syms.symbols.irq_check){const a=syms.symbols.irq_check;result.irqRegression={ready:r32(a)===0x4b435249,
 blocks:r32(a+4),frames:r32(a+8),done:r32(a+12),hashes:Array.from({length:24},(_,i)=>r32(a+16+i*4))};}
if(syms.symbols.bootguard&&syms.symbols.ifw_boot_progress){const b=syms.symbols.bootguard,p=syms.symbols.ifw_boot_progress;
 result.boot={firmwareCode:r32(p+4),stage:r32(p+8),failed:r32(b+4),pending:r32(b+8),
  rescue:!!r32(syms.symbols.recovery_active),entryReason:r32(syms.symbols.ifw_boot_reason)};}
if(process.env.IFW_SIM_PROFILE){const ptr=x.fm1_profile_report(),b=new Uint8Array(x.memory.buffer);
 const profile=JSON.parse(new TextDecoder().decode(b.subarray(ptr,b.indexOf(0,ptr))));
 const counts=new Map();for(const [pc,n] of profile[0].pcs||[]){const f=syms.functions.find(f=>pc>=f.address&&pc<f.address+f.bytes);
  const name=f?.name??'other';counts.set(name,(counts.get(name)||0)+n);}
 result.instructionProfile=[...counts].sort((a,b)=>b[1]-a[1]);}
if(process.env.IFW_SIM_PC_PROFILE){const ptr=x.fm1_profile_report(),b=new Uint8Array(x.memory.buffer);
 writeFileSync(process.env.IFW_SIM_PC_PROFILE,new TextDecoder().decode(b.subarray(ptr,b.indexOf(0,ptr)))+'\n');}
writeFileSync(outFile,JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({preset,all:result.all,steady:result.steady,targetRegression:result.targetRegression}));
