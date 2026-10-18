// Original test patches; no STUDIO-R factory preset parameters are bundled.
export function patch(parameters = {}, fx = []) {
  return {name: 'OpenIFW-FM1 synthetic test', parameters, settings: {}, fx};
}
export const sine = () => patch({
  Osc1TriLevel: 10, Osc1Waveform2: 'SIN', Osc1Output1: 'On',
  Filter1Frequency: 10, Filter1Type: 'LPF12', MasterLevel: 8,
  ExciterFrequency: 10, GateCloseSmoothTime: 15,
});
export function dense() {
  const p = sine();
  Object.assign(p.parameters, {
    Filter1Frequency: 5.2, Filter1Resonance: 3.5, Filter1KeyTrack: 3,
    Filter1Breath: 1.4, Filter1BreathSubMod: 'BRxEG2',
    Filter2Type: 'BPF12', Filter2Frequency: 6.1, Filter2Resonance: 1,
    Filter3Enabled: 'On', Filter3Stage: 14, Filter3Mix: -2.3,
    Filter3Frequency: 7.3, Filter3Resonance: 3,
    Amp1BreathSubMod: 'BRxEG1', Amp2Level: 7, Amp2Breath: 4,
    Eg1Attack: 1.4, Eg1Decay: 3.2, Eg1Sustain: 7.5, Eg1Release: 4.2,
    Eg2Attack: 2.8, Eg2Decay: 2.5, Eg2Sustain: 6.3, Eg2Release: 3,
    Eg3Retrigger: 'On', Eg3Attack: 2, Eg3Sustain: 5,
    Eg4Attack: 1, Eg4Release: 5,
    Lfo1Waveform: 'SIN', Lfo1Speed: 6.3, Lfo1KeySync: 'On',
    Lfo2Waveform: 'SQU', Lfo2Speed: 3.2,
    ExciterFrequency: 7.4, ExciterMix: 2.5,
    EnhancerMix: 1.7, EnhancerDelayL: 8.5, EnhancerDelayR: 13.2,
    EqLowGain: 2, EqHighGain: -1.2, PitchBendRange: 12,
  });
  for (let i=1;i<=4;i++) Object.assign(p.parameters, {
    [`Osc${i}Semi`]: [0,12,19,-12][i-1], [`Osc${i}Tune`]: [1.3,-2.7,4.4,0][i-1],
    [`Osc${i}SawLevel`]: 1.5, [`Osc${i}TriLevel`]: 1.2,
    [`Osc${i}PwmLevel`]: .8, [`Osc${i}PwmWidth`]: 1+i*1.7,
    [`Osc${i}Waveform2`]: ['SIN','TRI','SAW','SQU'][i-1],
    [`Osc${i}Output1`]: i%2?'On':'Off', [`Osc${i}Output2`]: i%2?'Off':'On',
    [`Osc${i}Sync`]: i===1?'Off':'On',
  });
  p.parameters.Osc4NoiseLevel = .7;
  p.parameters.Osc4NoiseLoFi = 2.4;
  return p;
}
export const notes = [
  {time:.002,type:'noteOn',note:60,velocity:.78},
  {time:.055,type:'noteOn',note:67,velocity:.91},
  {time:.073,type:'noteOn',note:67,velocity:.63},
  {time:.105,type:'noteOff',note:67},
  {time:.123,type:'noteOn',note:72,velocity:.84},
  {time:.153,type:'noteOff',note:60},
  {time:.18,type:'noteOff',note:72},
];
export const expression = [
  ...notes, {time:.01,type:'breath',value:.35}, {time:.013,type:'breath',value:.35},
  {time:.035,type:'breath',value:.9}, {time:.09,type:'bend',value:.46},
  {time:.08,type:'vibrato',value:.7}, {time:.11,type:'glide',value:.8},
  {time:.14,type:'bend',value:-.3}, {time:.19,type:'breath',value:.6},
];
const c = (id,p,events=expression,extra={}) => ({id,patch:p,events,duration:.32,sampleRate:44100,dry:true,breath:.8,...extra});
export function cases() {
  const result = [c('empty-defaults',patch({})),
    c('sine-440',sine(),[{time:.002,type:'noteOn',note:69},{time:.25,type:'noteOff',note:69}],{duration:.36,breath:1}),
    c('dense-parallel2',dense()),
  ];
  for (const type of ['SIN','SINE','TRI','SAW','SQU','PWM','PULSE','Ext In']) {
    const p=sine(); p.parameters.Osc1Waveform2=type;
    result.push(c(`wave-${type.replaceAll(' ','-')}`,p,notes));
  }
  for (const route of ['Serial','Parallel 1','Parallel 2']) for(const type of ['LPF24','LPF12','HPF12','BPF12']) {
    const p=dense(); Object.assign(p.parameters,{FilterConnection:route,Filter1Type:type,Filter2Type:type});
    result.push(c(`filter-${route.replaceAll(' ','')}-${type}`,p));
  }
  for(const width of [0,5,10]) {
    const p=sine(); Object.assign(p.parameters,{Osc1TriLevel:0,Osc1PwmLevel:10,Osc1PwmWidth:width});
    result.push(c(`pwm-width-${width}`,p,notes));
  }
  for(const mode of ['Time','Rate']) {
    const p=dense(); Object.assign(p.parameters,{GlideMidiEnabled:'On',GlideMode:mode,GlideSpeed:5.2,GlideMinimumSpeed:.2});
    result.push(c(`legato-glide-${mode}`,p));
  }
  const matrix=dense();
  const routes=[['BREATH','EG 1','EG 2 Rate',1.6],['LFO 1 (+-)','None','OSC 2 Pitch',.7],
    ['OSC 2 TRI (+-)','None','OSC 3 FM',1.9],['VELOCITY (+-)','BREATH','AMP 1 Level',.7],
    ['NOTE (+-)','None','Filter 3 Frequency',.8],['EG 3','None','LFO 2 Rate',1.3],
    ['PITCH BEND','None','Filter 1 Frequency',.6],['VIBRATO','GLIDE','Exciter Mix',.4]];
  routes.forEach(([a,b,d,depth],i)=>Object.assign(matrix.parameters,{
    [`Mod${i+1}Source1`]:a,[`Mod${i+1}Source2`]:b,[`Mod${i+1}Destination`]:d,[`Mod${i+1}Depth`]:depth}));
  result.push(c('matrix-eight-slots',matrix));
  const fx=[
    {name:'IFW Phaser',parameters:{Bypass:'Off',Stereo:2.5,Speed:1.3,Feedback:4,Lowpass:4,WetDry:3}},
    {name:'IFW Chorus',parameters:{Bypass:'Off',Rate:.4,Depth:7,Mix:6}},
    {name:'IFW Delay1',parameters:{Bypass:'Off',Time:.041,Depth:3.7,Lowpass:4500,Highpass:80,WetDry:3}},
    {name:'IFW Delay2',parameters:{Bypass:'Off',Time:.063,Depth:2.4,Lowpass:2300,Highpass:40,WetDry:2}},
    {name:'IFW Reverb',parameters:{Bypass:'Off',Size:6.4,HighDamp:3,Width:7,WetDry:2}},
  ];
  for(let i=0;i<5;i++) { const p=dense(); p.fx=structuredClone(fx);
    p.fx.forEach((f,j)=>f.parameters.Bypass=i===j?'Off':'On'); result.push(c(`fx-isolated-${i}`,p,expression,{dry:false})); }
  const all=dense(); all.fx=fx; all.modelTuning={chorusPhase:.42};
  result.push(c('fx-full-chain',all,expression,{dry:false,duration:.6}));
  result.push(c('panic-with-tail',all,[...notes,{time:.2,type:'panic'},{time:.28,type:'noteOn',note:64}],{dry:false,duration:.45}));
  const bypass=structuredClone(all); bypass.settings.FxBypassChorus='1'; bypass.settings.FxBypassReverb='1';
  result.push(c('fx-setting-bypass',bypass,expression,{dry:false}));
  for(const sr of [48000,96000]) result.push(c(`sample-rate-${sr}`,matrix,expression,{sampleRate:sr}));
  const noise=sine(); Object.assign(noise.parameters,{Osc1Output1:'Off',Osc4Output1:'On',Osc4NoiseLevel:5,Osc4NoiseLoFi:3.7});
  result.push(c('noise-seeded',noise));
  const silent=sine(); Object.assign(silent.parameters,{
    Osc1Output1:'Off',Osc2Waveform2:'SIN',Osc2Semi:12,Osc2TriLevel:0,
    Osc3Output1:'On',Osc3PwmLevel:3.1,Osc3PwmWidth:5,Osc3Semi:24,Osc3Sync:'On',
    Mod1Source1:'OSC 2 TRI (+-)',Mod1Destination:'OSC 3 FM',Mod1Depth:1.9,
  });
  result.push(c('silent-osc-fm',silent,notes));
  const nofm=structuredClone(silent); nofm.parameters.Mod1Depth=0; result.push(c('silent-osc-no-fm',nofm,notes));
  const chain=structuredClone(silent); Object.assign(chain.parameters,{
    Osc3Output1:'Off',Osc4Output1:'On',Osc4SawLevel:4.4,Osc4Semi:36,Osc4Sync:'On',Osc2Sync:'On'});
  result.push(c('sync-clock-fm',chain,notes));
  const nochain=structuredClone(chain); nochain.parameters.Mod1Depth=0; result.push(c('sync-clock-no-fm',nochain,notes));
  const brEg=sine(); Object.assign(brEg.parameters,{Amp1BreathSubMod:'BRxEG1',Eg1Release:5,Eg1Retrigger:'Off',Eg2Retrigger:'On',Eg2Attack:5});
  result.push(c('br-eg-release',brEg,notes,{duration:.5}));
  const bfs=sine(); Object.assign(bfs.parameters,{Lfo1BPMSync:'On',Lfo1Beat:'1/8',Lfo2BpmSync:'On',Lfo2Beat:'3/16'});
  result.push(c('lfo-both-bpm-spellings',bfs));
  const tuning=sine(); tuning.settings.Tuning=432; Object.assign(tuning.parameters,{MasterTune:12,Transpose:-5});
  result.push(c('tuning-transpose',tuning,notes));
  const alias=dense(); Object.assign(alias.parameters,{Mod1Source1:'LFO 1 (+-)',Mod1Destination:'  osc   1   wave level  ',Mod1Depth:1.3,
    Mod2Source1:'EG 1',Mod2Destination:'OSC 2 PWM Phase',Mod2Depth:2});
  result.push(c('destination-aliases',alias));
  result.push(c('live-edit-preserves-legato',matrix,[...expression,
    {time:.06,type:'edit',key:'Osc1Semi',value:7}, {time:.08,type:'edit',key:'Eg1Attack',value:4.2},
    {time:.115,type:'edit',key:'Mod2Destination',value:'OSC 2 Wave Level'},
    {time:.13,type:'edit',key:'FilterConnection',value:'Serial'},
  ]));
  const stack=sine(); const stackEvents=Array.from({length:128},(_,note)=>({time:.002,type:'noteOn',note,velocity:.5}));
  for(let note=127;note>=0;note--) stackEvents.push({time:.005+(127-note)*.0001,type:'noteOff',note});
  result.push(c('all-128-held-notes',stack,stackEvents,{duration:.04}));
  return result;
}
