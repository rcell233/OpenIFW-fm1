// SPDX-License-Identifier: GPL-3.0-only
// Isolate the panel's persistent global octave from preset/DSP differences.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import assert from 'node:assert/strict';
const e=(await WebAssembly.instantiate(readFileSync('build/web/openifw.wasm'),{})).instance.exports;
e.web_nor_erase();e.web_boot();
const advance=ms=>{const end=e.web_now_ms()+ms;while(e.web_now_ms()<end)e.web_render(256);};
advance(40);e.web_keys(1<<7);advance(40);assert.equal(e.web_note(),60);e.web_keys(0);advance(40);
for(let i=0;i<3;i++){e.web_buttons(1<<12);advance(20);e.web_buttons(0);advance(300);}
const notes=[];
for(let i=0;i<6;i++){
  if(i){e.web_enc(2,1);advance(30);}
  e.web_keys(1<<7);advance(40);notes.push(e.web_note());e.web_keys(0);advance(40);
}
assert.deepEqual(notes,Array(6).fill(24));
e.web_midi(0x90,60,100);advance(40);assert.equal(e.web_note(),60);
const result={kind:'configuration-isolation-not-a-bug',defaultPhysicalC4:60,
  physicalC4AfterOctaveMinusThree:notes,midiC4AfterOctaveMinusThree:e.web_note(),hardwareSettingsRead:false};
mkdirSync('build/target-audit',{recursive:true});
writeFileSync('build/target-audit/octave-config.json',JSON.stringify(result,null,2)+'\n');
console.log('PASS panel octave: shared across six presets; incoming MIDI note is unchanged');
