// SPDX-License-Identifier: GPL-3.0-only
// Capture actual firmware UI pixels, not concept artwork.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
const [wasm='build/web/openifw.wasm',out='build/ui-refresh/screens']=process.argv.slice(2);
const module=await WebAssembly.compile(readFileSync(wasm));mkdirSync(out,{recursive:true});
const pages=JSON.parse(readFileSync('docs/ui-pages.json'));
const groups={HOME:6,EDIT:4,ENV:2,LFO:3,MOD:9,FX:0,TUNE:1,AMP:8,GLOBAL:5,SAVE:7};
function advance(e,ms){const until=e.web_now_ms()+ms;while(e.web_now_ms()<until)e.web_render(256);}
function press(e,b,ms=24){e.web_buttons(1<<b);advance(e,ms);e.web_buttons(0);advance(e,280);}
function enc(e,k,v){e.web_enc(k,v);advance(e,40);}
async function make(){const e=(await WebAssembly.instantiate(module,{})).exports;e.web_nor_erase();e.web_boot();advance(e,70);return e;}
function page(e,key){const id=pages.findIndex(p=>p.key===key),group=pages[id].group;
 if(pages[e.web_page()].group!==group)press(e,groups[group]);
 const list=pages.flatMap((p,i)=>p.group===group?[i]:[]);enc(e,0,list.indexOf(id)-list.indexOf(e.web_page()));
}
function capture(e,name){
 const pcm=new Float32Array(256); // Keep the held note active during real paints.
 for(let i=0;e.web_ui_draw_pending()&&i<30;i++)e.web_render(pcm.length);
 const fb=new Uint16Array(e.memory.buffer,e.web_screen(),57600),rgb=Buffer.alloc(57600*3);
 for(let i=0;i<fb.length;i++){const p=((fb[i]>>8)|(fb[i]<<8))&65535;rgb[3*i]=(p>>11)*255/31;rgb[3*i+1]=((p>>5)&63)*255/63;rgb[3*i+2]=(p&31)*255/31;}
 writeFileSync(`${out}/${name}.ppm`,Buffer.concat([Buffer.from('P6\n240 240\n255\n'),rgb]));
}
for(const key of ['HOME','OSC1','EG1','REVERB','MOD','LFO1','SAVE']) {
 const e=await make();page(e,key);e.web_midi(0x90,60,110);advance(e,500);
 if(key==='OSC1')enc(e,3,1);
 capture(e,key);
}
const presets=await make();enc(presets,2,3);capture(presets,'PRESETS');
const menu=await make();press(menu,6,550);enc(menu,1,2);enc(menu,2,4);capture(menu,'MENU');
for(const color of [1,2,3]) {
 const e=await make();press(e,6,550);enc(e,3,color);press(e,6);e.web_midi(0x90,60,110);advance(e,500);capture(e,'COLOR-'+color);
}
console.log('Captured firmware UI:',out);
