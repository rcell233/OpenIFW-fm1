// SPDX-License-Identifier: GPL-3.0-only
// Exercise the real button levels, clock, parameter bridge and NOR storage.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
const pages=JSON.parse(readFileSync('docs/ui-pages.json'));
const module=await WebAssembly.compile(readFileSync('build/web/openifw.wasm'));
const O='build/ui';mkdirSync(O,{recursive:true});
let checks=0;const operations=[];const eq=(a,b,m)=>{assert.deepEqual(a,b,m);checks++;};
const id=k=>pages.findIndex(p=>p.key===k);
async function make(nor,record=false){const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();if(nor)new Uint8Array(e.memory.buffer,e.web_nor(),4096).set(nor);e.web_boot();if(!record)return e;const codes={web_keys:0,web_buttons:1,web_enc:2,web_midi:3,web_master:4,web_render:6};return new Proxy({}, {get(_,k){const value=e[k];return typeof value==='function'&&k in codes?(...args)=>{operations.push([codes[k],...args,0,0,0].slice(0,4));return value(...args);}:value;}});}
function advance(e,ms){const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);}
function tap(e,b,gap=260){e.web_buttons(1<<b);advance(e,18);e.web_buttons(0);advance(e,18);if(gap)advance(e,gap);}
function hold(e,b,ms=550){e.web_buttons(1<<b);advance(e,ms);}
function release(e){e.web_buttons(0);advance(e,24);}
function enc(e,k,d){e.web_enc(k,d);advance(e,12);}
const patch=e=>Buffer.from(new Uint8Array(e.memory.buffer,e.web_patch(),916));
const words=e=>new Int32Array(e.memory.buffer,e.web_patch(),229);
function str(e,p){const b=new Uint8Array(e.memory.buffer);let s='';while(b[p])s+=String.fromCharCode(b[p++]);return s;}
const screenNames=[];
function shot(e,name){let turns=0;while(e.web_ui_draw_pending()&&turns++<32)e.web_render(256);assert.ok(!e.web_ui_draw_pending(),'display pass did not complete');const b=Buffer.alloc(240*240*3),fb=new Uint16Array(e.memory.buffer,e.web_screen(),57600);for(let i=0;i<fb.length;i++){const v=fb[i],p=(v>>8|v<<8)&65535;b[i*3]=((p>>11)&31)*255/31;b[i*3+1]=((p>>5)&63)*255/63;b[i*3+2]=(p&31)*255/31;}writeFileSync(`${O}/${name}.ppm`,Buffer.concat([Buffer.from('P6\n240 240\n255\n'),b]));screenNames.push(name);}
const buttons={HOME:6,EDIT:4,ENV:2,LFO:3,MOD:9,FX:0,TUNE:1,AMP:8,GLOBAL:5,SAVE:7};
function go(e,key){const p=pages[id(key)];if(p.group==='ADVANCED'){hold(e,6);release(e);enc(e,1,4);enc(e,2,1);enc(e,3,1);}else if(p.group==='PRESETS'){enc(e,2,e.web_preset()===5?-1:1);}else if(pages[e.web_page()].group!==p.group){tap(e,buttons[p.group]);}
 const list=pages.flatMap((p,i)=>p.group===pages[id(key)].group?[i]:[]),delta=list.indexOf(id(key))-list.indexOf(e.web_page());if(delta)enc(e,0,delta);eq(e.web_page(),id(key),key+' navigation');}

// All named pages have the advertised four controls, including both matrix sources.
const e=await make(null,true);advance(e,40);let stable=patch(e);
for(const p of pages.filter(p=>!['PRESETS','ADVANCED'].includes(p.group))){go(e,p.key);for(let k=0;k<4;k++){const resolve={309:92,310:93,311:94,312:95};eq(e.web_ui_control_id(k),resolve[p.ids[k]]??p.ids[k],p.key+' knob '+k);}eq(patch(e),stable,p.key+' navigation changed sound');shot(e,p.key);}
go(e,'PRESETS');shot(e,'PRESETS');go(e,'ADVANCED');shot(e,'ADVANCED');

// A remembered page returns on the group button; repeat tap / SELECT cycles it.
// Performance controls are temporary diagnostics; navigation restores synth.
const diagnostic=await make();advance(diagnostic,40);go(diagnostic,'PERF');
const diagnosticPatch=patch(diagnostic);eq(diagnostic.web_perf_mode(),0);
eq(str(diagnostic,diagnostic.web_ui_control_value(3)),'N/A','host must not claim device CPU timing');
enc(diagnostic,3,1);eq(diagnostic.web_perf_mode(),1);advance(diagnostic,100);
diagnostic.web_render(1024);
const tone=Array.from(new Float32Array(diagnostic.memory.buffer,diagnostic.web_out_l(),1024));
assert.ok(Math.max(...tone)>.02&&Math.min(...tone)<-.02,'diagnostic sine is missing');checks++;
let crossings=0;for(let i=1;i<tone.length;i++)if(tone[i-1]<=0&&tone[i]>0)crossings++;
assert.ok(crossings>=9&&crossings<=11,'diagnostic sine frequency is not 440 Hz');checks++;
enc(diagnostic,3,1);eq(diagnostic.web_perf_mode(),2);advance(diagnostic,100);diagnostic.web_render(1024);
eq(Array.from(new Float32Array(diagnostic.memory.buffer,diagnostic.web_out_l(),1024)).every(v=>v===0),true,'diagnostic mute');
eq(patch(diagnostic),diagnosticPatch,'diagnostics changed preset');
go(diagnostic,'HOME');eq(diagnostic.web_perf_mode(),0,'leaving diagnostics must restore synth');

go(e,'OSC3TUNE');go(e,'EG2');tap(e,4);eq(e.web_page(),id('OSC3TUNE'));tap(e,4);eq(e.web_page(),id('OSC3ROUTE'));enc(e,1,1);eq(e.web_page(),id('OSC4'));

// The press timestamp must work on even and odd milliseconds, without false holds.
for(let n=0;n<12;n++){go(e,'HOME');tap(e,0);eq(e.web_ui_layer(),0);eq(pages[e.web_page()].group,'FX');advance(e,n%2?7:12);}
go(e,'HOME');stable=patch(e);hold(e,0,450);eq(e.web_ui_layer(),0);advance(e,80);eq(e.web_ui_layer(),1);shot(e,'LAYER-FX');release(e);eq(e.web_ui_layer(),0);eq(patch(e),stable);
tap(e,0,0);tap(e,0,0);eq(e.web_ui_layer(),1);release(e);eq(e.web_ui_layer(),1);tap(e,0);eq(e.web_ui_layer(),0);

// A layer shortcut consumes the key, while ordinary keys still use mono / legato.
hold(e,4);eq(e.web_ui_layer(),4);e.web_keys(1<<2);advance(e,20);eq(e.web_held(),0);eq(e.web_page(),id('OSC2'));e.web_keys(0);release(e);
e.web_keys((1<<7)|(1<<14));advance(e,20);eq(e.web_note(),67);eq(e.web_held(),2);e.web_keys(1<<7);advance(e,20);eq(e.web_note(),60);e.web_keys(0);advance(e,20);

// Delay 1 / 2, local + global bypass and alias values target only the displayed field.
go(e,'DELAY1');stable=patch(e);enc(e,3,1);let after=patch(e);for(let i=0;i<229;i++)if(i!==193)eq(after.readInt32LE(i*4),stable.readInt32LE(i*4),'delay 1 isolation');assert.notEqual(after.readInt32LE(193*4),stable.readInt32LE(193*4));checks++;
go(e,'DELAY2');stable=patch(e);enc(e,3,1);after=patch(e);for(let i=0;i<229;i++)if(i!==207)eq(after.readInt32LE(i*4),stable.readInt32LE(i*4),'delay 2 isolation');
go(e,'PHASER');enc(e,6,1);eq(words(e)[156],1);enc(e,6,-1);eq(words(e)[156],0);eq(words(e)[228]&1,0);
go(e,'MOD');enc(e,3,3);eq(e.web_ui_slot(),3);stable=patch(e);enc(e,4,1);after=patch(e);for(let i=0;i<229;i++)if(i!==104)eq(after.readInt32LE(i*4),stable.readInt32LE(i*4),'matrix slot isolation');
// All destination aliases can be crossed in both directions without a dead detent.
enc(e,5,1000);let previous=words(e)[106],turns=0;while(previous!==-1&&turns<60){enc(e,5,-1);const v=words(e)[106];assert.notEqual(v,previous,'backwards destination got stuck on alias');checks++;previous=v;turns++;}eq(previous,-1);
tap(e,9);eq(e.web_page(),id('MOD2'));eq(e.web_ui_control_id(1),105);enc(e,4,1);shot(e,'MATRIX-SECOND-SOURCE');

// The menu opens only on hold. Its palette and tabs do not alter the patch.
go(e,'HOME');stable=patch(e);hold(e,6);eq(e.web_ui_menu(),1);release(e);
for(let t=0;t<5;t++){if(t)enc(e,1,1);shot(e,'MENU-'+t);}enc(e,1,-4);enc(e,3,1);eq(patch(e),stable);shot(e,'MENU-GREEN');
enc(e,1,3);enc(e,2,3);enc(e,3,1);eq(e.web_ui_confirm(),4);shot(e,'MENU-CONFIRM');tap(e,12);eq(e.web_ui_confirm(),0);tap(e,6);eq(e.web_ui_menu(),0);

// Save is explicit, quiet gated, and restores the exact user patch through CRC boot.
const s=await make();advance(s,40);go(s,'HOME');enc(s,3,2);stable=patch(s);s.web_keys(1<<7);advance(s,40);tap(s,7);eq(s.web_page(),id('SAVE'));eq(s.web_flash_writes_count(),0);tap(s,13);eq(s.web_ui_naming(),1);shot(s,'NAME');s.web_keys(0);advance(s,20);s.web_keys(1);advance(s,20);s.web_keys(0);advance(s,20);eq(s.web_held(),0);s.web_midi(0x90,60,100);advance(s,20);tap(s,13);eq(s.web_ui_naming(),0);eq(s.web_flash_writes_count(),0,'save wrote while sounding');s.web_midi(0x80,60,0);s.web_keys(0);advance(s,12000);assert.ok(s.web_flash_writes_count()>0);checks++;eq(s.web_preset(),6);
const nor=new Uint8Array(s.memory.buffer,s.web_nor(),4096).slice(),r=await make(nor);advance(r,40);enc(r,2,6);eq(r.web_preset(),6);eq(patch(r),stable);shot(r,'USER-RESTORED');
// Overwrite / load / erase cancellation keeps both stored and edited data intact.
go(r,'HOME');enc(r,3,1);const edited=patch(r);tap(r,7);tap(r,13);eq(r.web_ui_confirm(),3);shot(r,'OVERWRITE');tap(r,12);eq(r.web_ui_confirm(),0);eq(patch(r),edited);enc(r,4,1);tap(r,13);eq(r.web_ui_confirm(),1);tap(r,12);eq(patch(r),edited);tap(r,13);tap(r,13);eq(patch(r),stable);hold(r,7);release(r);eq(patch(r),edited,'undo load');hold(r,7);release(r);eq(patch(r),stable,'redo load');
enc(r,5,1);tap(r,13);eq(r.web_ui_confirm(),2);tap(r,13);advance(r,2000);eq(patch(r),stable,'erase altered audible patch');enc(r,2,-6);enc(r,2,6);eq(r.web_preset(),5,'erased slot remains in browser list');
// A corrupt patch CRC is rejected even when UI preferences remain valid.
const bad=nor.slice();bad[200]^=1;const corrupt=await make(bad);advance(corrupt,40);enc(corrupt,2,6);eq(corrupt.web_preset(),5);
// Favorite filters do not accidentally load a sound while the list is empty.
const f=await make();advance(f,40);enc(f,2,1);enc(f,5,1);enc(f,6,1);stable=patch(f);enc(f,2,4);eq(f.web_preset(),1);eq(patch(f),stable);shot(f,'FAVORITES');
const input=Buffer.alloc(operations.length*16);operations.forEach((o,i)=>o.forEach((v,j)=>input.writeUInt32LE(v>>>0,i*16+j*4)));writeFileSync(`${O}/input.bin`,input);
writeFileSync(`${O}/results.json`,JSON.stringify({checks,pages:pages.length,screens:screenNames,recordedNativeOperations:operations.length,realFirmware:true,shortHoldDoubleTap:true,menu:true,fxIsolation:true,matrixAliases:true,saveCancelUndoCrc:true,browserClickQA:false},null,2)+'\n');
console.log(`PASS UI: ${checks} checks; ${pages.length} mapped pages; tap/hold/double tap, FX and matrix isolation, menu, save/cancel/undo/CRC.`);
