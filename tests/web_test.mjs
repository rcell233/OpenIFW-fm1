// SPDX-License-Identifier: GPL-3.0-only
// The executable firmware, rather than a second JS synthesizer, is the test subject.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const read=p=>JSON.parse(readFileSync(p));
const bytes=readFileSync('build/web/openifw.wasm'),module=await WebAssembly.compile(bytes);
assert.deepEqual(WebAssembly.Module.imports(module),[],'firmware must not depend on emulated syscalls');
const make=async()=>{const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();e.web_boot();return e;};
const e=await make(),ops=[],chunks=[],snapshots=[];
function call(type,a=0,b=0,c=0){ops.push([type,a,b,c]);
  if(type===0)e.web_keys(a);if(type===1)e.web_buttons(a);if(type===2)e.web_enc(a,b);if(type===3)e.web_midi(a,b,c);if(type===4)e.web_master(a);
}
function view(ptr,len){return Buffer.from(e.memory.buffer,ptr,len);}
function render(n=1024){ops.push([5,n,0,0]);e.web_render(n);
  chunks.push(Buffer.from(view(e.web_out_l(),n*4)),Buffer.from(view(e.web_out_r(),n*4)),Buffer.from(view(e.web_screen(),240*240*2)),Buffer.from(view(e.web_patch(),916)));
  const state=[e.web_now_ms(),e.web_preset(),e.web_page(),e.web_home(),e.web_held(),e.web_note(),e.web_lit_buttons(),e.web_lit_keys(),
    e.web_env_stage(0),e.web_env_level(0),e.web_env_stage(1),e.web_env_level(1),e.web_env_stage(2),e.web_env_level(2),e.web_env_stage(3),e.web_env_level(3),e.web_flash_writes_count()];
  const b=Buffer.alloc(state.length*4);state.forEach((v,i)=>b.writeUInt32LE(v>>>0,i*4));chunks.push(b);snapshots.push(state);
}
function wait(ms){for(let n=0;n<Math.ceil(ms*44.1/1024);n++)render();}
function button(bit){call(1,1<<bit);render();call(1,0);render();wait(260);}
render();assert.ok(view(e.web_screen(),115200).some(v=>v),'boot must draw the actual LCD');
call(0,1<<7);render();assert.equal(e.web_note(),60);assert.equal(e.web_held(),1);
call(0,(1<<7)|(1<<11));render();assert.equal(e.web_note(),64);assert.equal(e.web_held(),2);
call(0,(1<<7)|(1<<11)|(1<<14));render();assert.equal(e.web_note(),67);assert.equal(e.web_held(),3);
call(0,(1<<7)|(1<<11));render();assert.equal(e.web_note(),64);
call(0,1<<7);render();assert.equal(e.web_note(),60);call(0,0);render();assert.equal(e.web_held(),0);
const files=['tresure-island.json','primer.json','ai-no-labyrinth.json','daisy-area-2.json','wanna-go-to-hawaii.json','judd-ish-2000.json'];
const spec=read('docs/parameters.json'),panel=read('docs/panel-controls.json'),audit=[];
const canon=v=>String(v).toUpperCase().replace(/\(\+\)/g,'').replace(/[ ()/]/g,'');
let checked=0,matrixFields=0,envFields=0;
for(let i=0;i<files.length;i++){
  if(i){call(2,2,1);render();}assert.equal(e.web_preset(),i);
  const p=read('vendor/openifw/presets/'+files[i]),words=new Int32Array(e.memory.buffer,e.web_patch(),229);
  for(const d of spec){const raw=p.parameters[d.name]??d.default,v=words[d.id];
    if(d.kind==='number')assert.ok(Math.abs(v/65536-Number(raw))<=.5/65536+1e-12,`${p.name}: ${d.name}`);
    else if(d.name.endsWith('Beat')){const[a,b]=String(raw).split('/').map(Number);assert.ok(Math.abs(v/65536-a/b)<=.5/65536);}
    else {const choices=panel.choices[panel.controls[d.id].choice],choice=choices.find(x=>x.value===v);assert.ok(choice,`${d.name}: invalid ${v}`);
      const aliases={'SINE':'SIN','PWM':'SQU','PULSE':'SQU'};assert.equal(canon(choice.label),aliases[canon(raw)]??canon(raw),`${p.name}: ${d.name}`);}
    checked++;if(d.name.startsWith('Mod'))matrixFields++;if(/^Eg\d/.test(d.name))envFields++;
  }
  const keys=['Bypass','Stereo','Speed','Feedback','Lowpass','WetDry','Rate','Depth','Mix','Time','Highpass','Size','HighDamp','Width'];
  p.fx.forEach((fx,k)=>Object.entries(fx.parameters).forEach(([key,v])=>{const j=keys.indexOf(key);assert.ok(j>=0);const a=words[156+k*14+j];if(key==='Bypass')assert.equal(a,Number(v!=='Off'));else assert.ok(Math.abs(a/65536-Number(v))<=.5/65536+1e-12);checked++;}));
  assert.equal(words[226],Number(p.settings.Tuning)*65536);
  // The original ProgramIndex, not the invented sequential demo number, selects it.
  call(3,0xc0,Number(p.settings.ProgramIndex),0);render();assert.equal(e.web_preset(),i);
  call(0,1<<16);for(let k=0;k<4;k++)render();call(0,0);render();
  audit.push({name:p.name,program:Number(p.settings.ProgramIndex),sha256:createHash('sha256').update(readFileSync('vendor/openifw/presets/'+files[i])).digest('hex')});
}
// Each physical mode button reaches the intended firmware parameter group.
const pages=read('docs/ui-pages.json'),pageId=key=>pages.findIndex(p=>p.key===key);
for(const[bit,key]of [[1,'TUNE'],[4,'OSC1'],[2,'EG1'],[3,'LFO1'],[9,'MOD'],[8,'AMP'],[0,'FX'],[5,'GLOBAL']]){button(bit);assert.equal(e.web_page(),pageId(key));assert.equal(e.web_home(),0);}
button(9);const before=Buffer.from(view(e.web_patch(),916));call(2,4,1);render();assert.notDeepEqual(view(e.web_patch(),916),before);assert.equal(e.web_page(),pageId('MOD'));
button(6);assert.equal(e.web_home(),1);
call(3,0x90,60,100);render();call(3,0x90,67,90);render();assert.equal(e.web_held(),2);
call(3,0xb0,64,127);call(3,0x80,67,0);render();assert.equal(e.web_note(),67);
call(3,0xb0,64,0);render();assert.equal(e.web_note(),60);call(3,0xb0,123,0);render();assert.equal(e.web_held(),0);
call(4,0);render();render();call(4,1023);render();
// Native C and WASM receive the same API operations: compare every output byte.
const input=Buffer.alloc(ops.length*16);ops.forEach((o,i)=>o.forEach((v,j)=>input.writeUInt32LE(v>>>0,i*16+j*4)));
const native=spawnSync('build/emu-native',[],{input,maxBuffer:100*1024*1024});assert.equal(native.status,0,String(native.stderr));
const wasm=Buffer.concat(chunks);assert.equal(native.stdout.length,wasm.length);assert.ok(native.stdout.equals(wasm),'native/WASM PCM, framebuffer, patch or state differ');
// A real factory release must settle: catches the former negative-Q15 feedback DC tail.
const t=await make();t.web_master(1023);t.web_keys(1<<16);t.web_render(256);t.web_render(256);t.web_keys(0);
for(let i=0;i<1600;i++)t.web_render(256);
for(let i=0;i<4;i++)assert.equal(t.web_env_stage(i),0);
const tail=new Float32Array(t.memory.buffer,t.web_out_l(),256);assert.ok(Math.max(...tail.map(Math.abs))<2/65536,'factory FX did not fall silent');
// Saved user patch + globals really travel through the simulated NOR sector and CRC boot path.
function tap(x,bit){x.web_buttons(1<<bit);x.web_render(1024);x.web_buttons(0);x.web_render(1024);}
tap(t,7);tap(t,13);assert.equal(t.web_ui_naming(),1);tap(t,13);
for(let i=0;i<100 && !t.web_flash_writes_count();i++)t.web_render(1024);
assert.ok(t.web_flash_writes_count()>0);
const nor=new Uint8Array(t.memory.buffer,t.web_nor(),4096).slice(),reboot=(await WebAssembly.instantiate(module,{})).exports;
reboot.web_nor_erase();new Uint8Array(reboot.memory.buffer,reboot.web_nor(),4096).set(nor);reboot.web_boot();
reboot.web_enc(2,6);reboot.web_render(256);assert.equal(reboot.web_preset(),6);assert.deepEqual(new Uint8Array(reboot.memory.buffer,reboot.web_patch(),916),new Uint8Array(t.memory.buffer,t.web_patch(),916));
// Instantiate and run the real AudioWorklet wrapper, including quick taps and frame publication.
globalThis.currentTime=0;let Processor,registered,messages=[];
globalThis.AudioWorkletProcessor=class{constructor(){this.port={postMessage:m=>messages.push(m)};}};
globalThis.registerProcessor=(name,C)=>{registered=name;Processor=C;};
await import('../build/web/worklet.js');assert.equal(registered,'openifw');
const w=new Processor();await w.onMessage({type:'load',wasm:bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.length),sectors:{},master:700});
assert.ok(messages.some(m=>m.type==='ready'));assert.ok(!messages.some(m=>m.type==='error'));
await w.onMessage({type:'keys',mask:1<<7});await w.onMessage({type:'keys',mask:0});let sounded=false;
for(let i=0;i<100;i++){const out=[new Float32Array(128),new Float32Array(128)];w.process([],[out]);sounded||=out[0].some(v=>v!==0);globalThis.currentTime+=128/44100;}
assert.ok(sounded,'quick tap was lost');assert.equal(w.ex.web_held(),0);assert.ok(messages.some(m=>m.fb?.length===57600));
const html=readFileSync('web/index.html','utf8');assert.match(html,/openifw.wasm/);assert.doesNotMatch(html,/dsp\/core|processor.js|PURE CURRENT/);
mkdirSync('build/emu',{recursive:true});
const result={factoryPrograms:audit,parameterChecks:checked,matrixFields,envFields,equivalence:{operations:ops.length,renders:snapshots.length,bytesCompared:wasm.length,pcm:true,lcd:true,patch:true,musicalState:true},flashReboot:true,quickTapWorklet:true,browserUIQA:false};
writeFileSync('build/emu/results.json',JSON.stringify(result,null,2)+'\n');
console.log(`PASS firmware WASM: ${checked} factory values, ${matrixFields} matrix and ${envFields} envelope fields; ${wasm.length} native/WASM bytes identical; physical modes, mono/legato, sustain, real flash reboot and AudioWorklet taps.`);
