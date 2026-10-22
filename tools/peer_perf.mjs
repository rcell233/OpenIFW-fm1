// SPDX-License-Identifier: GPL-3.0-only
// OberVave context experiment, not an equivalent-engine or hardware benchmark.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const [emulator,firmware,symbolFile,outFile,voiceArg='1']=process.argv.slice(2),voices=Number(voiceArg);
if(!outFile||![1,8].includes(voices))throw Error('emulator.wasm obervave.fwsc symbols.json output.json [1|8]');
const w=readFileSync(emulator),fw=readFileSync(firmware),s=JSON.parse(readFileSync(symbolFile)).symbols;
const x=(await WebAssembly.instantiate(w,{env:{fm1_jit_compile:()=>0}})).instance.exports;
x.fm1_jit(0,0);const p=x.fm1_alloc(fw.length);new Uint8Array(x.memory.buffer,p,fw.length).set(fw);
if(x.fm1_boot_fwsc(p,fw.length,0,0,240))throw Error('boot failed');x.fm1_audit_div_cycles(48);x.fm1_audit_spi_timing(1);
const notes=['C4','D4','E4','F4','G4','A4','B4','C5'].slice(0,voices),script='500:e6:28,'+notes.map((n,i)=>`${1200+i*10}:${n}`).join(',');
const b=new TextEncoder().encode(script),q=x.fm1_alloc(b.length);new Uint8Array(x.memory.buffer,q,b.length).set(b);
if(x.fm1_bench_script(q,b.length))throw Error('script failed');
const read=a=>x.fm1_target_read32(a)>>>0,values=[];let activeMax=0;const start=performance.now();
while(x.fm1_ms()<3000){
 x.fm1_render(128,4800000);
 if(x.fm1_halted())throw Error('peer halted at '+x.fm1_ms());
 if(x.fm1_ms()>=1600){const active=Array.from({length:8},(_,i)=>read(s.ui_vlevel+i*4)).filter(v=>v>0).length;
  activeMax=Math.max(activeMax,active);if(active===voices)values.push(read(s.ui_render_us));}
 if(performance.now()-start>240000)throw Error('peer model timed out');
}
values.sort((a,b)=>a-b);const pctl=p=>values[Math.ceil(values.length*p)-1];
const sha=b=>createHash('sha256').update(b).digest('hex');
const result={kind:'different-engine-context-instruction-model-not-hardware',cycleAccurate:false,hardwareMeasured:false,
 assumptions:'240 MHz; non-divide instruction 1 cycle, divide 48 cycles; no Flash/cache/bus model; no USB host; SPI 60 MHz assumed',
 firmwareSha256:sha(fw),emulatorSha256:sha(w),script,preset:28,actualPreset:(read(s.set)>>>8)&255,requestedVoices:voices,activeMax,
 qualifiedQuanta:values.length,engineBlockUs:{p50:pctl(.5),p95:pctl(.95),max:pctl(1)},budgetUs:256/44100*1e6,emulatedMs:x.fm1_ms()};
if(activeMax!==voices||result.actualPreset!==28||values.length<50)throw Error('program/active-voice observation failed');
writeFileSync(outFile,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
