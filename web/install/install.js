// SPDX-License-Identifier: GPL-3.0-only
import {loadFirmware,installFirmware,errorText} from './controller.js?v=0.4.7-rc5';
const $=id=>document.getElementById(id),button=$('install'),status=$('status'),bar=$('progress');
const requestedRecovery=new URL(globalThis.location?.href??import.meta.url).searchParams.get('recovery');
const rollback=['0.4.1','0.4.4','0.4.6-rc2','0.4.7-rc2','0.4.7-rc3','0.4.7-rc4'].includes(requestedRecovery)?requestedRecovery:null;
const requestedCandidate=new URL(globalThis.location?.href??import.meta.url).searchParams.get('candidate');
const candidate=['0.4.7-rc2','0.4.7-rc3','0.4.7-rc4','0.4.7-rc5'].includes(requestedCandidate)?requestedCandidate:null;
let verified=null,busy=false;
const log=s=>{$('log').textContent+=(new Date().toLocaleTimeString()+' '+s+'\n');};
const say=(s,state='')=>{status.textContent=s;status.className='status '+state;};
function stage(n){document.querySelectorAll('[data-stage]').forEach(e=>{const i=Number(e.dataset.stage);e.classList.toggle('active',i===n);e.classList.toggle('complete',i<n);});}
function step(k,a){
  if(k==='connect'){stage(0);say('正在查找 FM-1…');}
  if(k==='start'){stage(1);say('已连接设备，正在准备升级。');}
  if(k==='verify'){stage(1);say('设备正在检查固件 · 已响应 '+a+' 个数据块');}
  if(k==='loader'){stage(1);say('正在等待设备切换到升级模式…');}
  if(k==='resume'){stage(2);say('检测到升级模式，正在恢复写入…');}
  if(k==='write'){stage(2);say('正在写入固件 · 已响应 '+a+' 个数据块');}
  if(k==='reboot'){stage(3);say('写入结束，正在等待设备重启并确认固件身份…');}
  if(k==='done'){stage(4);bar.max=1;bar.value=1;say('安装完成，FM-1 已重启并通过固件身份确认。','done');}
  if(!['write','verify'].includes(k))log(k+(a?' '+a:''));
}
async function prepare(){
  if(!isSecureContext){say('请通过 HTTPS 打开本页。','error');button.textContent='需要 HTTPS';return;}
  try{
    verified=await loadFirmware(import.meta.url,undefined,rollback?`recovery-${rollback}.json`:candidate?`candidate-${candidate}.json`:'release.json');
    const m=verified.meta;
    $('version').textContent='OpenIFW FM-1 · '+m.version;
    $('identity').textContent=m.product+' · '+m.bytes.toLocaleString()+' bytes';
    $('digest').textContent='SHA-256\n'+m.sha256;
    $('integrity').textContent='SHA-256、固件身份和包内 CRC 校验已通过。';
    $('download').href=new URL('../firmware/'+m.file,import.meta.url).href;
    $('download').hidden=false;
    $('recovery-note').hidden=!rollback;
    if(rollback)$('recovery-note').textContent=rollback==='0.4.7-rc4'?'当前选择 0.4.7-rc4 回退包：保留约 6 dB 输出增益，恢复之前的 Q15 混响。':rollback==='0.4.7-rc3'?'当前选择 0.4.7-rc3 回退包：保留半速混响优化，恢复较低的原输出音量。':rollback==='0.4.7-rc2'?'当前选择 0.4.7-rc2 回退包：已确认可以运行，JUDD 正常，Daisy / Tresure 仍会爆音。':rollback==='0.4.6-rc2'?'当前选择 0.4.6-rc2 回退包：用户反馈界面较流畅，爆音仍未解决。':rollback==='0.4.4'?'当前选择 0.4.4 回退包：已收到设备正常启动的反馈，仍有 UI 卡顿和少量破音。':'当前选择 0.4.1 恢复包：此前可启动，原有声音问题仍然存在。';
    if(!navigator.requestMIDIAccess){say('这个浏览器无法执行网页安装，请使用桌面版 Chrome 或 Edge。','error');button.textContent='请使用 Chrome / Edge';return;}
    button.textContent='Install '+m.version;button.disabled=false;
    say(rollback?`回退包 ${rollback} 已准备好，点 Install 开始。`:m.version==='0.4.7-rc5'?`混响降噪候选 ${m.version} 已准备好，保留约 6 dB 输出增益；首次试音请先调低 MASTER，实机声音待验收。`:m.version==='0.4.7-rc4'?`音量修复候选 ${m.version} 已准备好，输出提高约 6 dB；首次试音请先调低 MASTER。`:m.version==='0.4.7-rc3'?`性能修复候选 ${m.version} 已准备好，声音尚待实机验收。`:`候选 ${m.version} 已准备好；部分音色仍有爆音反馈。`);
  }catch(e){button.textContent='重新读取固件';button.disabled=false;say(errorText(e),'error');log(e.message);}
}
button.addEventListener('click',async()=>{
  if(busy)return;
  if(!verified){button.disabled=true;await prepare();return;}
  busy=true;button.disabled=true;button.textContent='安装中…';bar.hidden=false;bar.removeAttribute('value');stage(0);
  try{
    say('请在浏览器提示中允许 MIDI / SysEx 访问。');
    let access;
    try{access=await navigator.requestMIDIAccess({sysex:true});}catch(e){throw Object.assign(new Error(e.message),{code:'denied'});}
    await installFirmware(verified,access,step);
  }catch(e){bar.hidden=true;say(errorText(e),'error');log((e.code??'error')+': '+e.message);}
  finally{busy=false;button.disabled=false;button.textContent='Install '+verified.meta.version;}
});
addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='固件正在安装，请不要关闭页面。';}});
prepare();
