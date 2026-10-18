import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {cases,dense,notes} from './cases.mjs';
import {modulationDestinations} from '../vendor/openifw/dsp/modulation.js';
import {writeScenario} from '../tools/scenario.mjs';
const dir=resolve('build/fixed-engine');mkdirSync(dir,{recursive:true});
function run(cmd,args){const r=spawnSync(cmd,args,{encoding:'utf8',timeout:60000,maxBuffer:1e6});assert.equal(r.status,0,`${cmd}: ${r.stderr}\n${r.error||''}`);}
const suite=cases().filter(c=>c.sampleRate===44100);
for(const dest of modulationDestinations.filter(x=>x!=='None')) {
  const p=dense();Object.assign(p.parameters,{Mod1Source1:'LFO 1 (+-)',Mod1Source2:'BREATH',Mod1Destination:dest,Mod1Depth:1.3});
  suite.push({id:`dest-${suite.length+2}`,patch:p,events:notes,duration:.065,sampleRate:44100,dry:true,breath:.7,cover:dest});
}
const sources=['None','BREATH','Breath (+-)','VELOCITY','Velocity (+-)','NOTE','Note (+-)','PITCH BEND','VIBRATO','GLIDE','GATE',
  ...Array.from({length:4},(_,i)=>`EG ${i+1}`),...Array.from({length:2},(_,i)=>`LFO ${i+1} (+-)`),
  ...Array.from({length:4},(_,i)=>['SAW','TRI','PWM','NOISE'].map(w=>`OSC ${i+1} ${w} (+-)`)).flat()];
for(const fm of [false,true])for(const src of sources){const p=dense();Object.assign(p.parameters,{Mod1Source1:src,Mod1Source2:'None',Mod1Destination:fm?'OSC 3 FM':'Filter 1 Frequency',Mod1Depth:fm?.3:1.1});suite.push({id:`${fm?'fm':'cv'}-source-${suite.length}`,patch:p,events:notes,duration:.045,sampleRate:44100,dry:true,breath:.7,cover:src});}
writeFileSync(dir+'/cases.json',JSON.stringify(suite));run('python3',['tools/encode_fixed_cases.py',dir+'/cases.json',dir]);
function typed(path,T){const b=readFileSync(path);return new T(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength));}
let results=[];
for(const c of suite) {
  const out=dir+'/'+c.id;run(process.env.IFW_FIXED||'build/ifw-fixed',[out+'.fixed',out]);
  writeScenario(c,out+'.ifwtest');run('build/ifw-reference',[out+'.ifwtest',out+'-ref','1','trace']);
  const fixed=typed(out+'.f32',Float32Array),ref=typed(out+'-ref.f32',Float32Array);
  assert.equal(fixed.length,ref.length);let max=0,sum=0,power=0,peak=0;
  for(let i=0;i<ref.length;i++){assert.ok(Number.isFinite(fixed[i]));const d=fixed[i]-ref[i];max=Math.max(max,Math.abs(d));sum+=d*d;power+=ref[i]**2;peak=Math.max(peak,Math.abs(fixed[i]));}
  const a=typed(out+'.state.f64',Float64Array),b=typed(out+'-ref.state.f64',Float64Array),errors=Array(36).fill(0);
  for(let i=0;i<a.length;i++){let d=Math.abs(a[i]-b[i]);if([22,23,24,25,26,27,28].includes(i%36))d=Math.min(d,Math.abs(1-d));errors[i%36]=Math.max(errors[i%36],d);}
  // Exact musical event semantics, independent of audio approximation.
  for(const i of [0,2,5,29,32,33,34])assert.equal(errors[i],0,`${c.id}: state[${i}]`);
  assert.ok(errors[1]<.001,`${c.id}: glide pitch drift`);
  assert.ok(Math.max(...errors.slice(14,18))<.0001,`${c.id}: envelope level drift`);
  assert.ok(Math.sqrt(sum/ref.length)<(c.dry?.0001:.001),`${c.id}: audio RMSE`);
  assert.ok(max<(c.dry?.004:.01),`${c.id}: peak audio difference`);
  results.push({id:c.id,cover:c.cover,max,rmse:Math.sqrt(sum/ref.length),relativeRMS:Math.sqrt(sum/(power||1)),peak,errors});
  console.log(`${c.id}: rmse=${results.at(-1).rmse.toExponential(2)}, max=${max.toExponential(2)}, pitch=${errors[1].toExponential(2)}, EG=${Math.max(...errors.slice(14,18)).toExponential(2)}`);
}
writeFileSync('build/fixed-engine-results.json',JSON.stringify({cases:results.length,sampleRate:44100,thresholds:{pitchSemitones:.001,envelopeLevel:.0001,dryRMSE:.0001,wetRMSE:.001,dryPeak:.004,wetPeak:.01},results},null,2)+'\n');
console.log(`PASS ${results.length} fixed-engine comparisons`);
