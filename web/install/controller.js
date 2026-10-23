// SPDX-License-Identifier: GPL-3.0-only
// OpenIFW installer policy around Felucca's unchanged USB-MIDI transport.
import {Updater} from './lib/fm1ota.js';
import {Package,crc16,logicalImage,productOf,sha256hex} from './lib/fm1pkg.js';

export const PRODUCT='FM-1_982';
const PORT=/fm-1|felucca|ota|composite|sinco|usb-midi/i;
function fail(code,message){return Object.assign(new Error(message),{code});}

export async function verifyFirmware(bytes,meta){
  if(meta?.schema!==1||meta.product!==PRODUCT||!/^\d+\.\d+\.\d+(?:-rc[1-9]\d*)?$/.test(meta.version)||
      !/^[a-f0-9]{64}$/.test(meta.sha256)||!Number.isSafeInteger(meta.bytes)||
      meta.bytes<960||meta.bytes>2*1024*1024||
      meta.file!=='OpenIFW-FM1-'+meta.version+'.fwsc')
    throw fail('badpkg','固件发布信息不完整，请刷新页面。');
  if(meta.version.includes('-rc')&&meta.experimental!==true)throw fail('badpkg','候选固件缺少实验版标记。');
  if(bytes.length!==meta.bytes||await sha256hex(bytes)!==meta.sha256)
    throw fail('badpkg','固件下载不完整或校验不符；没有开始写入，请刷新重试。');
  if(productOf(bytes)!==PRODUCT)
    throw fail('badpkg','这个文件不是 OpenIFW FM-1 固件。');
  try{
    const pkg=new Package(bytes),image=logicalImage(bytes);
    if(!new TextDecoder('latin1').decode(bytes).includes('FELUCCA-LOADER-1'))
      throw new Error('missing update loader');
    if(!pkg.entries.some(e=>e.type===100&&e.name==='ota.bin'))
      throw new Error('missing ota.bin');
    for(const e of pkg.entries){
      if(e.dataOff+e.size>image.length||crc16(image.subarray(e.dataOff,e.dataOff+e.size))!==e.dcrc)
        throw new Error(e.name+' CRC');
    }
    return {image,product:PRODUCT,meta:Object.freeze({...meta})};
  }catch(e){throw fail('badpkg','固件内部校验失败：'+e.message);}
}

export async function loadFirmware(baseURL,fetcher=fetch,release='release.json'){
  if(!['release.json','recovery-0.4.1.json','recovery-0.4.4.json','recovery-0.4.6-rc2.json','recovery-0.4.7-rc2.json','recovery-0.4.7-rc3.json','recovery-0.4.7-rc4.json','recovery-0.4.7-rc5.json','recovery-0.4.7-rc7.json','candidate-0.4.7-rc2.json','candidate-0.4.7-rc3.json','candidate-0.4.7-rc4.json','candidate-0.4.7-rc5.json','candidate-0.4.7-rc7.json','candidate-0.4.7-rc8.json'].includes(release))
    throw fail('badpkg','未知的固件发布入口。');
  const response=await fetcher(new URL('./'+release,baseURL),{cache:'no-store'});
  if(!response.ok)throw fail('download','无法读取固件信息（HTTP '+response.status+'）。');
  const meta=await response.json();
  if(release!=='release.json'&&release!==`${release.startsWith('candidate-')?'candidate':'recovery'}-${meta?.version}.json`)
    throw fail('badpkg','恢复固件版本与入口不一致。');
  if(!/^OpenIFW-FM1-\d+\.\d+\.\d+(?:-rc[1-9]\d*)?\.fwsc$/.test(meta?.file??''))
    throw fail('badpkg','固件文件名无效。');
  const url=new URL('../firmware/'+meta.file,baseURL);
  const binary=await fetcher(url,{cache:'no-store'});
  if(!binary.ok)throw fail('download','固件下载失败（HTTP '+binary.status+'），请刷新重试。');
  return verifyFirmware(new Uint8Array(await binary.arrayBuffer()),meta);
}

// Only matching FM-1 port pairs are considered. Never choose among multiple devices.
export function candidateCount(access){
  const outputs=[...access.outputs.values()];
  return [...access.inputs.values()].filter(i=>i.state!=='disconnected'&&PORT.test(i.name??'')&&
    outputs.some(o=>o.state!=='disconnected'&&o.name===i.name)).length;
}

export async function installFirmware(verified,access,onStep=()=>{}){
  if(verified.product!==PRODUCT)throw fail('badpkg','固件型号不匹配。');
  if(candidateCount(access)>1)
    throw fail('multiple','检测到多台可能的 FM-1。请只连接要安装的那一台，然后重试。');
  const up=new Updater(access);
  onStep('connect');
  try{
    const ota=await up.find(id=>/^ota-/i.test(id.model));
    if(ota){
      ota.link.close();
      if(!/^ota-FM-1_9\d\d$/i.test(ota.id.text))
        throw fail('foreign','设备正在其他固件的升级模式中，请先用它原来的升级器完成更新。');
      onStep('resume');
      if(!await up.resume(verified.image,onStep,PRODUCT))
        throw fail('stopped','恢复写入未完成。重新连接 USB 后，再点 Install 继续。');
      return PRODUCT;
    }
    return await up.install(verified.image,PRODUCT,onStep);
  }catch(e){
    if(e?.code==='mismatch'){
      let identity=e.detail;
      // The vendor resume path omits the reported identity from its error.
      if(!identity){const back=await up.find(id=>id.model==='FM-1');identity=back?.id.text;back?.link.close();}
      if(identity==='FM-1_000')throw fail('rescue','设备已进入 USB RESCUE，正常固件没有通过启动确认。');
    }
    throw e;
  }finally{
    const ports=[...access.inputs.values(),...access.outputs.values()].filter(p=>PORT.test(p.name??''));
    await Promise.allSettled(ports.map(p=>{
      if('onmidimessage' in p)p.onmidimessage=null;
      return p.close?.();
    }));
  }
}

export const ERROR_TEXT={
  denied:'没有获得 MIDI / SysEx 权限。请在浏览器的站点设置中允许 MIDI，然后重试。',
  notfound:'没有找到 FM-1。请使用 USB 数据线连接，打开设备，并关闭占用 MIDI 的其他软件或网页。',
  model:'连接的设备型号不是 FM-1，没有开始写入。',
  lost:'USB 连接中断。重新连接设备后，再点 Install；兼容的升级模式会继续完成写入。',
  stopped:'传输已停止。重新连接设备后，再点 Install 重试。',
  noloader:'没有找到升级模式中的 FM-1。重新插拔 USB，再点 Install 重试。',
  noreturn:'写入后没有检测到设备重启。请重新接通设备电源，再确认设备状态。',
  mismatch:'设备重启后报告了不同的固件身份，安装没有通过最终确认。',
  rescue:'设备进入了 USB RESCUE（FM-1_000），正常固件尚未启动。保持当前连接，可使用本页的“回退到 0.4.7-rc2”入口恢复。',
  badreq:'设备请求的数据超出固件范围，传输已停止。',
};
export function errorText(e){return ERROR_TEXT[e?.code]??e?.message??'安装失败，请重试。';}
