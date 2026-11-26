// SPDX-License-Identifier: GPL-3.0-only
import {readBootSnapshot} from './protocol.js';
// Read frozen indexed pages; the device never starts or stops recording over USB.
function decodeTrace(bytes,seq,page) {
 const b=Array.from(bytes),n=b[8];
 if(n<4||n>28||b.length!==10+5*n||b.at(-1)!==247||
    ![240,125,73,70,87,1,5,seq].every((v,i)=>b[i]===v)) return null;
 const w=[];for(let i=0;i<n;i++){let v=0;for(let k=0;k<5;k++){const x=b[9+5*i+k];if(x>127||(k===4&&x>15))return null;v+=x*2**(7*k);}w.push(v);}
 return w[0]===1&&w[2]===page?w:null;
}
let serial=0;
const nextSeq=()=>serial++ & 127;
function request(input,output,seq,page,timeout) {
 return new Promise((resolve,reject)=>{
  const done=(err,w)=>{clearTimeout(timer);input.removeEventListener('midimessage',on);err?reject(err):resolve(w);};
  const on=e=>{const w=decodeTrace(e.data,seq,page);if(w)done(null,w);};
  const timer=setTimeout(()=>done(null,null),timeout);
  input.addEventListener('midimessage',on);
  try{output.send([240,125,73,70,87,1,4,seq,page&127,page>>7,247]);}catch(e){done(e);}
 });
}
function firmwareName(code){
 return code>=40701&&code<40800?'0.4.7-rc'+(code-40700):String(code);
}
export async function readTrace(input,output,progress=()=>{},{status=()=>{},timeout=1800}={}) {
 const ports='输出「'+(output.name||output.id||'?')+'」→ 输入「'+(input.name||input.id||'?')+'」';
 const boot=await readBootSnapshot(input,output,nextSeq(),timeout).catch(()=>null);
 if(boot?.rescue)throw Error('设备报告 USB RESCUE；当前未运行演奏固件，无法读取演奏录制。');
 if(boot && boot.firmwareCode<40716)throw Error('设备实际报告 '+firmwareName(boot.firmwareCode)+'；机身录制需要 0.4.7-rc16 或更新版本。');
 let version=boot?firmwareName(boot.firmwareCode):'';
 async function pageRead(page){
  const where=page?'数据页 '+page:'录制头';
  for(let attempt=0;attempt<3;attempt++){
   if(attempt || !page)status((version?'已确认 '+version+'。':'')+(attempt?'重试'+where+'（'+attempt+'/2）…':'读取'+where+'…'));
   const w=await request(input,output,nextSeq(),page,timeout);if(w)return w;
   if(input.state==='disconnected'||output.state==='disconnected')throw Error('MIDI 端口已断开：'+ports+'。请保持 FM-1 供电，重新连接诊断页。');
  }
  if(!version)throw Error('启动查询和录制查询均未收到有效回复：'+ports+'。请在两栏选择 FM-1 的 USB 端口（通常名为 Felucca），关闭占用端口的软件后重试。请保持机身供电。');
  throw Error('已确认设备 '+version+'，但'+where+'三次均未收到有效回复（'+ports+'）。请保持机身供电并再次读取；不必重录。');
 }
 const head=await pageRead(0),[,id,,mode,count,durationMs,preset,channel,breathCC,curve,raw,dropped,glideMidi,glideQ31,bendQ31,held,patchHash,firmwareCode,glideMode,glideMin,glideDepth,breathMs,glideMs]=head;
 if(head.length!==23||!Number.isInteger(count)||count>4096)throw Error('录制头格式不匹配。');
 version=firmwareName(firmwareCode);
 if(!mode)throw Error('设备里没有录制。请在 GLO → PERFORMANCE 短按 REC，演奏后再按一次。');
 if(mode===1)throw Error('设备正在录制。请在 PERFORMANCE 页再按一次 REC 停止，然后读取。');
 const events=[];for(let page=1;events.length<count;page++){
   const w=await pageRead(page);
   if(w[1]!==id||w[3]!==mode||w.length!==4+3*Math.min(8,count-events.length))throw Error('设备录制已改变，请重新读取。');
   for(let i=4;i<w.length;i+=3){const [us,m,lag]=w.slice(i,i+3),s=m&255,a=(m>>>8)&127,b=(m>>>16)&127;
    const wait=v=>v===65535?null:v;
    events.push({us,source:['USB','TRS','PANEL'][(m>>>24)&3],acceptedChannel:!!(m&(1<<26)),channel:1+(s&15),
      bytes:(s&224)===192?[s,a]:[s,a,b],dmaNoticeToDispatchUs:wait(lag&65535),[firmwareCode>=40718?'dispatchToNextSliceUs':'dispatchToNextBlockUs']:wait(lag>>>16)});
   }
   if(page%8===0||events.length===count)progress(events.length,count);
 }
 const histogram={};for(const e of events){
  const [s,a]=e.bytes,op=s>>4,k=e.source+' ch'+e.channel+' '+(op===11?'CC'+a:({8:'Note Off',9:e.bytes[2]?'Note On':'Note Off',10:'Poly pressure',12:'Program',13:'Pressure',14:'Pitch bend'})[op]);
  histogram[k]=(histogram[k]||0)+1;
 }
 return {schema:'openifw-midi-trace-1',firmwareCode,session:id,stopReason:({2:'manual',3:'capacity',4:'120s'})[mode],durationMs,
   start:{preset,patchHash,channel,breathCC,breathCurve:[0,8,16,24].map(k=>(curve>>>k)&255),lastBreathRaw:raw?raw-1:null,glideMidiEnabled:!!glideMidi,glideQ31,bendQ31:bendQ31|0,heldNotes:held,glideMode:glideMode?'Rate':'Time',glideMinimum:glideMin/65536,glideDepth:glideDepth/65536,breathSmoothMs:breathMs/65536,glideSmoothMs:glideMs/65536},
   droppedTrsBytes:dropped,histogram,events};
}
// SMF 0, 500,000 us/quarter and 1,000 ticks/quarter (0.5 ms/tick).
// Keep the original order and channel bytes, even for messages filtered by FM-1.
export function midiFile(recording){
 const track=[],vlq=n=>{let a=[n&127];while((n=Math.floor(n/128)))a.unshift(128|(n&127));track.push(...a);};
 let tick=0;track.push(0,255,81,3,7,161,32);
 for(const e of recording.events){const t=Math.max(tick,Math.round(e.us/500));vlq(t-tick);track.push(...e.bytes);tick=t;}
 vlq(Math.max(0,Math.round(recording.durationMs*2)-tick));track.push(255,47,0);
 const n=track.length;return Uint8Array.from([77,84,104,100,0,0,0,6,0,0,0,1,3,232,77,84,114,107,n>>>24,(n>>>16)&255,(n>>>8)&255,n&255,...track]);
}
