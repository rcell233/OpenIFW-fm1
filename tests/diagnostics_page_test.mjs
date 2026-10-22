// SPDX-License-Identifier: GPL-3.0-only
// Exercise the actual page and protocol with a simulated MIDI device.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {readSnapshot,fields} from '../web/diagnostics/protocol.js';
const elements=new Map(),listeners=new Set();let permissions=0,sent=0,report;
const element=()=>({value:'',hidden:true,disabled:false,textContent:'',children:[],
 append(v){this.children.push(v);if(!this.value&&v.value)this.value=v.value;},replaceChildren(){this.children=[];}});
const el=id=>{if(!elements.has(id))elements.set(id,element());return elements.get(id);};
const input={id:'in',name:'FM-1 MIDI',async open(){},addEventListener(_k,f){listeners.add(f);},removeEventListener(_k,f){listeners.delete(f);}};
const output={id:'out',name:'FM-1 MIDI',async open(){},send(request){
 const sequence=sent++;assert.deepEqual(Array.from(request),[240,125,73,70,87,1,0,sequence,247]);
 const values={schema:1,firmwareCode:40701,sampleRate:44100,blockFrames:256,
  blocks:sequence<5?100+sequence:sequence-5,xruns:sequence<5?4:sequence-5,
  dspMaxTicks:96000,irqMaxTicks:102000,scanMaxTicks:240,usbPollMaxTicks:480,drawMaxTicks:120000};
 const response=[240,125,73,70,87,1,1,sequence,fields.length];
 for(const name of fields){let n=values[name]||0;for(let k=0;k<5;k++)response.push((n/2**(7*k))&127);}
 response.push(247);queueMicrotask(()=>{for(const f of listeners)f({data:response});});
}};
const source=readFileSync('web/diagnostics/diagnostics.js','utf8').replace(/^import .*;\n/m,'');
vm.runInNewContext(source,{document:{getElementById:el,createElement:element},
 navigator:{async requestMIDIAccess(options){permissions++;assert.equal(options.sysex,true);return {inputs:new Map([['in',input]]),outputs:new Map([['out',output]])};}},
 readSnapshot,performance:{now:()=>0},setTimeout:f=>setTimeout(f,0),Date,Blob,
 URL:{createObjectURL(blob){report=blob;return 'blob:report';},revokeObjectURL(){}},console});
assert.equal(permissions,0);assert.equal(sent,0);
await el('connect').onclick();assert.equal(permissions,1);assert.equal(sent,0);
await el('capture').onclick();assert.equal(sent,20);assert.equal(listeners.size,0);
const captured=JSON.parse(await report.text());assert.equal(captured.samples.length,20);
assert.equal(captured.summarizedFromSample,5);assert.equal(captured.timerHz,24000000);
assert.equal(el('rows').children[0].children[1].textContent,'4.000 ms / 5.805 ms');
assert.equal(el('rows').children[2].children[1].textContent,'14 / 0 / 0');
assert.equal(el('download').hidden,false);assert.equal(el('capture').disabled,false);
el('input').value='unavailable';await el('capture').onclick();assert.equal(sent,20);
assert.match(el('status').textContent,/没有可用/);
console.log('PASS diagnostics page: permission on click, selected ports, 20 read-only snapshots, timer units, counter reset, JSON download. No hardware attached.');
