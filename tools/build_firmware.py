#!/usr/bin/env python3
"""Build using an isolated, unmodified Dinghy platform and its audited packer."""
import importlib.util,json,os,shutil,sys,hashlib,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
V=R/'vendor/dinghy';STAGE=R/'build/target-source'
STAGE.mkdir(parents=True,exist_ok=True)
shutil.copytree(V/'firmware',STAGE/'firmware',dirs_exist_ok=True)
for folder in ['fixed','control','ui']:shutil.copytree(R/'firmware/src'/folder,STAGE/'firmware/src'/folder,dirs_exist_ok=True)
for name in ['panel_params.h','presets.h']:shutil.copy2(R/'firmware/src'/name,STAGE/'firmware/src'/name)
shutil.copy2(R/'firmware/src/app.c',STAGE/'firmware/src/dinghy.c')
sys.path.insert(0,str(V/'tools'))
spec=importlib.util.spec_from_file_location('dinghy_build',V/'tools/build.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
b.SRC=STAGE;b.FW=STAGE/'firmware';b.OUT=R/'build/firmware';b.GEN=b.OUT/'gen';b.LDR=b.OUT/'loader'
# Tool paths must remain below SRC; the stage and output are siblings originally.
b.OUT=STAGE/'build';b.GEN=b.OUT/'gen';b.LDR=b.OUT/'loader'
b.PRODUCT='FM-1_982';b.VERSION='0.4.0'
b.CFLAGS.extend(['-DIFW_TARGET','-Werror'])
tmp=R/'build/tmp';tmp.mkdir(exist_ok=True);os.environ['TMPDIR']=str(tmp)
original_tc_all=b.tc_all
def compile_all(*cmds):
    expanded=[]
    for c in cmds:
        expanded.append(c)
        if c[0]=='cc' and any(str(x).endswith('src/dinghy.c') for x in c):
            for name in ['fixed/synth.c','fixed/effects.c','fixed/math.c','fixed/envelope.c','control/mono.c']:
                out=b.OUT/(Path(name).stem+'.o')
                expanded.append(('cc',*b.CFLAGS,'-Ifirmware/hal','-Ifirmware/src','-Ifirmware/gen','-c',b.FW/'src'/name,'-o',out))
    return original_tc_all(*expanded)
original_tc=b.tc
def tc(tool,*args):
    if tool=='pi32v2/bin/ld' and any(str(x).endswith('dinghy.o') for x in args):
        args=(*args,*(b.OUT/(name+'.o') for name in ['synth','effects','math','envelope','mono']))
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
for source,name in [('dinghy.fwsc','OpenIFW-FM1-0.4.0.fwsc'),('dinghy.bin','openifw.bin'),('dinghy.elf','openifw.elf'),('dinghy.dis','openifw.dis')]:shutil.copy2(b.OUT/source,dest/name)
image=(dest/'openifw.bin').read_bytes();package=(dest/'OpenIFW-FM1-0.4.0.fwsc').read_bytes()
result={'version':b.VERSION,'identity':b.PRODUCT,'appBytes':len(image),'packageBytes':len(package),'sha256':hashlib.sha256(package).hexdigest(),'hardwareTested':False}
symbols=b.tc('common/bin/objdump','-t',b.OUT/'dinghy.elf')
def sym(name):return int(re.search(r'^([0-9a-f]+) .*\s'+name+r'$',symbols,re.M)[1],16)
result.update(ramDataBss=sym('_bss_end')-0x01C08000,poolBytes=sym('_pool_end')-0x01C20000,limits={'app':581564,'ram':98304,'pool':344064},sampleRate=44100,framesPerBlock=256,cpuMeasured=False)
(dest/'build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
