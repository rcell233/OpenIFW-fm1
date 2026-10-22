// SPDX-License-Identifier: GPL-3.0-only
export const fields=['schema','firmwareCode','uptimeMs','preset','sampleRate','blockFrames','blocks',
 'dspTicks','dspMaxTicks','irqTicks','irqMaxTicks','xruns','dmaLate','missedCommits','peak','clips',
 'scanCount','scanTotalTicks','scanMaxTicks','usbPollCount','usbPollTotalTicks','usbPollMaxTicks',
 'uacUnderruns','uacOverruns','uacMissed','uacStalls','uacRate','mainMaxGapMs','uiFrames','lcdPixels',
 'drawMaxTicks','irqTotalTicks'];
export function decode(bytes,sequence) {
 const b=Array.from(bytes),h=[0xf0,0x7d,73,70,87,1,1,sequence,fields.length];
 if(b.length!==10+fields.length*5||!h.every((v,i)=>b[i]===v)||b.at(-1)!==0xf7)return null;
 const out={};
 for(let i=0;i<fields.length;i++) {
  let v=0;for(let k=0;k<5;k++){const n=b[9+i*5+k];if(!Number.isInteger(n)||n<0||n>127||(k===4&&n>15))return null;v+=n*2**(7*k);}
  out[fields[i]]=v;
 }
 return out.schema===1?out:null;
}
export async function readSnapshot(input,output,sequence,timeoutMs=2500) {
 sequence&=127;
 return new Promise((resolve,reject)=>{
  const end=(error,value)=>{clearTimeout(timer);input.removeEventListener('midimessage',receive);error?reject(error):resolve(value);};
  const receive=event=>{const value=decode(event.data,sequence);if(value)end(null,value);};
  const timer=setTimeout(()=>end(new Error('设备没有返回性能数据。请确认已安装 0.4.7-rc1 或更新版本，并选择 FM-1 对应的输入与输出。')),timeoutMs);
  input.addEventListener('midimessage',receive);
  try{output.send([0xf0,0x7d,73,70,87,1,0,sequence,0xf7]);}catch(error){end(error);}
 });
}
