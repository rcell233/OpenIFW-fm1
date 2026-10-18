import {readFileSync,writeFileSync} from 'node:fs';
const names=new Map(JSON.parse(readFileSync(new URL('../docs/parameters.json',import.meta.url))).map(p=>[p.name,p]));
export function schedule(c) {
  return c.events.map((e,i)=>({...e,i,frame:Math.max(0,Math.round(e.time*c.sampleRate))})).sort((a,b)=>a.frame-b.frame||a.i-b.i);
}
export function writeScenario(c,path) {
  const total=Math.ceil(c.duration*c.sampleRate),lines=[`sr ${c.sampleRate}`,`frames ${total}`,`dry ${+c.dry}`,`breath ${c.breath}`];
  for(const [key,v] of Object.entries(c.patch.parameters)) {
    if(!names.has(key)) throw new Error(`unknown patch key ${key}`);
    const d=names.get(key);
    if(d.kind==='text') {
      if(typeof v==='number' || typeof v==='boolean') lines.push(`num ${key} ${Number(v)}`);
      else lines.push(`str ${key} ${v}`);
    } else lines.push(`num ${key} ${Number(v)}`);
  }
  c.patch.fx.forEach((f,i)=>Object.entries(f.parameters).forEach(([k,v])=>lines.push(`fx ${i} ${k} ${k==='Bypass'?+(v!=='Off'):Number(v)}`)));
  const settings=c.patch.settings;
  if(settings.Tuning!==undefined) lines.push(`tuning ${Number(settings.Tuning)}`);
  ['Phaser','Chorus','Delay1','Delay2','Reverb'].forEach((name,i)=>{
    const v=settings['FxBypass'+name]; if(v!==undefined) lines.push(`bypass ${i} ${+(v===1||v==='1'||v===true||v==='On')}`);
  });
  if(c.patch.modelTuning?.chorusPhase!==undefined) lines.push(`chorusPhase ${c.patch.modelTuning.chorusPhase}`);
  for(const e of schedule(c)) {
    if(e.type==='edit') lines.push(`${typeof e.value==='string'?'editstr':'editnum'} ${e.frame} ${e.key} ${e.value}`);
    else lines.push(`event ${e.frame} ${e.type} ${e.note ?? e.value ?? 0} ${e.velocity ?? 1}`);
  }
  writeFileSync(path,lines.join('\n')+'\n'); return total;
}
export function writeWav(pcm,sr,path) {
  const b=Buffer.alloc(44+pcm.length*2); b.write('RIFF'); b.writeUInt32LE(b.length-8,4); b.write('WAVEfmt ',8);
  b.writeUInt32LE(16,16); b.writeUInt16LE(1,20); b.writeUInt16LE(2,22); b.writeUInt32LE(sr,24); b.writeUInt32LE(sr*4,28);
  b.writeUInt16LE(4,32); b.writeUInt16LE(16,34); b.write('data',36); b.writeUInt32LE(pcm.length*2,40);
  pcm.forEach((v,i)=>b.writeInt16LE(Math.round(Math.max(-1,Math.min(1,v))*32767),44+i*2)); writeFileSync(path,b);
}
