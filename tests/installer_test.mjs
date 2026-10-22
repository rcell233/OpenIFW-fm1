// SPDX-License-Identifier: GPL-3.0-only
// Protocol mock based on Felucca's test_web.mjs, Leo Kuroshita / Hügelton Instruments.
// No connected hardware is probed by these tests.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import vm from 'node:vm';
import {verifyFirmware,loadFirmware,installFirmware,errorText,candidateCount} from '../build/web/install/controller.js';
import {pack7,unpack7} from '../build/web/install/lib/fm1ota.js';
import {sha256hex,Package} from '../build/web/install/lib/fm1pkg.js';
let checks=0;const ok=(v,m)=>{assert.ok(v,m);checks++;};const eq=(a,b,m)=>{assert.deepEqual(a,b,m);checks++;};
const meta=JSON.parse(readFileSync('build/web/install/release.json'));
const bytes=new Uint8Array(readFileSync('build/web/firmware/'+meta.file));
const verified=await verifyFirmware(bytes,meta),pkg=new Package(bytes);
eq(verified.product,'FM-1_982');eq(verified.image.length,bytes.length-20);
const bad=async(p,code)=>{await assert.rejects(p,e=>e.code===code);checks++;};
await bad(verifyFirmware(bytes.slice(0,-1),meta),'badpkg');
const changed=bytes.slice();changed[2000]^=1;await bad(verifyFirmware(changed,meta),'badpkg');
await bad(verifyFirmware(bytes,{...meta,product:'FM-2_982'}),'badpkg');
if(meta.version.includes('-rc')){
 eq(meta.experimental,true);await bad(verifyFirmware(bytes,{...meta,experimental:false}),'badpkg');
 for(const v of ['0.4.6-rc0','0.4.6-rc','../0.4.6'])await bad(verifyFirmware(bytes,{...meta,version:v}),'badpkg');
}

const brokenLoader=bytes.slice(),ota=pkg.entries.find(e=>e.type===100);brokenLoader[ota.dataOff+20+200]^=1;
await bad(verifyFirmware(brokenLoader,{...meta,sha256:await sha256hex(brokenLoader)}),'badpkg');
const addresses=[];
await loadFirmware('https://rcell233.github.io/OpenIFW-fm1/install/install.js',async u=>{
  addresses.push(String(u));return String(u).endsWith('release.json')?new Response(JSON.stringify(meta)):new Response(bytes);
});
eq(addresses,['https://rcell233.github.io/OpenIFW-fm1/install/release.json','https://rcell233.github.io/OpenIFW-fm1/firmware/'+meta.file]);
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response('',{status:404})),'download');
const recoveryMeta=JSON.parse(readFileSync('build/web/install/recovery-0.4.1.json'));
const recoveryBytes=new Uint8Array(readFileSync('build/web/firmware/'+recoveryMeta.file));
const recoveryAddresses=[];
const recovery=await loadFirmware('https://example.test/install/install.js',async u=>{
 recoveryAddresses.push(String(u));return String(u).endsWith('.json')?new Response(JSON.stringify(recoveryMeta)):new Response(recoveryBytes);
},'recovery-0.4.1.json');
eq(recovery.meta.version,'0.4.1');eq(recovery.meta.sha256,'cf7ab1e35378ae7797ab817c7e32aaa8ce0b49d3bbbe55185aa18ded7ae2a9b8');
eq(recoveryAddresses,['https://example.test/install/recovery-0.4.1.json','https://example.test/firmware/OpenIFW-FM1-0.4.1.fwsc']);
const stableMeta=JSON.parse(readFileSync('build/web/install/recovery-0.4.4.json'));
const stableBytes=new Uint8Array(readFileSync('build/web/firmware/'+stableMeta.file));
const stable=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(stableMeta)):new Response(stableBytes),'recovery-0.4.4.json');
eq(stable.meta.version,'0.4.4');eq(stable.meta.sha256,'39fb0338e9c12394897edd62b19902af1f82b324c03628ee102c7d8c9ec1af8e');
const rc2Meta=JSON.parse(readFileSync('build/web/install/recovery-0.4.6-rc2.json'));
const rc2Bytes=new Uint8Array(readFileSync('build/web/firmware/'+rc2Meta.file));
const rc2=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(rc2Meta)):new Response(rc2Bytes),'recovery-0.4.6-rc2.json');
eq(rc2.meta.version,'0.4.6-rc2');eq(rc2.meta.sha256,'22ba5f9c4e92388c299870de28d50179bc93718034c00da66e5cc3e946d20e9f');
const candidateMeta=JSON.parse(readFileSync('build/web/install/candidate-0.4.7-rc2.json'));
const candidateBytes=new Uint8Array(readFileSync('build/web/firmware/'+candidateMeta.file));
const candidate=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(candidateMeta)):new Response(candidateBytes),'candidate-0.4.7-rc2.json');
eq(candidate.meta.version,'0.4.7-rc2');eq(candidate.meta.experimental,true);
ok(candidate.meta.sha256!==rc2.meta.sha256);eq(meta.version,'0.4.7-rc6','dense-note transition repair is the default');
const repairedMeta=JSON.parse(readFileSync('build/web/install/candidate-0.4.7-rc6.json'));
const repaired=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(repairedMeta)):new Response(bytes),'candidate-0.4.7-rc6.json');
eq(repaired.meta.sha256,meta.sha256);eq(repaired.meta.version,'0.4.7-rc6');
const previousNoiseMeta=JSON.parse(readFileSync('build/web/install/candidate-0.4.7-rc5.json'));
const previousNoiseBytes=new Uint8Array(readFileSync('build/web/firmware/'+previousNoiseMeta.file));
const previousNoise=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(previousNoiseMeta)):new Response(previousNoiseBytes),'candidate-0.4.7-rc5.json');
eq(previousNoise.meta.sha256,'59cb87236c0ca266a8d00143b73f1ffc0320251f797b9ca315e5af3f25bbf5a9');
const noiseRecoveryMeta=JSON.parse(readFileSync('build/web/install/recovery-0.4.7-rc5.json'));
const noiseRecovery=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(noiseRecoveryMeta)):new Response(previousNoiseBytes),'recovery-0.4.7-rc5.json');
eq(noiseRecovery.meta.sha256,previousNoise.meta.sha256);ok(previousNoise.meta.sha256!==meta.sha256);
const previousReverbMeta=JSON.parse(readFileSync('build/web/install/candidate-0.4.7-rc4.json'));
const previousReverbBytes=new Uint8Array(readFileSync('build/web/firmware/'+previousReverbMeta.file));
const previousReverb=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(previousReverbMeta)):new Response(previousReverbBytes),'candidate-0.4.7-rc4.json');
eq(previousReverb.meta.sha256,'661c57a11f60a3824d0f10a205f38c8a35974925ddd82e545fae20b5ebc5918e');
const reverbRecoveryMeta=JSON.parse(readFileSync('build/web/install/recovery-0.4.7-rc4.json'));
const reverbRecovery=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(reverbRecoveryMeta)):new Response(previousReverbBytes),'recovery-0.4.7-rc4.json');
eq(reverbRecovery.meta.sha256,previousReverb.meta.sha256);ok(previousReverb.meta.sha256!==meta.sha256);
const previousGainMeta=JSON.parse(readFileSync('build/web/install/candidate-0.4.7-rc3.json'));
const previousGainBytes=new Uint8Array(readFileSync('build/web/firmware/'+previousGainMeta.file));
const previousGain=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(previousGainMeta)):new Response(previousGainBytes),'candidate-0.4.7-rc3.json');
eq(previousGain.meta.version,'0.4.7-rc3');eq(previousGain.meta.sha256,'e5f1a8cf944debd3b79bd0f2939f7d8a5e3f8ecc0836644d68291234c9f70f7d');
const gainRecoveryMeta=JSON.parse(readFileSync('build/web/install/recovery-0.4.7-rc3.json'));
const gainRecovery=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(gainRecoveryMeta)):new Response(previousGainBytes),'recovery-0.4.7-rc3.json');
eq(gainRecovery.meta.sha256,previousGain.meta.sha256);ok(previousGain.meta.sha256!==meta.sha256);
const previousRecovery=JSON.parse(readFileSync('build/web/install/recovery-0.4.7-rc2.json'));
const previousCandidate=await loadFirmware('https://example.test/install/install.js',async u=>String(u).endsWith('.json')?new Response(JSON.stringify(previousRecovery)):new Response(candidateBytes),'recovery-0.4.7-rc2.json');
eq(previousCandidate.meta.sha256,candidate.meta.sha256);
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response(JSON.stringify(candidateMeta)),'candidate-0.4.7-rc6.json'),'badpkg');
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response(JSON.stringify(rc2Meta)),'candidate-0.4.7-rc2.json'),'badpkg');
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response(JSON.stringify(stableMeta)),'recovery-0.4.6-rc2.json'),'badpkg');
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response(JSON.stringify(recoveryMeta)),'recovery-0.4.4.json'),'badpkg');
await bad(loadFirmware('https://example.test/install/install.js',async()=>{throw Error('must not fetch');},'../other.json'),'badpkg');
await bad(loadFirmware('https://example.test/install/install.js',async()=>new Response(JSON.stringify(meta)),'recovery-0.4.1.json'),'badpkg');

// Virtual device time keeps the unchanged transport's production timeouts intact.
async function clock(work){
  const original={setTimeout,clearTimeout,now:Date.now};let now=0,id=0,timers=new Map(),done=false,value,error;
  globalThis.setTimeout=(f,ms,...args)=>{const n=++id;timers.set(n,{at:now+Number(ms),f:()=>f(...args)});return n;};
  globalThis.clearTimeout=n=>timers.delete(n);Date.now=()=>now;
  try{
    Promise.resolve().then(work).then(v=>{value=v;done=true;},e=>{error=e;done=true;});
    for(let n=0;!done&&n<30000;n++){
      for(let k=0;k<24;k++)await Promise.resolve();
      if(done)break;
      let next=null;for(const t of timers)if(!next||t[1].at<next[1].at)next=t;
      if(!next)throw new Error('simulated install stalled without a timer');
      timers.delete(next[0]);now=next[1].at;next[1].f();
    }
    assert.ok(done,'simulated install did not settle');if(error)throw error;return value;
  }finally{globalThis.setTimeout=original.setTimeout;globalThis.clearTimeout=original.clearTimeout;Date.now=original.now;}
}
const HS=[0xf0,0,0x32,0x45,0,0,0,0x40,0x7f,0xf7],UPGRADE=[0xf0,0x22,0x24,0x35,0x7f,0xf7];
const same=(a,b)=>a.length===b.length&&a.every((v,i)=>v===b[i]);
class Device{
  constructor(options={}){this.opt=options;this.firmware=options.firmware??verified;this.access={inputs:new Map(),outputs:new Map()};this.reads=0;this.bad=0;this.upgrades=0;this.written=0;this.boot(options.identity??'FM-1_015');}
  boot(identity){
    for(const m of [this.access.inputs,this.access.outputs]){for(const p of m.values())p.state='disconnected';m.clear();}
    this.identity=identity;this.pending=null;this.queue=[];
    const name=identity.startsWith('ota-')?'FM-1 Update':'FM-1',
      input={id:'in',name,state:'connected',connection:'closed',onmidimessage:null,async open(){this.connection='open';},async close(){this.connection='closed';}},
      output={id:'out',name,state:'connected',connection:'closed',async open(){this.connection='open';},async close(){this.connection='closed';},send:d=>{if(output.state!=='connected')throw new Error('disconnected');setTimeout(()=>this.receive(Array.from(d)),1);}};
    this.input=input;this.output=output;this.access.inputs.set(input.id,input);this.access.outputs.set(output.id,output);
  }
  emit(bytes){const input=this.input;setTimeout(()=>{if(input.state==='connected'&&input.onmidimessage)input.onmidimessage({data:Uint8Array.from(bytes)});},1);}
  disconnect(){this.input.state=this.output.state='disconnected';}
  receive(data){
    if(same(data,HS)){const t=Array.from(new TextEncoder().encode(this.identity)),b=[0,0x59,0x11,0,0,0,...t,...Array(28-t.length).fill(0)];this.emit([0xf0,...pack7(b),0xf7]);return;}
    if(same(data,UPGRADE)){
      this.upgrades++;this.writing=this.identity.startsWith('ota-');
      this.queue=this.writing?Array.from({length:Math.ceil(this.firmware.image.length/512)},(_,k)=>[k*512,Math.min(512,this.firmware.image.length-k*512)]):[[0,64],[64,160],[this.firmware.image.length-64,64]];
      if(this.opt.badRequest)this.queue.unshift([this.firmware.image.length+1,512]);
      if(!this.opt.stopBeforeFinish)this.queue.push([this.writing?0xf0000000:0xe0000000,8]);
      this.next();return;
    }
    if(!this.pending)return;
    const [addr,len,flag]=this.pending,u=unpack7(data.slice(1,-1));let sum=0;for(let i=6;i<u.length;i++)sum+=u[i];
    const wanted=addr>=0xe0000000?[...new TextEncoder().encode('success'),0]:this.firmware.image.subarray(addr,addr+len);
    if(!same(u.slice(14,-1),wanted)||(sum&255)!==255||u[6]!==(addr>=0xe0000000?0:flag)||u.length!==15+wanted.length)this.bad++;
    this.reads++;if(this.writing&&addr<0xe0000000)this.written+=len;this.pending=null;
    if(this.reads>=this.opt.unplugAfter){this.disconnect();return;}
    if(addr===0xe0000000){this.disconnect();if(!this.opt.noLoader)setTimeout(()=>this.boot('ota-FM-1_982'),300);}
    else if(addr===0xf0000000){this.disconnect();if(!this.opt.noReboot)setTimeout(()=>this.boot(this.opt.returnIdentity??'FM-1_982'),300);}
    else this.next();
  }
  next(){const r=this.queue.shift();if(!r)return;const [a,n]=r,flag=(this.reads%2)?0xa5:0;this.pending=[a,n,flag];const b=[0,0x59,0x30,0,0,0,flag,a&255,a>>>8&255,a>>>16&255,a>>>24&255,n&255,n>>>8,0];let sum=0;for(let i=6;i<14;i++)sum+=b[i];b.push(~sum&255);if(this.opt.corruptRequest&&!this.corruptSent){this.corruptSent=true;const bad=b.slice();bad[14]^=1;this.emit([0xf0,...pack7(bad),0xf7]);}this.emit([0xf0,...pack7(b),0xf7]);}
}
const events=[],dev=new Device({corruptRequest:true});
eq(await clock(()=>installFirmware(verified,dev.access,k=>events.push(k))),'FM-1_982');
eq(dev.bad,0,'every reply must preserve data, flags, packet length and checksum');
eq(dev.written,verified.image.length,'the full real release image was transferred');
eq(events.at(-1),'done');ok(events.includes('verify')&&events.includes('loader')&&events.includes('write')&&events.includes('reboot'));
eq(dev.input.connection,'closed');eq(dev.output.connection,'closed');
const resume=new Device({identity:'ota-FM-1_990'}),resumeEvents=[];
eq(await clock(()=>installFirmware(verified,resume.access,k=>resumeEvents.push(k))),'FM-1_982');ok(resumeEvents.includes('resume'));eq(resume.written,verified.image.length);eq(resume.bad,0);
// USB RESCUE identifies as FM-1_000 and uses the same complete update path.
const rescue=new Device({identity:'FM-1_000'}),rescueEvents=[];
eq(await clock(()=>installFirmware(verified,rescue.access,k=>rescueEvents.push(k))),'FM-1_982');
eq(rescue.written,verified.image.length);eq(rescue.bad,0);eq(rescueEvents.at(-1),'done');
const rollbackDevice=new Device({identity:'FM-1_000',firmware:recovery});
eq(await clock(()=>installFirmware(recovery,rollbackDevice.access)),'FM-1_982');
eq(rollbackDevice.written,recovery.image.length);eq(rollbackDevice.bad,0);
const stableDevice=new Device({identity:'FM-1_000',firmware:stable});
eq(await clock(()=>installFirmware(stable,stableDevice.access)),'FM-1_982');
eq(stableDevice.written,stable.image.length);eq(stableDevice.bad,0);
const candidateDevice=new Device({identity:'FM-1_000',firmware:candidate});
eq(await clock(()=>installFirmware(candidate,candidateDevice.access)),'FM-1_982');
eq(candidateDevice.written,candidate.image.length);eq(candidateDevice.bad,0);
for(const identity of ['FM-1_015','ota-FM-1_982']){
 const failed=new Device({identity,returnIdentity:'FM-1_000'}),events=[];
 await bad(clock(()=>installFirmware(verified,failed.access,k=>events.push(k))),'rescue');
 ok(!events.includes('done'),'rescue after reboot must never be accepted as success');
}
for(const [options,code]of [[{identity:'FM-2_015'},'model'],[{identity:'ota-FM-1_015'},'foreign'],[{unplugAfter:1},'lost'],[{unplugAfter:5},'lost'],[{returnIdentity:'FM-1_990'},'mismatch'],[{badRequest:true},'badreq'],[{noLoader:true},'noloader'],[{noReboot:true},'noreturn'],[{identity:'ota-FM-1_982',stopBeforeFinish:true},'stopped']]){
 const d=new Device(options),events=[];await bad(clock(()=>installFirmware(verified,d.access,k=>events.push(k))),code);ok(!events.includes('done'),code+' cannot claim success');if(['model','foreign'].includes(code))eq(d.upgrades,0);
}
await bad(clock(()=>installFirmware(verified,{inputs:new Map(),outputs:new Map()})),'notfound');
const multiple=new Device();multiple.access.inputs.set('other',{name:'FM-1 second',state:'connected'});multiple.access.outputs.set('other',{name:'FM-1 second',state:'connected'});eq(candidateCount(multiple.access),2);await bad(installFirmware(verified,multiple.access),'multiple');eq(multiple.upgrades,0);
ok(errorText({code:'denied'}).includes('权限'));ok(errorText({code:'lost'}).includes('重新连接'));
ok(errorText({code:'rescue'}).includes('FM-1_000'));

// Page lifecycle: loading never asks for MIDI; only the Install click may do so.
async function page({midi=true,denied=false,secure=true,downloadFail=false,recovery:rollbackVersion=null,candidate:selectCandidate=false}={}){
  const elements=new Map(),handlers={},stages=Array.from({length:4},(_,i)=>({dataset:{stage:String(i)},classList:{toggle(){}}}));
  const el=id=>{if(!elements.has(id))elements.set(id,{disabled:id==='install',hidden:true,textContent:'',dataset:{},classList:{toggle(){}},listeners:{},addEventListener(k,f){this.listeners[k]=f;},removeAttribute(k){delete this[k];}});return elements.get(id);};
  let permissions=0,installs=0,allow;
  const state={get permissions(){return permissions;},get installs(){return installs;},el,handlers,finish(){allow?.();}};
  const source=readFileSync('build/web/install/install.js','utf8').replace(/^import .*;\n/m,'').replaceAll('import.meta.url',JSON.stringify('https://rcell233.github.io/OpenIFW-fm1/install/install.js'));
  vm.runInNewContext(source,{document:{getElementById:el,querySelectorAll:()=>stages},navigator:midi?{async requestMIDIAccess(options){permissions++;eq(options.sysex,true);if(denied)throw new DOMException('Permission denied','NotAllowedError');return {};}}:{},isSecureContext:secure,URL,Date,console,
    addEventListener:(k,f)=>handlers[k]=f,errorText,location:{href:'https://example.test/install/'+(rollbackVersion?'?recovery='+rollbackVersion:selectCandidate?'?candidate='+(typeof selectCandidate==='string'?selectCandidate:'0.4.7-rc2'):'')},
    loadFirmware:async(_url,_fetch,release)=>{if(downloadFail)throw Object.assign(new Error('download failed'),{code:'download'});return ['candidate-0.4.7-rc2.json','recovery-0.4.7-rc2.json'].includes(release)?candidate:['candidate-0.4.7-rc3.json','recovery-0.4.7-rc3.json'].includes(release)?previousGain:release==='candidate-0.4.7-rc6.json'?repaired:['candidate-0.4.7-rc5.json','recovery-0.4.7-rc5.json'].includes(release)?previousNoise:['candidate-0.4.7-rc4.json','recovery-0.4.7-rc4.json'].includes(release)?previousReverb:release==='recovery-0.4.6-rc2.json'?rc2:release==='recovery-0.4.4.json'?stable:release==='recovery-0.4.1.json'?recovery:verified;},
    installFirmware:async(_v,_a,step)=>{installs++;step('write',1);await new Promise(r=>allow=r);step('done','FM-1_982');},
  });
  for(let i=0;i<20;i++)await Promise.resolve();return state;
}
const ui=await page();eq(ui.permissions,0);eq(ui.installs,0);eq(ui.el('install').disabled,false);
const installing=ui.el('install').listeners.click();for(let i=0;i<10;i++)await Promise.resolve();eq(ui.permissions,1);eq(ui.installs,1);eq(ui.el('install').disabled,true);
await ui.el('install').listeners.click();eq(ui.permissions,1,'second click must not launch another install');
let prevented=false;ui.handlers.beforeunload({preventDefault(){prevented=true;}});eq(prevented,true);
ui.finish();await installing;eq(ui.el('install').disabled,false);ok(ui.el('status').textContent.includes('安装完成'));
const permission=await page({denied:true});await permission.el('install').listeners.click();eq(permission.installs,0);eq(permission.el('install').disabled,false);ok(permission.el('status').textContent.includes('权限'));
const unsupported=await page({midi:false});eq(unsupported.el('install').disabled,true);eq(unsupported.el('download').hidden,false);
const insecure=await page({secure:false});eq(insecure.el('install').disabled,true);ok(insecure.el('status').textContent.includes('HTTPS'));
const network=await page({downloadFail:true});eq(network.el('install').disabled,false);eq(network.permissions,0);
const previous=await page({recovery:'0.4.4'});eq(previous.el('install').textContent,'Install 0.4.4');eq(previous.permissions,0);eq(previous.el('recovery-note').hidden,false);ok(previous.el('recovery-note').textContent.includes('0.4.4'));
const fluent=await page({recovery:'0.4.6-rc2'});eq(fluent.el('install').textContent,'Install 0.4.6-rc2');eq(fluent.permissions,0);eq(fluent.el('recovery-note').hidden,false);ok(fluent.el('recovery-note').textContent.includes('0.4.6-rc2'));
const optIn=await page({candidate:true});eq(optIn.el('install').textContent,'Install 0.4.7-rc2');eq(optIn.permissions,0);
const current=await page({candidate:'0.4.7-rc6'});eq(current.el('install').textContent,'Install 0.4.7-rc6');eq(current.permissions,0);ok(current.el('status').textContent.includes('密集演奏'));
ok(current.el('status').textContent.includes('实机声音待验收'));
const oldNoise=await page({recovery:'0.4.7-rc5'});eq(oldNoise.el('install').textContent,'Install 0.4.7-rc5');ok(oldNoise.el('recovery-note').textContent.includes('Q23 混响降噪'));
const oldReverb=await page({recovery:'0.4.7-rc4'});eq(oldReverb.el('install').textContent,'Install 0.4.7-rc4');ok(oldReverb.el('recovery-note').textContent.includes('Q15 混响'));
const oldGain=await page({candidate:'0.4.7-rc3'});eq(oldGain.el('install').textContent,'Install 0.4.7-rc3');eq(oldGain.permissions,0);
const oldGainRecovery=await page({recovery:'0.4.7-rc3'});eq(oldGainRecovery.el('install').textContent,'Install 0.4.7-rc3');ok(oldGainRecovery.el('recovery-note').textContent.includes('原输出音量'));
const previousBoot=await page({recovery:'0.4.7-rc2'});eq(previousBoot.el('install').textContent,'Install 0.4.7-rc2');ok(previousBoot.el('recovery-note').textContent.includes('JUDD'));eq(ui.el('install').textContent,'Install 0.4.7-rc6');
mkdirSync('build/installer',{recursive:true});writeFileSync('build/installer/results.json',JSON.stringify({checks,firmwareSha256:meta.sha256,fullImageBytes:verified.image.length,fullInstall:true,candidateFirmwareSha256:candidate.meta.sha256,candidateFullInstall:true,defaultVersion:meta.version,previousCandidateRecovery:true,previousReverbRecovery:true,previousNoiseRecovery:true,resume:true,rescueInstall:true,rescueReturnRejected:true,rollbackFirmwareSha256:recovery.meta.sha256,rollbackFullImageBytes:recovery.image.length,rollbackFullInstall:true,errors:['corrupt package','corrupt loader','wrong model','foreign loader','unplug','mismatch','rescue after reboot','bounds','timeouts','multiple devices','permission denied'],pageLifecycle:true,realHardwareFlashed:false},null,2)+'\n');
console.log(`PASS installer: ${checks} checks; complete ${verified.image.length}-byte USB-MIDI image transfer, resume, error paths and permission UI. No real device was flashed.`);
