// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {attachMidi} from '../web/midi-input.js';

const turns=()=>new Promise(r=>setImmediate(r));
const monitor=()=>new Promise(r=>setTimeout(r,100));
function setup(opts={}) {
  const inputs=new Map(), access={inputs}, forwarded=[], options=[];
  const status={}, activity={}, button={addEventListener(k,f){this[k]=f;}};
  const nav={requestMIDIAccess:async o=>{options.push(o);return access;}};
  const midi=attachMidi({button,status,activity,nav,secure:true,forward:d=>forwarded.push([...d]),...opts});
  function add(id,open=async()=>{}){const p={id,name:id,state:'connected',open};inputs.set(id,p);access.onstatechange?.();return p;}
  return {button,status,activity,options,add,inputs,access,midi,forwarded};
}
test('unsupported API, policy block and insecure origin explain their limits',()=>{
  for(const [opts,re] of [[{nav:{}},/浏览器/],[{secure:false},/HTTPS/],[{policy:{allowsFeature:()=>false}},/新标签页/]]){
    const {button,status}=setup(opts);assert.equal(button.disabled,true);assert.match(status.textContent,re);
  }
});
test('pending -> denied -> retry; permission does not imply an input',async()=>{
  let reject;const nav={requestMIDIAccess(){return new Promise((_,r)=>{reject=r;});}};
  const {button,status,access}=setup({nav});const click=button.click();assert.ok(button.disabled);assert.match(status.textContent,/请求/);
  reject(Object.assign(new Error(),{name:'NotAllowedError'}));await click;
  assert.equal(button.disabled,false);assert.match(status.textContent,/网站设置/);
  nav.requestMIDIAccess=async()=>access;await button.click();assert.match(status.textContent,/未发现/);
});
test('open/failed ports, power, zero CC, notes and hotplug',async()=>{
  const x=setup();await x.button.click();assert.deepEqual(x.options,[{sysex:false}]);
  let opens=0,fail=true;const p=x.add('Wind USB',async()=>{opens++; x.access.onstatechange();if(fail)throw Error('busy');});
  await turns();assert.match(x.status.textContent,/无法打开.*Wind USB/);
  fail=false;await x.button.click();await turns();assert.equal(opens,2);assert.match(x.status.textContent,/Wind USB.*Switch on/);
  p.onmidimessage({data:[0xb2,2,0]});await monitor();assert.match(x.activity.textContent,/CH 3 · CC2 = 0/);assert.deepEqual(x.forwarded,[]);
  x.midi.setPowered(true);assert.doesNotMatch(x.status.textContent,/Switch on/);
  for(const data of [[0xb2,2,78],[0x92,60,70],[0x92,60,0],[0xf8]])p.onmidimessage({data});
  await monitor();assert.deepEqual(x.forwarded,[[0xb2,2,78],[0x92,60,70],[0x92,60,0]]);assert.match(x.activity.textContent,/Note Off 60/);
  p.state='disconnected';x.access.onstatechange();assert.equal(p.onmidimessage,null);assert.equal(x.activity.textContent,'');assert.match(x.status.textContent,/未发现/);
  p.state='connected';x.access.onstatechange();await turns();assert.match(x.status.textContent,/已连接：Wind USB/);assert.equal(opens,3);
});
test('unplug while opening cannot attach stale listeners',async()=>{
  const x=setup();await x.button.click();let finish;const p=x.add('Unplugged',()=>new Promise(r=>finish=r));
  x.inputs.clear();x.access.onstatechange();finish();await turns();
  assert.equal(p.onmidimessage,null);assert.match(x.status.textContent,/未发现/);
});
test('a USB input mock can play the deployed AudioWorklet and WASM firmware',async()=>{
  const {readFileSync}=await import('node:fs');
  let Processor;
  globalThis.currentTime=0;
  globalThis.AudioWorkletProcessor=class {constructor(){this.port={postMessage(){}};}};
  globalThis.registerProcessor=(name,c)=>{assert.equal(name,'openifw');Processor=c;};
  await import('../build/web/worklet.js');
  const worklet=new Processor(), wasm=readFileSync('build/web/openifw.wasm');
  await worklet.onMessage({type:'load',wasm,sectors:{},master:700});
  const x=setup({forward:d=>worklet.onMessage({type:'midi',data:[...d]})});
  const p=x.add('Keyboard');await x.button.click();await turns();x.midi.setPowered(true);
  function render(){
    let peak=0;
    for(let i=0;i<32;i++){
      const out=[new Float32Array(128),new Float32Array(128)];
      worklet.process([],[out]);for(const ch of out)for(const sample of ch)peak=Math.max(peak,Math.abs(sample));
      globalThis.currentTime+=128/44100;
    }
    return peak;
  }
  p.onmidimessage({data:[0xb4,2,100]});p.onmidimessage({data:[0x94,65,112]});
  assert.ok(render()>0);assert.equal(worklet.ex.web_note(),65);assert.equal(worklet.ex.web_held(),1);
  p.onmidimessage({data:[0x84,65,0]});render();assert.equal(worklet.ex.web_held(),0);
});
