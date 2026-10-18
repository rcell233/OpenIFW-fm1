// SPDX-License-Identifier: GPL-3.0-only
// Actual six factory patches: original JS vs integer C, including held notes,
// repeated-note legato, note fallback, breath steps, bend/glide and long release.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {Synth} from '../vendor/openifw/dsp/core.js';
import {schedule,writeWav} from '../tools/scenario.mjs';
const dir='build/factory-regression';mkdirSync(dir,{recursive:true});
const files=['tresure-island.json','primer.json','ai-no-labyrinth.json','daisy-area-2.json','wanna-go-to-hawaii.json','judd-ish-2000.json'];
const events=[{time:.002,type:'noteOn',note:60,velocity:.78},{time:.08,type:'breath',value:.4},
 {time:.3,type:'noteOn',note:67,velocity:.9},{time:.42,type:'noteOn',note:67,velocity:.6},
 {time:.5,type:'bend',value:.35},{time:.55,type:'vibrato',value:.7},{time:.6,type:'noteOff',note:67},
 {time:.65,type:'glide',value:.8},{time:.8,type:'breath',value:1},{time:.9,type:'noteOn',note:72,velocity:.8},
 {time:1.1,type:'noteOff',note:60},{time:1.2,type:'noteOff',note:72},{time:1.45,type:'breath',value:.5}];
const suite=[];for(const [i,f]of files.entries())for(const dry of [true,false])suite.push({id:`${i}-${dry?'dry':'wet'}`,patch:JSON.parse(readFileSync('vendor/openifw/presets/'+f)),events,duration:4,sampleRate:44100,breath:.8,dry});
writeFileSync(dir+'/cases.json',JSON.stringify(suite));
let r=spawnSync('python3',['tools/encode_fixed_cases.py',dir+'/cases.json',dir],{encoding:'utf8'});assert.equal(r.status,0,r.stderr);
function data(path,T){const b=readFileSync(path);return new T(b.buffer.slice(b.byteOffset,b.byteOffset+b.length));}
const rows=[];
for(const c of suite){
 const path=dir+'/'+c.id;r=spawnSync('build/ifw-fixed',[path+'.fixed',path],{encoding:'utf8'});assert.equal(r.status,0,r.stderr);
 const pcm=data(path+'.f32',Float32Array),state=data(path+'.state.f64',Float64Array),s=new Synth(44100,c.patch);
 s.setDry(c.dry);s.setBreath(c.breath);const ev=schedule(c),l=new Float32Array(1),rr=new Float32Array(1),stages=['idle','attack','decay','sustain','release'];
 let j=0,peak=0,sum=0,power=0,eg=0,pitch=0,stageBoundaryMismatches=0,decisions=0;
 const jsPCM=new Float32Array(pcm.length);
 for(let frame=0;frame<pcm.length/2;frame++){
  while(j<ev.length&&ev[j].frame<=frame){const e=ev[j++];if(e.type==='noteOn')s.noteOn(e.note,e.velocity);else if(e.type==='noteOff')s.noteOff(e.note);else if(e.type==='breath')s.setBreath(e.value);else if(e.type==='bend')s.bendTarget=e.value;else if(e.type==='glide')s.glide=e.value;else if(e.type==='vibrato')s.vibrato=e.value;}
  s.process(l,rr,1);jsPCM[frame*2]=l[0];jsPCM[frame*2+1]=rr[0];
  for(let ch=0;ch<2;ch++){const expected=ch?rr[0]:l[0],d=pcm[frame*2+ch]-expected;assert.ok(Number.isFinite(d));peak=Math.max(peak,Math.abs(d));sum+=d*d;power+=expected**2;}
  if((frame&15)===0){const base=(frame>>4)*36;assert.equal(state[base],s.note);assert.equal(state[base+5],s.gateTarget);assert.equal(state[base+32],s.notes.length);decisions++;
   pitch=Math.max(pitch,Math.abs(state[base+1]-s.pitch));
   for(let k=0;k<4;k++){eg=Math.max(eg,Math.abs(state[base+14+k]-s.env[k].level));if(state[base+18+k]!==stages.indexOf(s.env[k].stage))stageBoundaryMismatches++;}
  }
 }
 const rmse=Math.sqrt(sum/pcm.length),row={name:c.patch.name,dry:c.dry,seconds:c.duration,rmse,peak,referenceRMS:Math.sqrt(power/pcm.length),envelopeMax:eg,pitchSemitonesMax:pitch,stageBoundaryMismatches,exactNoteGateStackChecks:decisions};
 assert.ok(pitch<.001,JSON.stringify(row));assert.ok(eg<.0001,JSON.stringify(row));
 if(c.dry){assert.ok(rmse<.0001,JSON.stringify(row));assert.ok(peak<.004,JSON.stringify(row));}
 rows.push(row);writeWav(pcm,44100,path+'-fixed.wav');writeWav(jsPCM,44100,path+'-original-js.wav');
 console.log(`${row.name} ${c.dry?'dry':'wet'}: RMSE=${rmse.toExponential(3)} EG=${eg.toExponential(3)} pitch=${pitch.toExponential(3)}`);
}
writeFileSync(dir+'/results.json',JSON.stringify({reference:'unchanged OpenIFW a478abd JavaScript',durationPerCase:4,wetIsMeasurementOnly:true,results:rows},null,2)+'\n');
