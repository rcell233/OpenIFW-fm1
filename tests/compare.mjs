import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {Synth} from '../vendor/openifw/dsp/core.js';
import {modulationDestinations} from '../vendor/openifw/dsp/modulation.js';
import {cases,dense,notes} from './cases.mjs';
import {writeScenario,schedule,writeWav} from '../tools/scenario.mjs';

const root=resolve(fileURLToPath(new URL('..',import.meta.url)));
process.chdir(root);
const dir=resolve('build/comparison'); mkdirSync(dir,{recursive:true});
const binary=resolve(process.env.IFW_REFERENCE || 'build/ifw-reference');
const stages=['idle','attack','decay','sustain','release'];
const traceNames=['note','pitch','pitchTarget','velocity','gate','gateTarget','breath','breathTarget','bend','glide',
  'glideSmoothed','glideElapsed','glideFrom','noteTime',...Array.from({length:4},(_,i)=>`eg${i+1}.level`),
  ...Array.from({length:4},(_,i)=>`eg${i+1}.stage`),'lfo1.phase','lfo2.phase',...Array.from({length:4},(_,i)=>`osc${i+1}.phase`),
  'masterPhase','seed','noiseHeld','noiseCounter','heldCount','breathSamplesLeft','blockCount','time'];
const snapshot=s=>[s.note,s.pitch,s.pitchTarget,s.velocity,s.gate,s.gateTarget,s.breath,s.breathTarget,s.bend,s.glide,
  s.glideSmoothed,s.glideElapsed,s.glideFrom,s.noteTime,...s.env.map(e=>e.level),...s.env.map(e=>stages.indexOf(e.stage)),
  ...s.lfoPhase,...s.phase,s.masterPhase,s.seed>>>0,s.noiseHeld,s.noiseCounter,s.notes.length,s.breathSamplesLeft,s.blockCount,s.time];
function dispatch(s,e) {
  if(e.type==='noteOn') s.noteOn(e.note,e.velocity??1);
  else if(e.type==='noteOff') s.noteOff(e.note);
  else if(e.type==='breath') s.setBreath(e.value);
  else if(e.type==='bend') s.bendTarget=e.value;
  else if(e.type==='glide') s.glide=e.value;
  else if(e.type==='vibrato') s.vibrato=e.value;
  else if(e.type==='panic') s.panic();
  else if(e.type==='dry') s.setDry(e.value);
  else if(e.type==='edit') { const p=structuredClone(s.patch); p.parameters[e.key]=e.value; s.setPatch(p); }
  else throw new Error(e.type);
}
function reference(c,total) {
  const s=new Synth(c.sampleRate,c.patch),pcm=new Float32Array(total*2),state=[];
  s.setDry(c.dry); s.setBreath(c.breath);
  const events=schedule(c),l=new Float32Array(1),r=new Float32Array(1); let j=0;
  for(let frame=0;frame<total;frame++) {
    while(j<events.length && events[j].frame<=frame) dispatch(s,events[j++]);
    s.process(l,r,1); pcm[frame*2]=l[0]; pcm[frame*2+1]=r[0];
    if((frame&15)===0) state.push(...snapshot(s));
  }
  return {pcm,state:new Float64Array(state)};
}
function run(args) {
  const p=spawnSync(binary,args,{encoding:'utf8',timeout:60000,maxBuffer:2**20});
  assert.equal(p.status,0,`${binary} ${args.join(' ')}\n${p.stderr}\n${p.error||''}`);
}
function typed(path,Type) { const b=readFileSync(path); return new Type(b.buffer.slice(b.byteOffset,b.byteOffset+b.length)); }
function rms(data) { return Math.sqrt(data.reduce((v,x)=>v+x*x,0)/data.length); }
function diff(a,b) {
  assert.equal(a.length,b.length); let max=0,sum=0,index=-1;
  for(let i=0;i<a.length;i++) { assert.ok(Number.isFinite(a[i])&&Number.isFinite(b[i]),`nonfinite at ${i}`);
    const d=Math.abs(a[i]-b[i]); if(d>max) { max=d; index=i; } sum+=d*d; }
  return {max,rmse:Math.sqrt(sum/a.length),index};
}
const suite=cases();
// Cover every upstream destination, including labels that intentionally do nothing in JS.
for(const dest of modulationDestinations.filter(x=>x!=='None')) {
  const p=dense(); Object.assign(p.parameters,{Mod1Source1:'LFO 1 (+-)',Mod1Source2:'BREATH',Mod1Destination:dest,Mod1Depth:1.3});
  suite.push({id:`dest-${suite.length}`,patch:p,events:notes,duration:.065,sampleRate:44100,dry:true,breath:.7,cover:dest});
}
const sources=['None','BREATH','Breath (+-)','VELOCITY','Velocity (+-)','NOTE','Note (+-)','PITCH BEND','VIBRATO','GLIDE','GATE',
  ...Array.from({length:4},(_,i)=>`EG ${i+1}`),...Array.from({length:2},(_,i)=>`LFO ${i+1} (+-)`),
  ...Array.from({length:4},(_,i)=>['SAW','TRI','PWM','NOISE'].map(w=>`OSC ${i+1} ${w} (+-)`)).flat()];
for(const src of sources) {
  const p=dense(); Object.assign(p.parameters,{Mod1Source1:src,Mod1Source2:'None',Mod1Destination:'Filter 1 Frequency',Mod1Depth:1.1});
  suite.push({id:`source-${suite.length}`,patch:p,events:notes,duration:.045,sampleRate:44100,dry:true,breath:.7,cover:src});
}
for(const src of sources) {
  const p=dense(); Object.assign(p.parameters,{Mod1Source1:src,Mod1Source2:'None',Mod1Destination:'OSC 3 FM',Mod1Depth:.3});
  suite.push({id:`fm-source-${suite.length}`,patch:p,events:notes,duration:.045,sampleRate:44100,dry:true,breath:.7,cover:`FM: ${src}`});
}
const results=[],audio=new Map(); let frames=0;
for(const c of suite) {
  const prefix=resolve(dir,c.id),scenario=prefix+'.ifwtest'; const total=writeScenario(c,scenario);
  const js=reference(c,total); run([scenario,prefix,'1','trace']);
  const pcm=typed(prefix+'.f32',Float32Array),state=typed(prefix+'.state.f64',Float64Array);
  const ad=diff(pcm,js.pcm),sd=diff(state,js.state);
  assert.ok(ad.max<2e-6 && ad.rmse<1e-7,`${c.id}: PCM ${JSON.stringify(ad)}`);
  assert.ok(sd.max<2e-8,`${c.id}: state ${traceNames[sd.index%36]} at frame ${Math.floor(sd.index/36)*16}: ${state[sd.index]} != ${js.state[sd.index]}`);
  run([scenario,prefix+'-block256','256']);
  assert.deepEqual(typed(prefix+'-block256.f32',Float32Array),pcm,`${c.id}: callback partition changed audio`);
  const row={id:c.id,cover:c.cover||null,sampleRate:c.sampleRate,frames:total,pcmMaxError:ad.max,pcmRMSE:ad.rmse,stateMaxError:sd.max,
    rms:rms(pcm),sha256:createHash('sha256').update(new Uint8Array(pcm.buffer)).digest('hex')};
  results.push(row); frames+=total;
  if(['sine-440','dense-parallel2','silent-osc-fm','silent-osc-no-fm','sync-clock-fm','sync-clock-no-fm','br-eg-release','fx-full-chain','noise-seeded'].includes(c.id)) audio.set(c.id,pcm);
  if(['sine-440','dense-parallel2','fx-full-chain'].includes(c.id)) {
    writeWav(pcm,c.sampleRate,prefix+'-c.wav'); writeWav(js.pcm,c.sampleRate,prefix+'-js.wav');
  }
  if(c.id==='br-eg-release') {
    const i=Math.floor(.21*c.sampleRate/16)*36;
    assert.ok(state[i+4]<1e-8 && state[i+14]>0,'BR×EG release must survive gate close');
  }
  if(!c.cover) console.log(`PASS ${c.id}: PCM max=${ad.max.toExponential(2)}, state max=${sd.max.toExponential(2)}`);
}
const delta=(a,b)=>a.map((x,i)=>x-b[i]);
assert.ok(rms(delta(audio.get('silent-osc-fm'),audio.get('silent-osc-no-fm')))>1e-3,'silent oscillator must remain an FM source');
assert.deepEqual(audio.get('sync-clock-fm'),audio.get('sync-clock-no-fm'),'FM must not change the following oscillator sync clock');
// Estimate pitch independently of sample-by-sample comparison.
const a=audio.get('sine-440'),crossings=[];
for(let i=3000;i<9500;i++) if(a[2*i-2]<=0 && a[2*i]>0) crossings.push(i-a[2*i]/(a[2*i]-a[2*i-2]));
const measured=(crossings.length-1)*44100/(crossings.at(-1)-crossings[0]);
assert.ok(Math.abs(measured-440)<.02,`sine pitch ${measured}`);
const noiseCase=suite.find(c=>c.id==='noise-seeded'),nframes=Math.ceil(noiseCase.duration*noiseCase.sampleRate);
assert.deepEqual(reference(noiseCase,nframes).pcm,audio.get('noise-seeded'),'noise must repeat from the fixed seed');
const summary={phase:'desktop-reference',cases:results.length,sampleFrames:frames,pcmSamples:frames*2,
  thresholds:{pcmMax:2e-6,pcmRMSE:1e-7,stateMax:2e-8},
  worstPCM:Math.max(...results.map(r=>r.pcmMaxError)),worstState:Math.max(...results.map(r=>r.stateMaxError)),
  measuredA4Hz:measured,blockPartitionInvariant:true,results};
writeFileSync('build/test-results.json',JSON.stringify(summary,null,2)+'\n');
console.log(`PASS ${results.length} comparisons; ${frames} frames; A4=${measured.toFixed(6)} Hz; worst PCM=${summary.worstPCM}`);
