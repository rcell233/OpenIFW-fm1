// A self-authored legato phrase rendered by the compiled C engine and original JS.
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {dense} from '../tests/cases.mjs';
import {renderSequence} from '../vendor/openifw/dsp/core.js';
import {writeScenario,writeWav} from './scenario.mjs';
const p=dense();
Object.assign(p.parameters,{MasterLevel:10,Filter3Enabled:'Off',Filter1Frequency:5.7,Filter2Frequency:6.5,
  GlideMidiEnabled:'Off',GlideMode:'Time',GlideSpeed:3.4,GlideMinimumSpeed:0,
  Osc1SawLevel:5,Osc1TriLevel:3,Osc2SawLevel:3,Osc2TriLevel:2,Osc4NoiseLevel:.15,
  Lfo1Waveform:'TRI',Lfo1Speed:7.6,Mod1Source1:'LFO 1 (+-)',Mod1Source2:'BREATH',Mod1Destination:'OSC 1 Pitch',Mod1Depth:.08});
p.fx=[
  {name:'IFW Phaser',parameters:{Bypass:'On'}},
  {name:'IFW Chorus',parameters:{Bypass:'Off',Rate:.27,Depth:6,Mix:4}},
  {name:'IFW Delay1',parameters:{Bypass:'On'}},
  {name:'IFW Delay2',parameters:{Bypass:'On'}},
  {name:'IFW Reverb',parameters:{Bypass:'Off',Size:4,HighDamp:3,Width:7,WetDry:1.5}},
];
const events=[
  {time:.05,type:'noteOn',note:60,velocity:.8},
  {time:.42,type:'noteOn',note:64,velocity:.9},
  {time:.5,type:'noteOff',note:60},
  {time:.78,type:'noteOn',note:67,velocity:.85},
  {time:.85,type:'noteOff',note:64},
  {time:1.13,type:'noteOn',note:72,velocity:.95},
  {time:1.5,type:'noteOff',note:72}, // fall back to the held G
  {time:1.92,type:'noteOff',note:67},
  {time:.35,type:'breath',value:.9},{time:1.3,type:'breath',value:1},{time:1.8,type:'breath',value:.65},
];
const c={patch:p,events,sampleRate:44100,duration:2.9,dry:false,breath:.7};
mkdirSync('build/demo',{recursive:true}); writeScenario(c,'build/demo/legato.ifwtest');
const child=spawnSync('build/ifw-reference',['build/demo/legato.ifwtest','build/demo/legato-c','256'],{encoding:'utf8'});
assert.equal(child.status,0,child.stderr);
const bytes=readFileSync('build/demo/legato-c.f32');
const pcm=new Float32Array(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.length));
const js=renderSequence(p,events,{sampleRate:c.sampleRate,duration:c.duration,dry:c.dry,breath:c.breath});
const oracle=new Float32Array(pcm.length); let max=0,peak=0;
for(let i=0;i<js.left.length;i++) { oracle[2*i]=js.left[i]; oracle[2*i+1]=js.right[i]; }
for(let i=0;i<pcm.length;i++) { max=Math.max(max,Math.abs(pcm[i]-oracle[i])); peak=Math.max(peak,Math.abs(pcm[i])); }
assert.ok(max<2e-6 && peak>.01 && peak<1,`demo max=${max}, peak=${peak}`);
writeWav(pcm,44100,'build/demo/legato-c.wav'); writeWav(oracle,44100,'build/demo/legato-js.wav');
writeFileSync('build/demo/scenario.json',JSON.stringify(c,null,2)+'\n');
writeFileSync('build/demo/result.json',JSON.stringify({sampleRate:44100,frames:pcm.length/2,peak,pcmMaxError:max},null,2)+'\n');
console.log(`PASS legato demo: ${pcm.length/2} frames, peak=${peak.toFixed(4)}, JS/C max=${max}`);
