// SPDX-License-Identifier: GPL-3.0-only
import {readSnapshot,readBootSnapshot} from './protocol.js';
import {readTrace,midiFile} from './trace.js?v=2';
const $=id=>document.getElementById(id);let access,url;
function saveReport(report){
 const text=JSON.stringify(report,null,2);$('raw').textContent=text;
 if(url)URL.revokeObjectURL(url);url=URL.createObjectURL(new Blob([text],{type:'application/json'}));
 $('download').href=url;$('download').hidden=false;
}
// The application USB product string is "Felucca". Do not guess from the
// first MIDI port: a wind controller or virtual driver often appears first.
function choosePair(){
 const deviceName=/felucca|openifw|fm[ _-]?1(?![0-9])/i;
 const lists=['inputs','outputs'].map(k=>[...access[k].values()].filter(p=>p.state!=='disconnected'));
 const candidates=lists.map(ports=>ports.filter(p=>deviceName.test(p.name||'') && !/update|loader|ota-/i.test(p.name||'')));
 const suggested=candidates.every(c=>c.length===1);
 for(const [i,id] of ['input','output'].entries()){
  const select=$(id);select.replaceChildren();
  const add=(value,label)=>{const o=document.createElement('option');o.value=value;o.textContent=label;select.append(o);};
  add('','请选择 FM-1 的 MIDI '+(i?'输出':'输入'));
  for(const port of lists[i])add(port.id,port.name||port.id);
  select.value=suggested?candidates[i][0].id:'';
 }
 return suggested;
}
$('connect').onclick=async()=>{
 try{
  if(!navigator.requestMIDIAccess)throw Error('请使用桌面 Chrome 或 Edge。');
  access=await navigator.requestMIDIAccess({sysex:true});
  const suggested=choosePair();
  $('ports').hidden=false;$('status').textContent=suggested?'已选中 FM-1 候选端口；读取录制时会向设备核对版本。':'请手动选择 FM-1 的输入和输出。它通常显示为 Felucca，请勿选择电吹管或虚拟 MIDI 口。';
 }catch(error){$('status').textContent=error.message;}
};
$('capture').onclick=async()=>{
 const input=access?.inputs.get($('input').value),output=access?.outputs.get($('output').value);
 if(!input||!output){$('status').textContent='没有可用的 MIDI 输入或输出。';return;}
 $('trace-read').disabled=$('boot').disabled=$('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=true;
 $('progress').hidden=false;$('progress').value=0;$('download').hidden=true;
 const samples=[];
 try{
  await input.open();await output.open();
  for(let i=0;i<20;i++){
   $('status').textContent=`正在采集 ${i+1}/20，请照常演奏和操作。`;
   const start=performance.now();samples.push(await readSnapshot(input,output,i));$('progress').value=i+1;
   await new Promise(resolve=>setTimeout(resolve,Math.max(0,500-(performance.now()-start))));
  }
  let epoch=0;for(let i=1;i<samples.length;i++)if(samples[i].blocks<samples[i-1].blocks)epoch=i;
  const a=samples[epoch],b=samples.at(-1),delta=k=>(b[k]-a[k])>>>0;
  if(!b.blocks){saveReport({kind:'hardware-no-audio-blocks',samples});$('status').textContent='设备尚未开始正常音频处理。请点击“读取启动信息”检查救援原因。';return;}
  const values=[['DSP 峰值',`${(b.dspMaxTicks/24000).toFixed(3)} ms / 5.805 ms`],['完整音频 IRQ 峰值',`${(b.irqMaxTicks/24000).toFixed(3)} ms`],
   ['本次采集 XRUN / DMA / UND',`${delta('xruns')} / ${delta('dmaLate')} / ${delta('missedCommits')}`],['本次采集 CLIP',String(delta('clips'))],
   ['输入扫描 IRQ 峰值',`${(b.scanMaxTicks/24).toFixed(1)} µs`],['完整 USB 轮询峰值',`${(b.usbPollMaxTicks/24).toFixed(1)} µs`],
   ['绘图峰值（含音频抢占）',`${(b.drawMaxTicks/24000).toFixed(3)} ms`],['USB Audio underrun / overrun / missed',`${delta('uacUnderruns')} / ${delta('uacOverruns')} / ${delta('uacMissed')}`]];
  $('rows').replaceChildren();for(const [name,value] of values){const tr=document.createElement('tr');for(const t of [name,value]){const cell=document.createElement('td');cell.textContent=t;tr.append(cell);}$('rows').append(tr);}
  const report={kind:'hardware-reported-read-only-performance',capturedAt:new Date().toISOString(),input:input.name,output:output.name,timerHz:24000000,summarizedFromSample:epoch,samples};
  saveReport(report);$('result').hidden=false;
  $('status').textContent='采集完成。可以下载报告，里面保留了全部设备原始读数。'+(epoch?' 采集中计数重置过，表格显示最后一段。':'');
 }catch(error){$('status').textContent=error.message;}
 finally{$('trace-read').disabled=$('boot').disabled=$('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=false;}
};
$('boot').onclick=async()=>{
 const input=access?.inputs.get($('input').value),output=access?.outputs.get($('output').value);
 if(!input||!output){$('status').textContent='没有可用的 MIDI 输入或输出。';return;}
 $('trace-read').disabled=$('boot').disabled=$('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=true;
 $('result').hidden=true;$('progress').hidden=true;$('download').hidden=true;
 try{
  await input.open();await output.open();const boot=await readBootSnapshot(input,output,0);
  saveReport({kind:'hardware-reported-read-only-boot',capturedAt:new Date().toISOString(),input:input.name,output:output.name,boot});
  const reason=['正常启动','连续启动失败后的自动救援','开机检测到 OCT−'][boot.entryReason]||'未知';
  const pc=boot.crashValid?'，保留的异常 PC：0x'+boot.crashPc.toString(16):'，没有有效的异常记录';
  $('status').textContent=`${reason}。当前阶段 ${boot.currentStage}，上次阶段 ${boot.previousStage}${pc}。可下载报告。`;
 }catch(error){$('status').textContent=error.message;}
 finally{$('trace-read').disabled=$('boot').disabled=$('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=false;}
};

const traceLinks=[];
$('trace-read').onclick=async()=>{
 const input=access?.inputs.get($('input').value),output=access?.outputs.get($('output').value);
 const msg=$('trace-status');
 if(!input||!output){msg.textContent='请在上方选择 FM-1 的输入和输出（通常名为 Felucca）。';return;}
 const buttons=['capture','boot','trace-read','input','output','connect'];
 for(const id of buttons)$(id).disabled=true;
 for(const url of traceLinks)URL.revokeObjectURL(url);traceLinks.length=0;
 $('trace-json').hidden=$('trace-midi').hidden=true;
 try {
  await input.open();await output.open();
  msg.textContent='正在向 FM-1 核对固件版本…';
  const log=await readTrace(input,output,(n,total)=>{msg.textContent='读取 '+n+' / '+total+' 条消息';},{status:s=>{msg.textContent=s;}});
  for(const [id,data,type,ext] of [['trace-json',JSON.stringify(log,null,2),'application/json','json'],['trace-midi',midiFile(log),'audio/midi','mid']]){
    const link=$(id),url=URL.createObjectURL(new Blob([data],{type}));link.href=url;link.download='OpenIFW-FM1-take-'+log.session+'.'+ext;link.hidden=false;traceLinks.push(url);
  }
  const cc=Object.entries(log.histogram).filter(([k])=>/CC5$|Pitch bend$/.test(k)).map(([k,n])=>k+' × '+n+(log.firmwareCode>=40717 && /^TRS ch\d+ CC5$/.test(k)?'（仅记录，固件已忽略）':''));
  msg.textContent='已读取 '+log.events.length+' 条，时长 '+(log.durationMs/1000).toFixed(1)+' 秒。'+(cc.length?cc.join('；'):'没有收到 CC5 或弯音。')+(log.stopReason==='capacity'?' 缓冲已满，录制提前结束。':'')+(log.droppedTrsBytes?' 串口发生丢字节，详见日志。':'');
 }catch(e){msg.textContent=e.message;}finally{for(const id of buttons)$(id).disabled=false;}
};
