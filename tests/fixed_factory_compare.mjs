// Optional local-only measurement. Factory patch data is never copied into the distribution.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {expression} from './cases.mjs';
import {writeScenario} from '../tools/scenario.mjs';
const checkout=process.argv[2];if(!checkout)throw Error('Pass a pinned OpenIFW checkout');
const dir=resolve('build/fixed-factory');mkdirSync(dir,{recursive:true});
const files=['tresure-island.json','presets/primer.json','presets/ai-no-labyrinth.json','presets/daisy-area-2.json','presets/wanna-go-to-hawaii.json','presets/judd-ish-2000.json'];
const names=new Set(JSON.parse(readFileSync('docs/parameters.json')).map(x=>x.name));
const suite=[];for(const [i,file] of files.entries())for(const dry of [true,false]){
  const patch=JSON.parse(readFileSync(resolve(checkout,'web/assets',file)));for(const key of Object.keys(patch.parameters))if(!names.has(key)){assert.match(key,/^Osc[1-4]Waveform3$/);delete patch.parameters[key];}
  suite.push({id:`patch-${i}-${dry?'dry':'wet'}`,patch,events:expression,sampleRate:44100,duration:.5,dry,breath:.8});}
function run(cmd,args){const r=spawnSync(cmd,args,{encoding:'utf8',timeout:60000});assert.equal(r.status,0,r.stderr);}
writeFileSync(dir+'/cases.json',JSON.stringify(suite));run('python3',['tools/encode_fixed_cases.py',dir+'/cases.json',dir]);
function read(path){const b=readFileSync(path);return new Float32Array(b.buffer.slice(b.byteOffset,b.byteOffset+b.length));}
const rows=[];for(const c of suite){const prefix=dir+'/'+c.id;run('build/ifw-fixed',[prefix+'.fixed',prefix]);writeScenario(c,prefix+'.ifwtest');run('build/ifw-reference',[prefix+'.ifwtest',prefix+'-ref','256']);const a=read(prefix+'.f32'),b=read(prefix+'-ref.f32');let max=0,sum=0;for(let i=0;i<a.length;i++){const d=a[i]-b[i];assert.ok(Number.isFinite(d));max=Math.max(max,Math.abs(d));sum+=d*d;}const rmse=Math.sqrt(sum/a.length);if(c.dry)assert.ok(rmse<.0001,`${c.id} dry RMSE ${rmse}`);rows.push({id:c.id,dry:c.dry,frames:a.length/2,max,rmse});}
writeFileSync('build/fixed-factory-results.json',JSON.stringify({cases:rows.length,redistributedPresetData:false,results:rows},null,2)+'\n');console.log(JSON.stringify(rows,null,2));
