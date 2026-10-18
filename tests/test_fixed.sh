#!/bin/sh
set -eu
mkdir -p build
cc -O2 -std=c11 -Wall -Wextra -Werror -Wpedantic -fsanitize=undefined tests/fixed_host.c firmware/src/control/mono.c firmware/src/fixed/envelope.c -o build/fixed-host
build/fixed-host
cc -O2 -std=c11 -Wall -Wextra -Werror -Wpedantic -c firmware/src/fixed/envelope.c -o build/fixed-envelope.o
cc -O2 -std=c11 -Wall -Wextra -Werror -Wpedantic -c firmware/src/control/mono.c -o build/mono.o
if nm -u build/fixed-envelope.o build/mono.o | rg '__.*(divdi|moddi|float|double|sf3|df3)|sin|cos|exp|log|pow|malloc'; then
    echo 'unexpected non-integer runtime dependency' >&2
    exit 1
fi
node tests/fixed_compare.mjs
