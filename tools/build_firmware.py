#!/usr/bin/env python3
"""Build OpenIFW explicitly; reuse Dinghy's verified loader and package format.

No compiler/linker/objcopy function is monkey-patched. The audio RAM section has
its own VMA/LMA, is assembled by its actual load address, and is checked against
crt0's contiguous copy range. The flash-off driver retains its no-XIP-call audit.
"""
import hashlib
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

R=Path(__file__).resolve().parents[1]
V=R/'vendor/dinghy'
stage_root=R/'build/target-source'
stage_root.mkdir(parents=True,exist_ok=True)
# Isolate each compilation's copied inputs. Keep the final output publication
# locked too: no variant may replace a different build's ELF/package pair.
build_lock=(R/'build/firmware-build.lock').open('w')
fcntl.flock(build_lock.fileno(),fcntl.LOCK_EX)
STAGE=Path(tempfile.mkdtemp(prefix='compile-',dir=stage_root))
shutil.copytree(V/'firmware',STAGE/'firmware')
shutil.copytree(R/'firmware',STAGE/'firmware',dirs_exist_ok=True)
# Recovery builds require the original electrical scan sequence and IRQ entry.
# Copy explicitly after the overlay, and bind the compiled inputs in metadata.
original_hal=['fm1_input.h','fm1_time.h','fm1_isr.S']
hal_sha={}
for name in original_hal:
    src=V/'firmware/hal'/name;dst=STAGE/'firmware/hal'/name
    shutil.copy2(src,dst)
    if dst.read_bytes()!=src.read_bytes():raise SystemExit('HAL staging mismatch: '+name)
    hal_sha[name]=hashlib.sha256(dst.read_bytes()).hexdigest()
shutil.copy2(R/'firmware/src/app.c',STAGE/'firmware/src/dinghy.c')
if os.environ.get('IFW_DSP_XIP')=='1':
    shutil.copy2(V/'firmware/app.ld',STAGE/'firmware/app.ld')
sys.path.insert(0,str(V/'tools'))
spec=importlib.util.spec_from_file_location('dinghy_build',V/'tools/build.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
b.SRC=STAGE;b.FW=STAGE/'firmware';b.OUT=STAGE/'build';b.GEN=b.OUT/'gen';b.LDR=b.OUT/'loader'
b.PRODUCT='FM-1_982';b.VERSION='0.4.7-rc3'
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
    shutil.copy2(V/'firmware/app.ld',STAGE/'firmware/app.ld')
dsp_files=['fixed/synth.c','fixed/effects.c','fixed/math.c','fixed/envelope.c','control/mono.c'] if baseline_dir else ['dsp.c']
for flag,suffix,files in [
    ('IFW_DSP_STRESS','-stress',[('stress_patch.h','dsp_stress.h')]),
    ('IFW_DSP_SELFTEST','-test',[('dsp_optimization.c','dsp_selftest.h'),('dsp_math.c','dsp_math_test.h')]),
    ('IFW_DSP_ISR_TEST','-irqtest',[('dsp_irq_stream.c','dsp_irq_stream.h')])]:
    if os.environ.get(flag)=='1':
        b.VERSION+=suffix;b.CFLAGS.append('-D'+flag)
        for source,name in files:shutil.copy2(R/'tests'/source,STAGE/'firmware/src'/name)
        b.CFLAGS.append('-Ifirmware/src/fixed')
if os.environ.get('IFW_DSP_XIP')=='1':b.VERSION+='-xip'
reverb_div=1 if baseline_dir or os.environ.get('IFW_DSP_SELFTEST')=='1' else 2
b.CFLAGS.append(f'-DIFW_REVERB_DIV={reverb_div}')
for rel,sha in b.SDK_SHA256.items():
    if hashlib.sha256(b.fm1pkg_make.sdk_file(rel)).hexdigest()!=sha:raise SystemExit('SDK checksum mismatch: '+rel)
for f in (R/'firmware/src').rglob('*'):
    if f.suffix not in ['.h','.c']:continue
    source=re.sub(r'/\*.*?\*/|//[^\n]*','',f.read_text(),flags=re.S)
    if any(rx.search(source) for rx in [b.MMIO_LIT,b.MMIO_CAST,b.MMIO_ASM]):raise SystemExit('Hardware access outside HAL: '+str(f))
tmp=R/'build/tmp';tmp.mkdir(exist_ok=True);os.environ['TMPDIR']=str(tmp)
b.OUT.mkdir(exist_ok=True)
ota=b.build_loader()
common=[*b.CFLAGS,'-Ifirmware/hal','-Ifirmware/src','-Ifirmware/gen',
        f'-DFELUCCA_ID="{b.PRODUCT}"',f'-DAPP_VER="{b.VERSION}"']
commands=[('cc','-c',b.FW/'crt0.S','-o',b.OUT/'crt0.o'),
          ('cc','-c',b.FW/'hal/fm1_vec.S','-o',b.OUT/'fm1_vec.o'),
          ('cc','-c',b.FW/'hal/fm1_isr.S','-o',b.OUT/'fm1_isr.o'),
          ('cc',*common,'-O2','-c',b.FW/'src/dinghy.c','-o',b.OUT/'dinghy.o')]
objects=[b.OUT/n for n in ['crt0.o','fm1_vec.o','fm1_isr.o','dinghy.o']]
for name in dsp_files:
    obj=b.OUT/(Path(name).stem+'.o');objects.append(obj)
    commands.append(('cc',*common,'-fdata-sections','-Os' if baseline_dir else '-O3','-c',b.FW/'src'/name,'-o',obj))
b.tc_all(*commands)
for name,sha in hal_sha.items():
    if hashlib.sha256((STAGE/'firmware/hal'/name).read_bytes()).hexdigest()!=sha:
        raise SystemExit('HAL input changed during compilation: '+name)
elf=b.OUT/'dinghy.elf'
b.tc('pi32v2/bin/ld','--gc-sections','-e','_start','-T',b.FW/'app.ld',*objects,'-o',elf)
syms,dis,rt=b.tc_all(('common/bin/objdump','-t',elf),('common/bin/objdump','-d',elf),
                    ('common/bin/objdump','-d','-j','.ram_text',elf))
def sym(name):
    m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M)
    if not m:raise SystemExit('Missing linker symbol: '+name)
    return int(m[1],16)
sections=[('.text',0x02000120),('.ram_text',sym('_rt_load'))]
ram_audio='_audio_start' in syms
if ram_audio:sections.append(('.audio_text',sym('_audio_load')))
sections.append(('.data',sym('_data_load')))
image=bytearray();blobs={}
for section,load in sections:
    path=b.OUT/(section[1:]+'.bin')
    b.tc('common/bin/objcopy','-O','binary','-j',section,elf,path)
    data=path.read_bytes();blobs[section]=data
    offset=load-0x02000120
    if load%4 or offset<len(image):raise SystemExit('Invalid/overlapping load section: '+section)
    image.extend(b'\xff'*(offset-len(image)));image.extend(data)
image.extend(b'\xff'*(-len(image)%4))
ramcode=blobs['.ram_text']+blobs.get('.audio_text',b'')
if len(ramcode)!=sym('_rt_end')-sym('_rt_start') or sym('_rt_end')>0x01C06000:
    raise SystemExit('RAM copy length/range differs from crt0')
if image[sym('_rt_load')-0x02000120:][:len(ramcode)]!=ramcode:
    raise SystemExit('RAM load image is not the contiguous crt0 copy')
if ram_audio and (sym('_audio_start')!=sym('_rt_start')+len(blobs['.ram_text']) or
                  sym('_audio_load')!=sym('_rt_load')+len(blobs['.ram_text'])):
    raise SystemExit('Audio section run/load addresses are not adjacent to the driver')
for name in ['ifw_fixed_sample','ifw_fixed_fx_tick','ifw_fixed_process']:
    if ram_audio and not sym('_audio_start')<=sym(name)<sym('_rt_end'):
        raise SystemExit('Hot DSP escaped the audio RAM section: '+name)
for name in ['fm1_timer5_irq','isr_timer5','isr_alnk0']:
    if sym(name)<0x02000120:
        raise SystemExit('Recovery build requires original Flash input/IRQ placement: '+name)
for name in ['FM1_KEYMAP','FM1_ENC']:
    if sym(name)<0x02000120:raise SystemExit('Original input map unexpectedly placed in RAM: '+name)
hot_tables=['ifw_sine_table','ifw_ap_table','ifw_ap_half_table','ifw_alpha_table','ifw_f3_table','decay_curve_q31']
if ram_audio:
    for name in hot_tables:
        if not sym('_data_start')<=sym(name)<sym('_data_end'):
            raise SystemExit('DSP table escaped the standard RAM data copy: '+name)
errors,notes=b.check(image,syms,dis,rt)
errors+=b.mmio_check()
for note in notes:print('  ok   ',note)
if errors:raise SystemExit('\n'.join(errors))
package=b.fm1pkg_make.ufw(b.fm1pkg_make.flash_image(bytes(image),b.fm1pkg_make.KEY),ota,b.PRODUCT)
dest=R/'build/firmware';dest.mkdir(exist_ok=True)
for name,data in [('openifw.bin',image),(f'OpenIFW-FM1-{b.VERSION}.fwsc',package)]:
    (dest/name).write_bytes(data)
shutil.copy2(elf,dest/'openifw.elf');(dest/'openifw.dis').write_text(dis)
(dest/'linker.ld').write_bytes((b.FW/'app.ld').read_bytes())
result=dict(version=b.VERSION,identity=b.PRODUCT,appBytes=len(image),packageBytes=len(package),
    originalHalSha256=hal_sha,
    sha256=hashlib.sha256(package).hexdigest(),hardwareTested=False,
    ramDataBss=sym('_bss_end')-0x01C08000,poolBytes=sym('_pool_end')-0x01C20000,
    limits=dict(app=581564,ram=98304,pool=344064),sampleRate=44100,framesPerBlock=256,
    cpuMeasured=False,ramCodeBytes=len(ramcode),ramFlashDriverBytes=len(blobs['.ram_text']),
    ramDspBytes=len(blobs.get('.audio_text',b'')),ramIrqBytes=0,
    dspPlacement='ram' if ram_audio else 'xip',
    linkerSha256=hashlib.sha256((b.FW/'app.ld').read_bytes()).hexdigest(),
    ramTables={name:sym(name) for name in hot_tables} if ram_audio else {},
    audioScheduling='foreground-32-frame-slices-with-4-slot-SPSC-queue',
    reverbSampleRate=44100//reverb_div,
    reverbTopology='stereo-8comb-4allpass' if reverb_div==1 else 'shared-4comb-2allpass-stereo-taps',
    fxProfile='full-rate-regression-only' if reverb_div==1 else 'half-rate-Q15-room',
    experimental=True,stage='release-candidate')
(dest/'build.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
