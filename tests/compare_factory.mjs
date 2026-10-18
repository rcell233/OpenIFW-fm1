// Optional validation against an existing pinned OpenIFW checkout. No preset redistribution.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {renderSequence} from '../vendor/openifw/dsp/core.js';
import {expression} from './cases.mjs';
import {writeScenario} from '../tools/scenario.mjs';
const checkout=process.argv[2];
if(!checkout) throw new Error('usage: node tests/compare_factory.mjs /path/to/OpenIFW');
const schemas=JSON.parse(readFileSync('docs/parameters.json'));
const names=new Set(schemas.map(p=>p.name));
const files=['tresure-island.json','presets/primer.json','presets/ai-no-labyrinth.json',
  'presets/daisy-area-2.json','presets/wanna-go-to-hawaii.json','presets/judd-ish-2000.json'];
const rows=[]; mkdirSync('build/factory-comparison',{recursive:true});
for(const file of files) for(const dry of [true,false]) {
  const p=JSON.parse(readFileSync(resolve(checkout,'web/assets',file)));
  const copied=structuredClone(p),ignored=[];
  for(const key of Object.keys(copied.parameters)) if(!names.has(key)) {
    assert.match(key,/^Osc[1-4]Waveform3$/,'unhandled upstream parameter');
    ignored.push(key); delete copied.parameters[key];
  }
  const c={patch:copied,events:expression,sampleRate:44100,duration:.32,dry,breath:.8};
  const prefix=resolve('build/factory-comparison',file.replaceAll('/','-').replace('.json','')+(dry?'-dry':'-full'));
  writeScenario(c,prefix+'.ifwtest');
  const child=spawnSync('build/ifw-reference',[prefix+'.ifwtest',prefix,'256'],{encoding:'utf8'});
  assert.equal(child.status,0,child.stderr);
  const b=readFileSync(prefix+'.f32'),actual=new Float32Array(b.buffer.slice(b.byteOffset,b.byteOffset+b.length));
  const expected=renderSequence(p,expression,{sampleRate:44100,duration:.32,dry,breath:.8});
  let max=0,sum=0;
  for(let i=0;i<expected.left.length;i++) for(let ch=0;ch<2;ch++) {
    const d=Math.abs(actual[2*i+ch]-(ch?expected.right[i]:expected.left[i])); assert.ok(Number.isFinite(d));
    max=Math.max(max,d); sum+=d*d;
  }
  const rmse=Math.sqrt(sum/actual.length); assert.ok(max<2e-6 && rmse<1e-7,`${file} dry=${dry}: max=${max}`);
  rows.push({file,dry,frames:actual.length/2,pcmMaxError:max,pcmRMSE:rmse,ignoredUnusedKeys:ignored});
}
writeFileSync('build/factory-results.json',JSON.stringify({cases:rows.length,results:rows},null,2)+'\n');
console.log(`PASS ${rows.length} factory comparisons against original JS; max=${Math.max(...rows.map(r=>r.pcmMaxError))}`);
