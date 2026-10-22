// SPDX-License-Identifier: GPL-3.0-only
// Read-only firmware file; patch ablations go through its RAM AudioEvent queue.
// All timing is from the uncalibrated CPU instruction model, never hardware.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const [emuFile,fwFile,symFile,outFile,presetText='0',probe='baseline',note='A4']=process.argv.slice(2);
if(!outFile)throw Error('emulator.wasm firmware.fwsc symbols.json out.json preset probe [note]');
const preset=Number(presetText),duration=3250,measureFrom=1750;
const modifications={baseline:[],no_reverb:[[228,16]],no_chorus:[[228,2]],no_delay:[[228,12]],
 no_post_fx:[[228,31]],no_eq:[[154,0],[155,0]],no_filter3:[[128,0]],
 filter3_8:[[125,8*65536]],no_fm:[[95,0]],no_sync:[[0,0],[12,0],[24,0],[36,0]],
 no_exciter_gain:[[150,0]],no_nonfm_mod:[[95,0],[99,0],[103,0]],
 no_wave_outputs:Array.from({length:4},(_,i)=>[3,4,5,7].map(k=>[i*12+k,0])).flat(),
 no_wet_fx:[[228,31],[150,0],[154,0],[155,0]]};
if(!modifications[probe])throw Error('Unknown probe: '+probe);
const syms=JSON.parse(readFileSync(symFile)),symbols=syms.symbols;
for(const k of ['audio_perf','synth','queue','qwrite','qread','preset'])if(!symbols[k])throw Error('Missing symbol '+k);
const wasm=readFileSync(emuFile),fw=readFileSync(fwFile),sha=b=>createHash('sha256').update(b).digest('hex');
const x=(await WebAssembly.instantiate(wasm,{env:{fm1_jit_compile:()=>0}})).instance.exports;
x.fm1_jit(0,0);
const p=x.fm1_alloc(fw.length);new Uint8Array(x.memory.buffer,p,fw.length).set(fw);
if(x.fm1_boot_fwsc(p,fw.length,0,0,240))throw Error('Firmware boot failed');
x.fm1_audit_div_cycles(48);x.fm1_audit_spi_timing(1);
const eventNote=/^\d+$/.test(note)?Number(note):null;
if(eventNote!==null&&(eventNote<0||eventNote>127))throw Error('Bad note number');
if(eventNote===null&&!/^(F3|F#3|G3|G#3|A3|A#3|B3|[A-G]#?4|C5|C#5|D5|D#5|E5|F5|F#5|G5)$/.test(note))
 throw Error('Panel keys cover F3..G5; use an AudioEvent note number for other pitches');
const script=`${preset?`500:e6:${preset}`:''}${eventNote===null?`${preset?',':''}1000:${note}`:''}`;
const b=new TextEncoder().encode(script),sp=x.fm1_alloc(b.length);new Uint8Array(x.memory.buffer,sp,b.length).set(b);
if(x.fm1_bench_script(sp,b.length))throw Error('Invalid script');
const r=a=>x.fm1_target_read32(a)>>>0,w=(a,v)=>x.fm1_target_write32(a,v>>>0);
const stat=()=>({blocks:r(symbols.audio_perf+8),dspUs:r(symbols.audio_perf+28)/24,
 peak:r(symbols.audio_perf+36),clips:r(symbols.audio_perf+40),underruns:r(symbols.audio_underruns)});
let injected=false,started=false,noteInjectedAt=null,startStats,stopStats,startMs,lastBlock=0,originalWords;
const dsp=[];let samples=0;const startedAt=performance.now();
while(x.fm1_ms()<duration){
 const got=x.fm1_render(128,4800000);if(started&&got!==128)throw Error('Short render in measurement window');
 if(x.fm1_halted())throw Error('CPU halted at '+x.fm1_ms());
 if(!injected&&x.fm1_ms()>=750){
  if(r(symbols.preset)!==preset)throw Error('Wrong preset');
  if(r(symbols.qwrite)!==r(symbols.qread))continue;
  originalWords=modifications[probe].map(([id])=>[id,r(symbols.synth+id*4)|0]);
  const at=r(symbols.qwrite);
  for(const [i,[id,value]] of modifications[probe].entries()){
   if(id<0||id>=229)throw Error('Bad patch word');
   const a=symbols.queue+((at+i)&127)*12;w(a,3);w(a+4,id);w(a+8,value);
  }
  w(symbols.qwrite,at+modifications[probe].length);injected=true;
 }
 if(eventNote!==null&&noteInjectedAt===null&&x.fm1_ms()>=1000&&r(symbols.qwrite)===r(symbols.qread)){
  const at=r(symbols.qwrite);
  for(const [i,e] of [[2,2147483647,0],[0,eventNote,2147483647]].entries()){
   const a=symbols.queue+((at+i)&127)*12;for(let j=0;j<3;j++)w(a+j*4,e[j]);
  }
  w(symbols.qwrite,at+2);noteInjectedAt=x.fm1_ms();
 }
 if(!started&&x.fm1_ms()>=measureFrom){
  if(!injected)throw Error('Probe not applied');
  for(const [id,value] of modifications[probe])if((r(symbols.synth+id*4)|0)!==value)throw Error('Patch event not consumed: '+id);
  startStats=stat();startMs=x.fm1_ms();lastBlock=startStats.blocks;x.fm1_profile(1);started=true;
 }
 if(started){const s=stat();if(s.blocks!==lastBlock){lastBlock=s.blocks;dsp.push(s.dspUs);}samples+=got;}
 if(performance.now()-startedAt>300000)throw Error('Probe timed out');
}
x.fm1_profile(0);stopStats=stat();
if(!stopStats.peak&&probe!=='no_wave_outputs')throw Error('No audio: the note was not exercised');
const monoCount=r(symbols.synth+916+128);
if(monoCount!==1)throw Error('Expected exactly one held note, got '+monoCount);
const currentNote=r(symbols.synth+916)&255;
const expectedNote=eventNote??(12*(Number(note.at(-1))+1)+({C:0,D:2,E:4,F:5,G:7,A:9,B:11}[note[0]])+(note.includes('#')?1:0));
if(currentNote!==expectedNote)throw Error(`Expected note ${expectedNote}, got ${currentNote}`);
const ptr=x.fm1_profile_report(),mem=new Uint8Array(x.memory.buffer);
const raw=JSON.parse(new TextDecoder().decode(mem.subarray(ptr,mem.indexOf(0,ptr))));
const funcs=[...syms.functions].sort((a,b)=>a.address-b.address),counts=new Map();
for(const [pc,n] of raw[0].pcs){
 let lo=0,hi=funcs.length;while(lo<hi){const m=(lo+hi)>>>1;if(funcs[m].address<=pc)lo=m+1;else hi=m;}
 const f=funcs[lo-1],name=f&&pc<f.address+f.bytes?f.name:'other';counts.set(name,(counts.get(name)||0)+n);
}
const profile=[...counts].sort((a,b)=>b[1]-a[1]);
const dspFunctions=profile.filter(([k])=>/^(ifw_|filter_tick|source20|update_controls|configure_|amp_gain|oscillator_sample|filter3_tick)/.test(k));
const sum=rows=>rows.reduce((n,[,v])=>n+v,0),s=[...dsp].sort((a,b)=>a-b),pcnt=p=>s[Math.ceil(p*s.length)-1];
const result={kind:'pi32v2-instruction-model-not-hardware',hardwareMeasured:false,cycleAccurate:false,
 assumptions:'240 MHz; non-divide instructions 1 cycle; divides 48 cycles; SPI peripheral 60 MHz; no cache/Flash/bus/USB-host model',
 firmwareSha256:sha(fw),emulatorSha256:sha(wasm),preset,actualPreset:r(symbols.preset),probe,note,script,
 input:eventNote===null?'panel-key':'AudioEvent-queue',noteInjectedAt,currentNote,monoCount,
 patchWords:modifications[probe],originalWords,startMs,endMs:x.fm1_ms(),blockFrames:256,blockBudgetUs:256/44100*1e6,
 dsp:{blocks:dsp.length,meanUs:dsp.reduce((a,b)=>a+b,0)/dsp.length,p50Us:pcnt(.5),p95Us:pcnt(.95),maxUs:s.at(-1)},
 observation:{clips:stopStats.clips-startStats.clips,peak:stopStats.peak,underruns:stopStats.underruns-startStats.underruns,
 completedBlocks:stopStats.blocks-startStats.blocks},profile,dspInstructions:sum(dspFunctions),dspFunctions,
 wallSeconds:(performance.now()-startedAt)/1000};
writeFileSync(outFile,JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({preset,probe,note,dsp:result.dsp,observation:result.observation,dspInstructions:result.dspInstructions,wallSeconds:result.wallSeconds}));
