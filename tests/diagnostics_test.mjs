// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fields,decode,readSnapshot,bootFields,decodeBoot,readBootSnapshot} from '../web/diagnostics/protocol.js';
mkdirSync('build/target-audit',{recursive:true});
const source=`#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "diagnostics_protocol.h"
int main(void){
 uint8_t req[]={0xf0,0x7d,'I','F','W',1,0,127,0xf7};
 assert(ifw_diag_sequence(req,9)==127);
 for(unsigned n=0;n<9;n++)assert(ifw_diag_sequence(req,n)==-1);
 for(unsigned i=0;i<9;i++)if(i!=7){req[i]^=1;assert(ifw_diag_sequence(req,9)==-1);req[i]^=1;}
 uint32_t words[IFW_DIAG_WORDS];for(unsigned i=0;i<IFW_DIAG_WORDS;i++)words[i]=i*0x9821937u;
 words[0]=1;words[2]=UINT32_MAX;uint8_t b[IFW_DIAG_BYTES+2];b[0]=b[IFW_DIAG_BYTES+1]=0x55;
 ifw_diag_encode(b+1,127,words);assert(b[0]==0x55 && b[IFW_DIAG_BYTES+1]==0x55);
 printf("[[");for(unsigned i=0;i<IFW_DIAG_BYTES;i++)printf("%s%u",i?",":"",b[i+1]);printf("],[");
 req[6]=2;assert(ifw_diag_sequence(req,9)==127);
 ifw_diag_encode_words(b+1,127,3,IFW_BOOT_WORDS,words);
 for(unsigned i=0;i<IFW_BOOT_BYTES;i++){printf("%s%u",i?",":"",b[i+1]);}puts("]]");return 0;
}`;
writeFileSync('build/target-audit/diagnostics_fixture.c',source);
execFileSync(process.env.CC||'cc',['-O2','-std=c11','-Wall','-Wextra','-Werror','-Ifirmware/src','build/target-audit/diagnostics_fixture.c','-o','build/target-audit/diagnostics-fixture']);
const [bytes,bootBytes]=JSON.parse(execFileSync('build/target-audit/diagnostics-fixture',{encoding:'utf8'})),gold=decode(bytes,127);
const bootGold=decodeBoot(bootBytes,127);
bootFields.forEach((key,i)=>assert.equal(bootGold[key],i===0?1:i===2?0xffffffff:Math.imul(i,0x9821937)>>>0));
assert.equal(decodeBoot(bytes,127),null);assert.equal(decode(bootBytes,127),null);
assert.equal(gold.uptimeMs,0xffffffff);fields.forEach((key,i)=>assert.equal(gold[key],i===0?1:i===2?0xffffffff:Math.imul(i,0x9821937)>>>0));
for(let i=0;i<bytes.length;i++){const b=[...bytes];b[i]=128;assert.equal(decode(b,127),null);}
assert.equal(decode(bytes,126),null);assert.equal(decode(bytes.slice(1),127),null);
class Input extends EventTarget {emit(data){const e=new Event('midimessage');e.data=data;this.dispatchEvent(e);}}
const input=new Input(),output={send(request){assert.deepEqual(request,[0xf0,0x7d,73,70,87,1,0,127,0xf7]);queueMicrotask(()=>input.emit(bytes));}};
assert.deepEqual(await readSnapshot(input,output,127),gold);
assert.deepEqual(await readBootSnapshot(input,{send(request){assert.deepEqual(request,[240,125,73,70,87,1,2,127,247]);queueMicrotask(()=>input.emit(bootBytes));}},127),bootGold);
await assert.rejects(readSnapshot(input,{send(){}},0,5),/设备没有返回性能数据/);
await assert.rejects(readSnapshot(input,{send(){throw Error('send failed');}},0),/send failed/);
const sourceUsb=readFileSync('firmware/src/usb.c','utf8');assert.match(sourceUsb,/ifw_diag_sequence\(usb.sysex,usb.sx_len\)/);
console.log('PASS read-only diagnostics: C-to-JS 32-word uint32 roundtrip, malformed frames, request sequence, timeout, send failure');
