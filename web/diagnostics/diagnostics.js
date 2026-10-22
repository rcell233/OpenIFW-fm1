// SPDX-License-Identifier: GPL-3.0-only
import {readSnapshot} from './protocol.js';
const $=id=>document.getElementById(id);let access,url;
function choose(element,ports){
 element.replaceChildren();
 for(const port of ports){const o=document.createElement('option');o.value=port.id;o.textContent=port.name||port.id;element.append(o);}
 const matches=ports.filter(p=>/fm.?1|openifw/i.test(p.name||''));if(matches.length===1)element.value=matches[0].id;
}
$('connect').onclick=async()=>{
 try{
  if(!navigator.requestMIDIAccess)throw Error('请使用桌面 Chrome 或 Edge。');
  access=await navigator.requestMIDIAccess({sysex:true});
  choose($('input'),[...access.inputs.values()]);choose($('output'),[...access.outputs.values()]);
  $('ports').hidden=false;$('status').textContent='选择设备对应的输入和输出，然后开始采集。';
 }catch(error){$('status').textContent=error.message;}
};
$('capture').onclick=async()=>{
 const input=access?.inputs.get($('input').value),output=access?.outputs.get($('output').value);
 if(!input||!output){$('status').textContent='没有可用的 MIDI 输入或输出。';return;}
 $('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=true;
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
  const values=[['DSP 峰值',`${(b.dspMaxTicks/24000).toFixed(3)} ms / 5.805 ms`],['完整音频 IRQ 峰值',`${(b.irqMaxTicks/24000).toFixed(3)} ms`],
   ['本次采集 XRUN / DMA / missed commit',`${delta('xruns')} / ${delta('dmaLate')} / ${delta('missedCommits')}`],['本次采集 CLIP',String(delta('clips'))],
   ['输入扫描 IRQ 峰值',`${(b.scanMaxTicks/24).toFixed(1)} µs`],['完整 USB 轮询峰值',`${(b.usbPollMaxTicks/24).toFixed(1)} µs`],
   ['绘图峰值（含音频抢占）',`${(b.drawMaxTicks/24000).toFixed(3)} ms`],['USB Audio underrun / overrun / missed',`${delta('uacUnderruns')} / ${delta('uacOverruns')} / ${delta('uacMissed')}`]];
  $('rows').replaceChildren();for(const [name,value] of values){const tr=document.createElement('tr');for(const t of [name,value]){const cell=document.createElement('td');cell.textContent=t;tr.append(cell);}$('rows').append(tr);}
  const report={kind:'hardware-reported-read-only-performance',capturedAt:new Date().toISOString(),input:input.name,output:output.name,timerHz:24000000,summarizedFromSample:epoch,samples};
  const text=JSON.stringify(report,null,2);$('raw').textContent=text;$('result').hidden=false;
  if(url)URL.revokeObjectURL(url);url=URL.createObjectURL(new Blob([text],{type:'application/json'}));$('download').href=url;$('download').hidden=false;
  $('status').textContent='采集完成。可以下载报告，里面保留了全部设备原始读数。'+(epoch?' 采集中计数重置过，表格显示最后一段。':'');
 }catch(error){$('status').textContent=error.message;}
 finally{$('capture').disabled=$('connect').disabled=$('input').disabled=$('output').disabled=false;}
};
