import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {writeFileSync} from 'node:fs';
import {Envelope} from '../vendor/openifw/dsp/envelope.js';
const rows=[];
for(const [a,d,s,r,retrigger,rate,frames] of [
  [0,0,0,0,0,0,44100], [300,500,700,500,0,0,88200], [500,300,350,400,1,0,88200],
  [1000,1000,1000,1000,0,0,441000], [130,680,910,410,1,350,88200],
  [100,200,500,400,0,-350,176400], [800,800,500,800,0,-1000,88200],
  [100,500,500,500,1,1000,44100],
]) {
  const p=spawnSync('build/fixed-host',[a,d,s,r,retrigger,rate,frames].map(String),{maxBuffer:4*frames+4096});
  assert.equal(p.status,0,p.stderr.toString()); const b=p.stdout;
  const levels=new Int32Array(b.buffer.slice(b.byteOffset,b.byteOffset+b.length)); assert.equal(levels.length,frames);
  const e=new Envelope(44100); e.configure(a/100,d/100,s/100,r/100,!!retrigger);
  let max=0,index=0;
  for(let i=0;i<frames;i++) {
    if(i===0) e.gate(true,false); if(i===4410) e.gate(true,true); if(i===11025) e.gate(false);
    if(i===26460) e.gate(true,false); if(i===39690) e.gate(false);
    const v=e.tick(Math.exp(.8742323893*rate/100)),error=Math.abs(v-levels[i]/2147483647);
    if(error>max) { max=error; index=i; }
  }
  assert.ok(max<4e-4,`fixed envelope CV=${a}/${d}/${s}/${r} rate=${rate}: max=${max} at ${index}`);
  rows.push({attack:a,decay:d,sustain:s,release:r,retrigger,rate,frames,maxLevelError:max});
}
writeFileSync('build/fixed-results.json',JSON.stringify({sampleRate:44100,cvResolution:.01,threshold:4e-4,cases:rows},null,2)+'\n');
console.log(`PASS ${rows.length} fixed Q31 envelope comparisons; max=${Math.max(...rows.map(r=>r.maxLevelError)).toExponential(3)}`);
