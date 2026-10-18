#!/bin/sh
set -eu
target="${1:-dependencies/ac79-sdk}"
git clone --depth 1 --filter=blob:none --sparse --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$target"
git -C "$target" sparse-checkout set cpu/wl82/tools
python3 - "$target" <<'PY'
import hashlib,sys
from pathlib import Path
p=Path(sys.argv[1])/'cpu/wl82/tools'
for f,sha in {'uboot.boot':'4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4','cfg_tool.bin':'276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b','cfg/eq_cfg_hw.bin':'41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d'}.items():
 assert hashlib.sha256((p/f).read_bytes()).hexdigest()==sha,f
print('PASS SDK hashes')
PY
