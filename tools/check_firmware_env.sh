#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import json,os,platform
from pathlib import Path
tc=Path(os.environ.get('JIELI_TOOLCHAIN',str(Path.home()/'.jieli/toolchain')))
sdk=Path(os.environ.get('AC79_SDK',str(Path.home()/'fw-AC79_AIoT_SDK')))
sdkfiles=['cpu/wl82/tools/uboot.boot','cpu/wl82/tools/cfg_tool.bin','cpu/wl82/tools/cfg/eq_cfg_hw.bin']
result={'platform':platform.system()+' '+platform.machine(),
        'jieliCompilerPresent':(tc/'pi32v2/bin/clang').is_file(),
        'sdkFilesPresent':{f:(sdk/f).is_file() for f in sdkfiles},
        'fixedSynthIntegrated':Path('firmware/src/app.c').is_file(),
        'firmwarePackageProduced':Path('build/firmware/OpenIFW-FM1-'+json.loads(Path('package.json').read_text())['version']+'.fwsc').is_file()}
Path('build').mkdir(exist_ok=True)
Path('build/firmware-environment.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if result['jieliCompilerPresent'] and all(result['sdkFilesPresent'].values()) else 2)
PY
