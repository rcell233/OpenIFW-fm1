// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {readTrace,midiFile} from '../web/diagnostics/trace.js';
const bytes=readFileSync('build/trs/replies.bin'),pages=[];
for(let offset=0;offset<bytes.length;){const size=10+bytes[offset+8]*5;pages.push(bytes.subarray(offset,offset+size));offset+=size;}
const events=new EventTarget(),input=events;let wrong=false;
const output={send(p){
 assert.deepEqual(p.slice(0,7),[240,125,73,70,87,1,4]);assert.equal(p.at(-1),247);
 const page=p[8]+128*p[9],reply=Uint8Array.from(pages[page]);assert.equal(reply[7],p[7]);
 if(wrong&&page===1)reply[14]++;
 queueMicrotask(()=>events.dispatchEvent(Object.assign(new Event('midimessage'),{data:reply})));
}};
const log=await readTrace(input,output);
assert.equal(log.start.breathCC,2);assert.equal(log.start.channel,4);assert.equal(log.start.glideMidiEnabled,true);
assert.equal(log.stopReason,'manual');assert.equal(log.events.length,18);
assert.deepEqual(log.events[0],{us:3000,source:'TRS',acceptedChannel:true,channel:4,bytes:[179,2,41],dmaNoticeToDispatchUs:40,dispatchToNextBlockUs:300});
assert.equal(log.events[1].acceptedChannel,false);assert.equal(log.events[2].source,'PANEL');assert.equal(log.events[3].source,'USB');
assert.equal(log.histogram['USB ch4 CC5'],1);
const smf=midiFile(log),d=new DataView(smf.buffer);assert.equal(d.getUint16(8),0);assert.equal(d.getUint16(12),1000);assert.equal(d.getUint32(18),smf.length-22);
let off=29,t=0;for(const e of log.events){
 let delta=0,x;do{x=smf[off++];delta=delta*128+(x&127);}while(x&128);t+=delta;
 assert.equal(t,Math.round(e.us/500));assert.deepEqual([...smf.slice(off,off+e.bytes.length)],e.bytes);off+=e.bytes.length;
}
assert.deepEqual([...smf.slice(-3)],[255,47,0]);wrong=true;await assert.rejects(readTrace(input,output),/录制已改变/);
console.log('PASS real C capture pages -> JSON timing/channel flags -> SMF note/control bytes and timing across pages.');
