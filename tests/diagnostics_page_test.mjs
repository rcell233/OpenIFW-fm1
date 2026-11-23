// SPDX-License-Identifier: GPL-3.0-only
// Exercise the actual page and protocol with a simulated MIDI device.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {readSnapshot,fields,readBootSnapshot,bootFields} from '../web/diagnostics/protocol.js';
import {readTrace,midiFile} from '../web/diagnostics/trace.js';
const elements=new Map(),listeners=new Set();let permissions=0,sent=0,report,readingTrace=false;
const blobs=[];
const element=()=>({value:'',hidden:true,disabled:false,textContent:'',children:[],
 append(v){this.children.push(v);if(!this.value&&v.value)this.value=v.value;},replaceChildren(){this.children=[];}});
const el=id=>{if(!elements.has(id))elements.set(id,element());return elements.get(id);};
const input={id:'in',name:'Felucca MIDI 1',async open(){},addEventListener(_k,f){listeners.add(f);},removeEventListener(_k,f){listeners.delete(f);}};
const output={id:'out',name:'Felucca MIDI 1',async open(){},send(request){
 if(readingTrace && request[6]===4){
  const words=[1,123,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,40716,0,0,0,0,0];
  const b=[240,125,73,70,87,1,5,request[7],words.length];
  for(const w of words)for(let n=0;n<5;n++)b.push((w>>>(n*7))&127);
  b.push(247);queueMicrotask(()=>{for(const f of listeners)f({data:b});});return;
 }
 if(request[6]===2){
  if(!readingTrace)assert.deepEqual(Array.from(request),[240,125,73,70,87,1,2,0,247]);
  const values={schema:1,firmwareCode:readingTrace?40716:40702,rescue:readingTrace?0:1,entryReason:1,p3Reset:4,currentStage:3,previousStage:11,previousFirmwareCode:40702};
  const response=[240,125,73,70,87,1,3,0,bootFields.length];
  for(const name of bootFields){const v=values[name]||0;for(let k=0;k<5;k++)response.push((v/2**(7*k))&127);}
  response.push(247);queueMicrotask(()=>{for(const f of listeners)f({data:response});});return;
 }
 const sequence=sent++;assert.deepEqual(Array.from(request),[240,125,73,70,87,1,0,sequence,247]);
 const values={schema:1,firmwareCode:40701,sampleRate:44100,blockFrames:256,
  blocks:sequence<5?100+sequence:sequence-5,xruns:sequence<5?4:sequence-5,
  dspMaxTicks:96000,irqMaxTicks:102000,scanMaxTicks:240,usbPollMaxTicks:480,drawMaxTicks:120000};
 const response=[240,125,73,70,87,1,1,sequence,fields.length];
 for(const name of fields){let n=values[name]||0;for(let k=0;k<5;k++)response.push((n/2**(7*k))&127);}
 response.push(247);queueMicrotask(()=>{for(const f of listeners)f({data:response});});
}};
const ewi={id:'ewi',name:'Wind controller'},loop={id:'loop',name:'Virtual MIDI'};
const ports={inputs:new Map([['ewi',ewi],['in',input]]),outputs:new Map([['loop',loop],['out',output]])};
const source=readFileSync('web/diagnostics/diagnostics.js','utf8').replace(/^import .*;\n/gm,'');
vm.runInNewContext(source,{document:{getElementById:el,createElement:element},
 navigator:{async requestMIDIAccess(options){permissions++;assert.equal(options.sysex,true);return ports;}},
 readSnapshot,readBootSnapshot,readTrace,midiFile,performance:{now:()=>0},setTimeout:f=>setTimeout(f,0),Date,Blob,
 URL:{createObjectURL(blob){blobs.push(blob);report=blob;return 'blob:report';},revokeObjectURL(){}},console});
assert.equal(permissions,0);assert.equal(sent,0);
await el('connect').onclick();assert.equal(permissions,1);assert.equal(sent,0);
assert.equal(el('input').value,'in');assert.equal(el('output').value,'out');
await el('capture').onclick();assert.equal(sent,20);assert.equal(listeners.size,0);
const captured=JSON.parse(await report.text());assert.equal(captured.samples.length,20);
assert.equal(captured.summarizedFromSample,5);assert.equal(captured.timerHz,24000000);
assert.equal(el('rows').children[0].children[1].textContent,'4.000 ms / 5.805 ms');
assert.equal(el('rows').children[2].children[1].textContent,'14 / 0 / 0');
assert.equal(el('download').hidden,false);assert.equal(el('capture').disabled,false);
await el('boot').onclick();const bootReport=JSON.parse(await report.text());
assert.equal(bootReport.boot.rescue,1);assert.equal(bootReport.boot.previousStage,11);
assert.equal(sent,20);assert.match(el('status').textContent,/自动救援/);
readingTrace=true;await el('trace-read').onclick();
assert.equal(listeners.size,0);assert.equal(el('trace-read').disabled,false);assert.equal(el('trace-json').hidden,false);
assert.equal(el('trace-json').download,'OpenIFW-FM1-take-123.json');assert.equal(el('trace-midi').download,'OpenIFW-FM1-take-123.mid');
assert.equal(JSON.parse(await blobs.at(-2).text()).session,123);assert.equal((await blobs.at(-1).text()).slice(0,4),'MThd');
el('input').value='unavailable';await el('capture').onclick();assert.equal(sent,20);
assert.match(el('status').textContent,/没有可用/);
ports.inputs.set('two',{id:'two',name:'Felucca MIDI 2'});await el('connect').onclick();
assert.equal(el('input').value,'');assert.equal(el('output').value,'');
ports.inputs.delete('two');ports.inputs.delete('in');ports.outputs.delete('out');await el('connect').onclick();
assert.equal(el('input').value,'');assert.equal(el('output').value,'');assert.equal(sent,20);
console.log('PASS diagnostics page: permission on click, selected ports, 20 read-only snapshots, timer units, counter reset, JSON download. No hardware attached.');
