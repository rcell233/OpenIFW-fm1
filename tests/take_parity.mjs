// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync as read,writeFileSync as write,mkdirSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const [json,mid,upstream,bin='build/take-parity',dir='build/parity']=process.argv.slice(2);
if(!upstream)throw Error('usage: node tests/take_parity.mjs recording.json recording.mid upstream-checkout [runner] [out-dir]');
const imp=f=>import(pathToFileURL(path.resolve(upstream,'web/src',f)));
const {Synth}=await imp('dsp/core.js'),{decodeMidi}=await imp('midi.js');
const rec=JSON.parse(read(json)),midi=read(mid),events=rec.events;
assert.equal(rec.start.patchHash,1355846903,'record a stock Tresure Island preset');
assert.equal(rec.start.lastBreathRaw,0);assert.equal(rec.start.heldNotes,0);
assert.equal(rec.start.glideQ31,0);assert.equal(rec.start.bendQ31,0);
assert.equal(rec.start.breathCC,2);assert.deepEqual(rec.start.breathCurve,[0,33,66,100]);
assert.ok(events.every(e=>e.source==='TRS' && e.acceptedChannel));
assert.equal(midi.toString('ascii',0,4),'MThd');assert.equal(midi.readUInt16BE(12),1000);
assert.equal(midi.toString('ascii',14,18),'MTrk');assert.equal(midi.readUInt32BE(18),midi.length-22);
let index=22,time=0,p=0,tempo=0;
function vlq(){let v=0,b;do{b=midi[index++];assert.ok(b!==undefined);v=(v<<7)|(b&127);}while(b&128);return v;}
while(index<midi.length){
 time+=vlq();const st=midi[index++];
 if(st===255){const t=midi[index++],n=vlq();if(t===81)tempo=midi.readUIntBE(index,n);index+=n;continue;}
 assert.ok(st>=0x80&&st<0xf0);
 const length=(st&224)===192?1:2,e=events[p++];
 assert.deepEqual([st,...midi.subarray(index,index+length)],e.bytes);index+=length;
 assert.ok(Math.abs(time*500-e.us)<=250);
}
assert.equal(p,events.length);assert.equal(tempo,500000);
const cfg={breathCC:2,glideCC:-1,vibratoCC:9,channel:0};
const patch=JSON.parse(read(path.join(upstream,'web/assets/tresure-island.json')));
const duration=rec.durationMs/1000+0.75;
mkdirSync(dir,{recursive:true});
const cases=[
 {id:'dry-rc17',fx:0,step:256},
 {id:'dry-32',fx:0,step:32},
 {id:'no-reverb',fx:1,step:32},
 {id:'wet',fx:2,step:32},
 {id:'upstream-default',fx:0,step:32,config:undefined,raw:true},
 {id:'upstream-cc2-with-cc5',fx:0,step:32,config:{...cfg,glideCC:5},raw:true},
 {id:'upstream-matched-live-48k',fx:0,step:32,config:cfg,raw:true,fs:48000},
];
const l=new Float32Array(1),r=new Float32Array(1);
for(const {id,fx,step,config,raw,fs=44100} of cases){
 const frames=Math.ceil(duration*fs);
 const syn=new Synth(fs,patch);syn.setDry(!fx);
 if(fx===1)syn.setPatch({...patch,settings:{...patch.settings,FxBypassReverb:'1'}});
 const times=events.map(e=>Math.round(e.us*fs/1e6));
 const fw=path.join(dir,id+'-fw.i32');
 let fws;
 if(!raw){
  execFileSync(bin,[String(frames),String(step),String(fx),fw],{input:events.map((e,i)=>[times[i],...e.bytes].join(' ')).join('\n')+'\n'});
  const d=read(fw);fws=new Int32Array(d.buffer,d.byteOffset,d.length/4);
  assert.equal(fws.length,frames*12);
 }
 const pcm=new Float32Array(frames*2),state=new Float32Array(frames*8);
 let j=0,pitch=0,bend=0,env=0,gate=0,breath=0,stack=0;
 for(let f=0;f<frames;f++){
  if(raw || f%step===0)while(j<events.length&&times[j]<=f){
   const e=decodeMidi(events[j++].bytes,raw?config:cfg);if(!e)continue;
   if(e.type==='noteOn')syn.noteOn(e.note,e.velocity);
   else if(e.type==='noteOff')syn.noteOff(e.note);
   else if(e.type==='breath')syn.setBreath(e.value);
   else if(e.type==='bend')syn.bendTarget=e.value;
   else if(e.type==='vibrato')syn.vibrato=e.value;
   else if(e.type==='glide')syn.glide=e.value;
  }
  syn.process(l,r,1);pcm[2*f]=l[0];pcm[2*f+1]=r[0];
  state.set([syn.pitch,69+12*Math.log2(syn.frequency/440),syn.gate,syn.breath,...syn.env.map(e=>e.level)],f*8);
  if(fws){
   const x=f*12;
   pitch=Math.max(pitch,Math.abs(fws[x+2]/65536-syn.pitch));
   bend=Math.max(bend,Math.abs(fws[x+3]/65536-state[f*8+1]));
   gate=Math.max(gate,Math.abs(fws[x+4]/2147483648-syn.gate));
   breath=Math.max(breath,Math.abs(fws[x+5]/2147483648-syn.breath));
   stack+=+(fws[x+10]!==syn.notes.length || fws[x+11]!==syn.gateTarget);
   for(let k=0;k<4;k++)env=Math.max(env,Math.abs(fws[x+6+k]/2147483648-syn.env[k].level));
  }
 }
 assert.equal(j,events.length);
 if(fws){
  assert.equal(stack,0);
  assert.ok(pitch<0.001 && bend<0.001 && env<0.0001 && gate<0.0001 && breath<0.0001,'matched control trajectories');
  if(fx===0){
   let err=0,power=0;
   for(let i=0;i<frames;i++)for(let ch=0;ch<2;ch++){
    const u=pcm[i*2+ch],v=fws[i*12+ch]/2**20;
    err+=(u-v)**2;power+=u*u;
   }
   assert.ok(power>0 && err/power<0.001,'dry PCM regression: error must be below −30 dB relative RMS');
  }
 }
 write(path.join(dir,id+'-up.f32'),Buffer.from(pcm.buffer));
 console.log(id,{events:j,pitch,bend,gate,breath,env,stack});
}
