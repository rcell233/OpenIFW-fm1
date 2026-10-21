#!/usr/bin/env python3
"""Compile the board's C application to WASM and package the physical FM-1 host."""
from pathlib import Path
import os,shutil,subprocess,json,hashlib
R=Path(__file__).resolve().parents[1];O=R/'build/web'
sdk=Path(os.environ.get('WASI_SDK_PATH',R.parent/'dependencies/wasi-sdk-34/opt/wasi-sdk'))
cc=os.environ.get('WASM_CC',str(sdk/'bin/clang'))
if not Path(cc).is_file() and not shutil.which(cc):
    raise SystemExit('Install wasi-sdk 34 from https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-34 and set WASI_SDK_PATH.')
subprocess.run(['python3',str(R/'tools/generate_presets.py')],check=True)
if O.exists():shutil.rmtree(O)
O.mkdir(parents=True)
for name in ['index.html','worklet.js']:shutil.copy2(R/'web'/name,O/name)
if (R/'web/fonts').exists():shutil.copytree(R/'web/fonts',O/'fonts')
for n in ['LICENSE','THIRD_PARTY_NOTICES.md']:shutil.copy2(R/n,O/n)
shutil.copytree(R/'vendor/openifw/presets',O/'presets')
shutil.copytree(R/'web/install',O/'install')
(O/'install/lib').mkdir()
for name in ['fm1ota.js','fm1pkg.js']:
    shutil.copy2(R/'vendor/felucca-web'/name,O/'install/lib'/name)
release=json.loads((R/'firmware-bin/build.json').read_text())
filename=f"OpenIFW-FM1-{release['version']}.fwsc"
package=(R/'firmware-bin'/filename).read_bytes()
if len(package)!=release['packageBytes'] or hashlib.sha256(package).hexdigest()!=release['sha256']:
    raise SystemExit('Firmware package does not match firmware-bin/build.json; installer was not built.')
if release['identity']!='FM-1_982':
    raise SystemExit('Unexpected installer firmware identity.')
(O/'firmware').mkdir()
shutil.copy2(R/'firmware-bin'/filename,O/'firmware'/filename)
shutil.copy2(R/'firmware-bin/build.json',O/'firmware/build.json')
(O/'install/release.json').write_text(json.dumps(dict(schema=1,version=release['version'],
    product=release['identity'],file=filename,bytes=len(package),sha256=release['sha256'],
    hardwareTested=release['hardwareTested']),indent=2)+'\n')
# The byte-identical previously bootable package remains available for rescue.
for version in ['0.4.1','0.4.4']:
    recovery=json.loads((R/f'firmware-bin/OpenIFW-FM1-{version}.json').read_text())
    recovery_file=f'OpenIFW-FM1-{version}.fwsc'
    recovery_bytes=(R/'firmware-bin'/recovery_file).read_bytes()
    if recovery['version']!=version or recovery['identity']!='FM-1_982' or len(recovery_bytes)!=recovery['packageBytes'] or hashlib.sha256(recovery_bytes).hexdigest()!=recovery['sha256']:
        raise SystemExit(f'Recovery package does not match the locked {version} metadata.')
    shutil.copy2(R/'firmware-bin'/recovery_file,O/'firmware'/recovery_file)
    (O/f'install/recovery-{version}.json').write_text(json.dumps(dict(schema=1,version=version,
        product=recovery['identity'],file=recovery_file,bytes=len(recovery_bytes),sha256=recovery['sha256'],
        hardwareTested=recovery['hardwareTested']),indent=2)+'\n')
tmp=R/'build/tmp';tmp.mkdir(exist_ok=True)
env=dict(os.environ,TMPDIR=str(tmp),LD_LIBRARY_PATH=str(sdk/'lib')+(':'+os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else ''))
cmd=[cc,'-O2','-std=c11','-ffp-contract=off','-fno-fast-math','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',
     '-I'+str(R/'vendor/dinghy/firmware/gen'),'-nostartfiles','-Wl,--no-entry','-Wl,--export-dynamic','-Wl,--export-memory',
     '-Wl,-z,stack-size=1048576','-Wl,--initial-memory=4194304',str(R/'web/emu/openifw_web.c'),
     *[str(R/'firmware/src/fixed'/f'{n}.c') for n in ['synth','effects','math','envelope']],str(R/'firmware/src/control/mono.c'),'-o',str(O/'openifw.wasm')]
subprocess.run(cmd,check=True,env=env)
subprocess.run(['node','--check',str(O/'worklet.js')],check=True)
for name in ['install.js','controller.js']:
    subprocess.run(['node','--check',str(O/'install'/name)],check=True)
record=dict(version=release['version'],kind='shared-C-firmware-host',wasmBytes=(O/'openifw.wasm').stat().st_size,
            wasmSha256=hashlib.sha256((O/'openifw.wasm').read_bytes()).hexdigest(),factoryPresets=6,
            compiler=subprocess.check_output([cc,'--version'],env=env,text=True).splitlines()[0])
(O/'build.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record))
