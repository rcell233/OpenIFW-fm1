// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {readTrace,midiFile} from '../web/diagnostics/trace.js';
const bytes=readFileSync('build/trs/replies.bin'),pages=[];
for(let offset=0;offset<bytes.length;){const size=10+bytes[offset+8]*5;pages.push(bytes.subarray(offset,offset+size));offset+=size;}
const events=new EventTarget(),input=events;input.name='Felucca IN';
let wrong=false,firmware=40716,rescue=0,dropPage=1,rep=0,ignoreBoot=false,ignorePages=false;
const emit=data=>queueMicrotask(()=>events.dispatchEvent(Object.assign(new Event('midimessage'),{data})));
const output={name:'Felucca OUT',send(p){
 if(p[6]===2){
  if(ignoreBoot)return;
  const boot=[240,125,73,70,87,1,3,p[7],20];
  for(const v of [1,firmware,rescue,...Array(17).fill(0)])for(let i=0;i<5;i++)boot.push((v>>>(7*i))&127);
  emit([...boot,247]);return;
 }
 assert.deepEqual(p.slice(0,7),[240,125,73,70,87,1,4]);assert.equal(p.at(-1),247);
 const page=p[8]+128*p[9];if(ignorePages)return;
 const reply=Uint8Array.from(pages[page]);assert.equal(reply[7],page&127);reply[7]=p[7];
 if(wrong&&page===1)reply[14]++;
 if(page===dropPage && rep++===0){const stale=reply.slice();stale[7]=(p[7]+127)&127;emit(stale);return;}
 emit(reply);
}};
const read=()=>readTrace(input,output,()=>{},{timeout:10});
const log=await read();assert.equal(rep,2);
assert.equal(log.start.breathCC,2);assert.equal(log.start.channel,4);assert.equal(log.start.glideMidiEnabled,true);
assert.equal(log.stopReason,'manual');assert.equal(log.events.length,18);
assert.deepEqual(log.events[0],{us:3000,source:'TRS',acceptedChannel:true,channel:4,bytes:[179,2,41],dmaNoticeToDispatchUs:40,dispatchToNextSliceUs:300});
assert.equal(log.events[1].acceptedChannel,false);assert.equal(log.events[2].source,'PANEL');assert.equal(log.events[3].source,'USB');
assert.equal(log.histogram['USB ch4 CC5'],1);
const smf=midiFile(log),d=new DataView(smf.buffer);assert.equal(d.getUint16(8),0);assert.equal(d.getUint16(12),1000);assert.equal(d.getUint32(18),smf.length-22);
let off=29,t=0;for(const e of log.events){
 let delta=0,x;do{x=smf[off++];delta=delta*128+(x&127);}while(x&128);t+=delta;
 assert.equal(t,Math.round(e.us/500));assert.deepEqual([...smf.slice(off,off+e.bytes.length)],e.bytes);off+=e.bytes.length;
}
assert.deepEqual([...smf.slice(-3)],[255,47,0]);wrong=true;await assert.rejects(read(),/录制已改变/);wrong=false;
firmware=40715;await assert.rejects(read(),/实际报告 0.4.7-rc15/);
rescue=1;await assert.rejects(read(),/USB RESCUE/);rescue=0;firmware=40716;
ignorePages=true;await assert.rejects(read(),/已确认设备 0.4.7-rc16.*录制头/);
ignoreBoot=true;await assert.rejects(read(),/启动查询和录制查询均.*Felucca OUT.*Felucca IN/);
ignorePages=false;assert.deepEqual((await read()).events,log.events);
console.log('PASS real C capture pages -> JSON timing/channel flags -> SMF note/control bytes and timing across pages.');
