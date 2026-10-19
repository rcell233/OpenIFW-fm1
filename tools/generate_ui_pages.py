#!/usr/bin/env python3
"""Page layout only. Reads parameter IDs; never writes DSP or factory data."""
from pathlib import Path
import json
R=Path(__file__).resolve().parents[1]
ids={p['name']:p['id'] for p in json.loads((R/'docs/parameters.json').read_text())}
groups=['HOME','EDIT','ENV','LFO','MOD','FX','TUNE','AMP','GLOBAL','SAVE','PRESETS','ADVANCED']
special=dict(PRESET=300,CORE=301,FAV=302,LIST=303,SLOT=304,LOAD=305,ERASE=306,SAVE=307,MODSLOT=308,MODSRC1=309,MODSRC2=310,MODDEST=311,MODDEPTH=312,PERFMODE=313,PERFRESET=314,PERFFREEZE=315,PERFSTATE=316)
pages=[]
def add(key,group,title,params,labels,kind='SCOPE',description=''):
 def ident(p):
  if p is None:return 65535
  if isinstance(p,int):return p
  return ids[p] if p in ids else special[p]
 pages.append(dict(key=key,group=group,title=title,ids=[ident(p) for p in params]+[65535]*(4-len(params)),labels=labels+['']*(4-len(labels)),kind=kind,description=description))
add('HOME','HOME','HOME',['Filter1Frequency','Filter1Resonance','Eg1Attack','Eg1Release'],['CUT','RES','ATK','REL'],description='FILTER 1 / ENVELOPE 1')
add('MIX','HOME','MIXER',['Amp1Level','Amp2Level','MasterLevel','FilterConnection'],['AMP1','AMP2','LEVEL','ROUTE'],kind='MIX')
for i in range(1,5):
 add(f'OSC{i}','EDIT',f'OSC {i} / MIX',[f'Osc{i}SawLevel',f'Osc{i}TriLevel',f'Osc{i}PwmLevel',f'Osc{i}PwmWidth'],['SAW','WAVE','PULSE','WIDTH'],kind='OSC')
 add(f'OSC{i}TUNE','EDIT',f'OSC {i} / TUNE',[f'Osc{i}Sync',f'Osc{i}Semi',f'Osc{i}Tune',f'Osc{i}Waveform2'],['SYNC','SEMI','CENTS','WAVE'],kind='OSC')
 add(f'OSC{i}ROUTE','EDIT',f'OSC {i} / ROUTE',[f'Osc{i}Output1',f'Osc{i}Output2']+([f'Osc{i}NoiseLevel',f'Osc{i}NoiseLoFi'] if i==4 else []),['OUT1','OUT2']+(['NOISE','LO-FI'] if i==4 else []),kind='ROUTE')
for i in range(1,3):
 add(f'FILTER{i}','EDIT',f'FILTER {i}',[f'Filter{i}Frequency',f'Filter{i}Resonance',f'Filter{i}Type',f'Filter{i}KeyTrack'],['CUT','RES','TYPE','KEY'],kind='FILTER')
 add(f'FILTER{i}BR','EDIT',f'FILTER {i} / BREATH',[f'Filter{i}Breath',f'Filter{i}BreathSubMod'],['DEPTH','SOURCE'],kind='ROUTE')
add('FILTER3','EDIT','FILTER 3',['Filter3Enabled','Filter3Stage','Filter3Frequency','Filter3Resonance'],['ON','STAGE','CUT','RES'],kind='FILTER')
add('FILTER3MIX','EDIT','FILTER 3 / MIX',['Filter3Mix','FilterConnection'],['MIX','ROUTE'],kind='ROUTE')
add('VOICE','EDIT','VOICE / GLIDE',['GlideSpeed','GlideMinimumSpeed','GlideMode','GlideMidiEnabled'],['GLIDE','MIN','MODE','MIDI'],kind='VOICE',description='MONO / LAST NOTE / LEGATO')
add('SMOOTH','EDIT','GATE / SMOOTH',['GateOpenSmoothTime','GateCloseSmoothTime','BreathSmoothTime','PitchBendSmoothTime'],['OPEN','CLOSE','BREATH','BEND'],kind='VOICE')
add('GLSMOOTH','EDIT','GLIDE / SMOOTH',['GlideSmoothTime','PitchBendRange','MasterTune','Transpose'],['SMOOTH','BEND','CENTS','TRANS'],kind='VOICE')
for i in range(1,5):add(f'EG{i}','ENV',f'ENVELOPE {i}',[f'Eg{i}Attack',f'Eg{i}Decay',f'Eg{i}Sustain',f'Eg{i}Release'],['ATK','DEC','SUS','REL'],kind='ENV')
add('RETRIGGER','ENV','RETRIGGER',[f'Eg{i}Retrigger' for i in range(1,5)],['EG1','EG2','EG3','EG4'],kind='VOICE',description='EACH EG KEEPS ITS OWN RETRIGGER')
add('BREG','ENV','BREATH x EG',['Filter1BreathSubMod','Filter2BreathSubMod','Amp1BreathSubMod','Amp2BreathSubMod'],['FLT1','FLT2','AMP1','AMP2'],kind='ROUTE')
for i in range(1,3):
 add(f'LFO{i}','LFO',f'LFO {i}',[f'Lfo{i}Speed',f'Lfo{i}Waveform',f'Lfo{i}KeySync',f'Lfo{i}BPMSync'],['RATE','WAVE','TRIG','SYNC'],kind='LFO')
 add(f'LFO{i}SYNC','LFO',f'LFO {i} / SYNC',[f'Lfo{i}Beat',f'Lfo{i}BPMSync',f'Lfo{i}KeySync'],['BEAT','SYNC','TRIG'],kind='LFO')
add('MOD','MOD','MOD MATRIX',['MODSLOT','MODSRC1','MODDEST','MODDEPTH'],['SLOT','SRC1','DEST','DEPTH'],kind='MOD')
add('MOD2','MOD','MOD / MULTIPLY',['MODSLOT','MODSRC2','MODDEST','MODDEPTH'],['SLOT','SRC2','DEST','DEPTH'],kind='MOD')
def fx(i,key):return 156+i*14+['Bypass','Stereo','Speed','Feedback','Lowpass','WetDry','Rate','Depth','Mix','Time','Highpass','Size','HighDamp','Width'].index(key)
add('FX','FX','FX / MIX',[fx(0,'WetDry'),fx(1,'Mix'),fx(2,'WetDry'),fx(4,'WetDry')],['PHASE','CHO','DLY1','REV'],kind='FX')
add('PHASER','FX','PHASER',[fx(0,'Speed'),fx(0,'Feedback'),fx(0,'WetDry'),fx(0,'Bypass')],['RATE','FDBK','MIX','BYP'],kind='FX')
add('PHASER2','FX','PHASER / TONE',[fx(0,'Lowpass'),fx(0,'Stereo')],['TONE','STEREO'],kind='FX')
for i in range(2):
 add(f'DELAY{i+1}','FX',f'DELAY {i+1}',[fx(i+2,'Time'),fx(i+2,'Depth'),fx(i+2,'WetDry'),fx(i+2,'Bypass')],['TIME','FDBK','MIX','BYP'],kind='DELAY')
 add(f'DELAY{i+1}TONE','FX',f'DELAY {i+1} / TONE',[fx(i+2,'Lowpass'),fx(i+2,'Highpass')],['LOW','HIGH'],kind='DELAY')
add('REVERB','FX','REVERB',[fx(4,'Bypass'),fx(4,'Size'),fx(4,'HighDamp'),fx(4,'WetDry')],['BYP','SIZE','DAMP','MIX'],kind='REVERB')
add('WIDTH','FX','REVERB / WIDTH',[fx(4,'Width'),fx(4,'Size'),fx(4,'HighDamp'),fx(4,'WetDry')],['WIDTH','SIZE','DAMP','MIX'],kind='REVERB')
add('CHORUS','FX','CHORUS',[fx(1,'Rate'),fx(1,'Depth'),fx(1,'Mix'),fx(1,'Bypass')],['RATE','DEPTH','MIX','BYP'],kind='FX')
add('EXCITER','FX','EXCITER',['ExciterFrequency','ExciterMix'],['FREQ','MIX'],kind='FX')
add('ENHANCER','FX','ENHANCER',['EnhancerDelayL','EnhancerDelayR','EnhancerMix'],['LEFT','RIGHT','MIX'],kind='DELAY')
add('EQ','FX','EQ / OUTPUT',['EqLowGain','EqHighGain','MasterLevel'],['LOW','HIGH','LEVEL'],kind='FX')
add('TUNE','TUNE','TUNING',['Transpose','MasterTune',226,'PitchBendRange'],['TRANS','CENTS','A4 HZ','BEND'],kind='VOICE')
add('AMP','AMP','AMPLIFIERS',['Amp1Level','Amp1Breath','Amp2Level','Amp2Breath'],['LVL1','BR1','LVL2','BR2'],kind='MIX')
add('AMPSOURCE','AMP','AMP / BREATH',['Amp1BreathSubMod','Amp2BreathSubMod','BreathSmoothTime'],['SRC1','SRC2','SMOOTH'],kind='ROUTE')
add('GLOBAL','GLOBAL','CONTROL',[229,230,231,232],['WIND','MIDI','OCT','SLOT'],kind='SETTINGS')
add('LEARN','GLOBAL','MIDI / LEARN',[],[],kind='LEARN',description='TURN PARAM / REC / SEND CC')
add('SAVE','SAVE','USER SOUND',['SLOT','LOAD','ERASE','SAVE'],['SLOT','LOAD','ERASE','SAVE'],kind='SAVE')
add('PRESETS','PRESETS','PRESETS',['PRESET','CORE','FAV','LIST'],['NO.','CORE','FAV','LIST'],kind='PRESETS')
add('ADVANCED','ADVANCED','ALL PARAMETERS',[],[],kind='ADVANCED',description='EXPERT / RAW PARAMETER WORDS')
add('PERF','GLOBAL','PERFORMANCE',['PERFMODE','PERFRESET','PERFFREEZE','PERFSTATE'],['OUTPUT','RESET','HOLD','STATUS'],kind='PERF',description='COMPLETE AUDIO IRQ / 5804 us')
kinds=list(dict.fromkeys(p['kind'] for p in pages))
out=['/* Generated UI mapping only; no synthesis code or preset parameters. */',
 'enum {'+','.join('UG_'+g for g in groups)+',UG_COUNT};',
 'enum {'+','.join('UK_'+k for k in kinds)+'};',
 'enum {'+','.join(f'UC_{k}={v}' for k,v in special.items())+'};',
 'enum {'+','.join('UP_'+p['key'] for p in pages)+',UP_COUNT};',
 'typedef struct {uint8_t group,kind;uint16_t id[4];const char *title,*label[4],*help;} IFWUiPage;',
 'static const IFWUiPage ui_pages[]={']
for p in pages:out.append('{UG_'+p['group']+',UK_'+p['kind']+',{'+','.join(map(str,p['ids']))+'},'+json.dumps(p['title'])+',{'+','.join(json.dumps(x) for x in p['labels'])+'},'+json.dumps(p['description'])+'},')
out+=['};'];d=R/'firmware/src/ui';d.mkdir(exist_ok=True);(d/'pages.h').write_text('\n'.join(out)+'\n')
(R/'docs/ui-pages.json').write_text(json.dumps(pages,indent=2)+'\n')
print('generated',len(pages),'named UI pages')
