#!/bin/sh
set -eu
cd "$(dirname "$0")"
case "${1:---host}" in
  --host) exec make all fixed-test ;;
  --test) exec make test ;;
  --dinghy-host) cd vendor/dinghy; exec sh tests/run_host.sh ;;
  --dinghy-baseline) cd vendor/dinghy; exec sh build.sh ;;
  --demo) exec make web web-test ;;
  --panel-host) exec make panel-host ;;
  --firmware)
    sh tools/check_firmware_env.sh || exit 2
    exec python3 tools/build_firmware.py ;;
  *) echo 'usage: sh build.sh [--host|--test|--demo|--panel-host|--dinghy-host|--dinghy-baseline|--firmware]' >&2; exit 2 ;;
esac
