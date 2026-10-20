#!/usr/bin/env python3
"""Build using an isolated, unmodified Dinghy platform and its audited packer."""
import importlib.util,json,os,shutil,sys,hashlib,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
V=R/'vendor/dinghy';STAGE=R/'build/target-source'
STAGE.mkdir(parents=True,exist_ok=True)
# Never retain generated sources/linker overrides from an earlier variant.
if (STAGE/'firmware').exists():shutil.rmtree(STAGE/'firmware')
shutil.copytree(V/'firmware',STAGE/'firmware',dirs_exist_ok=True,copy_function=shutil.copy)
if (STAGE/'firmware/app.ld').read_bytes()!=(V/'firmware/app.ld').read_bytes():
    raise SystemExit('Staged linker differs from the original platform')
shutil.copytree(R/'firmware/hal',STAGE/'firmware/hal',dirs_exist_ok=True)
for folder in ['fixed','control','ui']:shutil.copytree(R/'firmware/src'/folder,STAGE/'firmware/src'/folder,dirs_exist_ok=True)
for name in ['panel_params.h','presets.h','audio_perf.h','audio_output.h','dsp.c']:shutil.copy2(R/'firmware/src'/name,STAGE/'firmware/src'/name)
shutil.copy2(R/'firmware/src/app.c',STAGE/'firmware/src/dinghy.c')
sys.path.insert(0,str(V/'tools'))
spec=importlib.util.spec_from_file_location('dinghy_build',V/'tools/build.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
b.SRC=STAGE;b.FW=STAGE/'firmware';b.OUT=R/'build/firmware';b.GEN=b.OUT/'gen';b.LDR=b.OUT/'loader'
# Tool paths must remain below SRC; the stage and output are siblings originally.
b.OUT=STAGE/'build';b.GEN=b.OUT/'gen';b.LDR=b.OUT/'loader'
b.PRODUCT='FM-1_982';b.VERSION='0.4.4'
b.CFLAGS.extend(['-DIFW_TARGET','-Werror'])
baseline_dir=os.environ.get('IFW_DSP_BASELINE_DIR')
if baseline_dir:
    baseline_dir=Path(baseline_dir).resolve()
    lock=json.loads((R/'docs/evidence/ui-engine-baseline.json').read_text())['sha256']
    for path,sha in lock.items():
        if not path.startswith(('firmware/src/fixed/','firmware/src/control/')):continue
        rel=Path(path).relative_to('firmware/src');data=(baseline_dir/rel).read_bytes()
        if hashlib.sha256(data).hexdigest()!=sha:raise SystemExit('Invalid 0.4.0 baseline: '+path)
        shutil.copy2(baseline_dir/rel,STAGE/'firmware/src'/rel)
    b.VERSION='0.4.0-perf';b.CFLAGS.append('-DIFW_DSP_BASELINE')
dsp_files=['fixed/synth.c','fixed/effects.c','fixed/math.c','fixed/envelope.c','control/mono.c'] if baseline_dir else ['dsp.c']
dsp_opt='-Os' if baseline_dir else '-O3'
if os.environ.get('IFW_DSP_STRESS')=='1':
    b.VERSION+='-stress';b.CFLAGS.append('-DIFW_DSP_STRESS')
    shutil.copy2(R/'tests/stress_patch.h',STAGE/'firmware/src/dsp_stress.h')
if os.environ.get('IFW_DSP_SELFTEST')=='1':
    b.VERSION+='-test';b.CFLAGS.append('-DIFW_DSP_SELFTEST')
    for source,name in [('dsp_optimization.c','dsp_selftest.h'),('dsp_math.c','dsp_math_test.h')]:
        shutil.copy2(R/'tests'/source,STAGE/'firmware/src'/name)
    b.CFLAGS.extend(['-Ifirmware/src/fixed'])
if os.environ.get('IFW_DSP_ISR_TEST')=='1':
    b.VERSION+='-irqtest';b.CFLAGS.append('-DIFW_DSP_ISR_TEST')
    shutil.copy2(R/'tests/dsp_irq_stream.c',STAGE/'firmware/src/dsp_irq_stream.h')
    b.CFLAGS.append('-Ifirmware/src/fixed')
tmp=R/'build/tmp';tmp.mkdir(exist_ok=True);os.environ['TMPDIR']=str(tmp)
original_tc_all=b.tc_all
def compile_all(*cmds):
    expanded=[]
    for c in cmds:
        expanded.append(c)
        if c[0]=='cc' and any(str(x).endswith('src/dinghy.c') for x in c):
            for name in dsp_files:
                out=b.OUT/(Path(name).stem+'.o')
                expanded.append(('cc',*b.CFLAGS,dsp_opt,'-Ifirmware/hal','-Ifirmware/src','-Ifirmware/gen','-c',b.FW/'src'/name,'-o',out))
    return original_tc_all(*expanded)
original_tc=b.tc
def tc(tool,*args):
    if tool=='pi32v2/bin/ld' and any(str(x).endswith('dinghy.o') for x in args):
        args=(*args,*(b.OUT/(Path(name).stem+'.o') for name in dsp_files))
    return original_tc(tool,*args)
b.tc_all=compile_all;b.tc=tc
for rel,sha in b.SDK_SHA256.items():
    if hashlib.sha256(b.fm1pkg_make.sdk_file(rel)).hexdigest()!=sha:raise SystemExit('SDK checksum mismatch: '+rel)
for f in (R/'firmware/src').rglob('*'):
    if f.suffix not in ['.h','.c']:continue
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',f.read_text(),flags=re.S)
    if any(rx.search(text) for rx in [b.MMIO_LIT,b.MMIO_CAST,b.MMIO_ASM]):raise SystemExit('Hardware access outside HAL: '+str(f))
sys.argv=[sys.argv[0]]
b.main()
dest=R/'build/firmware';dest.mkdir(exist_ok=True)
for source,name in [('dinghy.fwsc',f'OpenIFW-FM1-{b.VERSION}.fwsc'),('dinghy.bin','openifw.bin'),('dinghy.elf','openifw.elf'),('dinghy.dis','openifw.dis')]:shutil.copy2(b.OUT/source,dest/name)
image=(dest/'openifw.bin').read_bytes();package=(dest/f'OpenIFW-FM1-{b.VERSION}.fwsc').read_bytes()
result={'version':b.VERSION,'identity':b.PRODUCT,'appBytes':len(image),'packageBytes':len(package),'sha256':hashlib.sha256(package).hexdigest(),'hardwareTested':False}
symbols=b.tc('common/bin/objdump','-t',b.OUT/'dinghy.elf')
def sym(name):return int(re.search(r'^([0-9a-f]+) .*\s'+name+r'$',symbols,re.M)[1],16)
result.update(ramDataBss=sym('_bss_end')-0x01C08000,poolBytes=sym('_pool_end')-0x01C20000,limits={'app':581564,'ram':98304,'pool':344064},sampleRate=44100,framesPerBlock=256,cpuMeasured=False)
ramcode=(b.OUT/'ramtext.bin').read_bytes()
if len(ramcode)!=sym('_rt_end')-sym('_rt_start') or sym('_rt_end')>0x01C06000:
    raise SystemExit('RAM code copy exceeds its reserved region or misses a section')
if image[sym('_rt_load')-0x02000120:][:len(ramcode)]!=ramcode:
    raise SystemExit('Packaged RAM code differs from ELF load image')
for name in ['ifw_fixed_sample','ifw_fixed_fx_tick','ifw_fixed_process']:
    if not 0x02000120<=sym(name)<sym('_etext'):
        raise SystemExit('DSP must remain in the original XIP layout: '+name)
if (STAGE/'firmware/app.ld').read_bytes()!=(V/'firmware/app.ld').read_bytes():
    raise SystemExit('The recovery release must use the unmodified platform linker')
(dest/'linker.ld').write_bytes((STAGE/'firmware/app.ld').read_bytes())
result.update(ramCodeBytes=len(ramcode),ramDspBytes=0,dspPlacement='xip',
              linkerSha256=hashlib.sha256((V/'firmware/app.ld').read_bytes()).hexdigest(),
              outputLookaheadFrames=256)
(dest/'build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
