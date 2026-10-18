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
tmp=R/'build/tmp';tmp.mkdir(exist_ok=True)
env=dict(os.environ,TMPDIR=str(tmp),LD_LIBRARY_PATH=str(sdk/'lib')+(':'+os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else ''))
cmd=[cc,'-O2','-std=c11','-ffp-contract=off','-fno-fast-math','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',
     '-I'+str(R/'vendor/dinghy/firmware/gen'),'-nostartfiles','-Wl,--no-entry','-Wl,--export-dynamic','-Wl,--export-memory',
     '-Wl,-z,stack-size=1048576','-Wl,--initial-memory=4194304',str(R/'web/emu/openifw_web.c'),
     *[str(R/'firmware/src/fixed'/f'{n}.c') for n in ['synth','effects','math','envelope']],str(R/'firmware/src/control/mono.c'),'-o',str(O/'openifw.wasm')]
subprocess.run(cmd,check=True,env=env)
subprocess.run(['node','--check',str(O/'worklet.js')],check=True)
record=dict(version='0.4.0',kind='shared-C-firmware-host',wasmBytes=(O/'openifw.wasm').stat().st_size,
            wasmSha256=hashlib.sha256((O/'openifw.wasm').read_bytes()).hexdigest(),factoryPresets=6,
            compiler=subprocess.check_output([cc,'--version'],env=env,text=True).splitlines()[0])
(O/'build.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record))
