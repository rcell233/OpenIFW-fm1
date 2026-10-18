#!/usr/bin/env python3
"""Download the pinned official Linux x86_64 WASI compiler into the workspace."""
from pathlib import Path
import hashlib,subprocess,urllib.request,platform
R=Path(__file__).resolve().parents[1]
D=R.parent/'dependencies';sdk=D/'wasi-sdk-34/opt/wasi-sdk'
if (sdk/'bin/clang').exists():print(sdk);raise SystemExit(0)
if platform.system()!='Linux' or platform.machine() not in ['x86_64','amd64']:
    raise SystemExit('Install wasi-sdk 34 for your OS and set WASI_SDK_PATH.')
D.mkdir(exist_ok=True);archive=D/'wasi-sdk-34.deb'
url='https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-34/wasi-sdk-34.0-x86_64-linux.deb'
if not archive.exists():urllib.request.urlretrieve(url,archive)
expected='09f37a478107bf30381e0d8922ac678ebe998de59e574f96830f08e520908d4a'
if hashlib.sha256(archive.read_bytes()).hexdigest()!=expected:raise SystemExit('WASI SDK checksum mismatch')
dest=D/'wasi-sdk-34';dest.mkdir(exist_ok=True)
p=subprocess.Popen(['dpkg-deb','--fsys-tarfile',str(archive)],stdout=subprocess.PIPE)
subprocess.run(['tar','--no-same-owner','-xf','-','-C',str(dest)],stdin=p.stdout,check=True)
p.stdout.close()
if p.wait():raise SystemExit('Could not unpack WASI SDK')
print(sdk)
