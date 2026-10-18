import {Synth} from '../vendor/openifw/dsp/core.js';
import {modulationDestinations,destinationKey} from '../vendor/openifw/dsp/modulation.js';
import {writeFileSync,mkdirSync} from 'node:fs';
const patch={parameters:{},fx:[],settings:{}};
function buffers(object,path='effects') {
  const result=[];
  for(const [key,value] of Object.entries(object)) {
    const name=`${path}.${key}`;
    if(ArrayBuffer.isView(value)) result.push({name,bytes:value.byteLength});
    else if(value && typeof value==='object') result.push(...buffers(value,name));
  }
  return result;
}
const audits=[44100,48000].map(sampleRate=>{
  const s=new Synth(sampleRate,patch),allocations=buffers(s.effects);
  return {sampleRate,allocations,totalEffectArrayBytes:allocations.reduce((n,r)=>n+r.bytes,0),
    delayPairBytes:s.effects.delays.reduce((n,d)=>n+d.data.byteLength,0),
    dinghyPoolBytes:336*1024};
});
const destinations=modulationDestinations.map(label=>({label,key:destinationKey(label),
  behavior:destinationKey(label)==='Osc4NoiseLoFi'?'Accepted upstream, not consumed by core.js; preserve no-op':'active'}));
mkdirSync('build',{recursive:true});
writeFileSync('build/memory-audit.json',JSON.stringify(audits,null,2)+'\n');
writeFileSync('docs/modulation-destinations.json',JSON.stringify(destinations,null,2)+'\n');
console.log(JSON.stringify(audits.map(({allocations,...a})=>a),null,2));
