CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -Wpedantic -ffp-contract=off
REFERENCE = src/reference/host.c src/reference/engine.c src/reference/effects.c src/reference/params.c firmware/src/control/mono.c
HEADERS = src/reference/engine.h src/reference/internal.h src/reference/params.h src/reference/envelope_curve.h firmware/src/control/mono.h
FIXED = firmware/src/fixed/synth.c firmware/src/fixed/effects.c firmware/src/fixed/math.c firmware/src/fixed/envelope.c firmware/src/control/mono.c
.PHONY: all test sanitize clean tables fixed-test firmware-check web web-test panel-test panel-host firmware factory-test
all: build/ifw-reference
build/ifw-reference: $(REFERENCE) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(REFERENCE) -lm -o $@
tables:
	python3 tools/generate_tables.py
test: all fixed-test build/ifw-fixed panel-test web-test
	python3 tools/verify_sources.py
	node tests/compare.mjs
	node tests/engine_fixed_compare.mjs
	node tests/factory_regression.mjs
factory-test: build/ifw-fixed
	node tests/factory_regression.mjs
build/ifw-fixed: tests/engine_fixed_host.c $(FIXED) $(wildcard firmware/src/fixed/*.h)
	mkdir -p build
	$(CC) $(CFLAGS) tests/engine_fixed_host.c $(FIXED) -o $@
panel-test:
	mkdir -p build
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -Ivendor/dinghy/firmware/gen tests/panel_test.c $(FIXED) -o build/panel-test
	ASAN_OPTIONS=detect_leaks=0 build/panel-test
web:
	python3 tools/build_web.py
web-test: web
	python3 tests/ui_engine_unchanged.py
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-misleading-indentation -Ivendor/dinghy/firmware/gen tests/emu_native.c $(FIXED) -o build/emu-native
	node tests/web_test.mjs
	node tests/ui_test.mjs
	node tests/ui_audio_unchanged.mjs
	node tests/installer_test.mjs
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -fno-omit-frame-pointer -Ivendor/dinghy/firmware/gen tests/emu_native.c $(FIXED) -o build/ui-sanitize
	ASAN_OPTIONS=detect_leaks=0 build/ui-sanitize < build/ui/input.bin
firmware:
	python3 tools/build_firmware.py
build/fm1-host: firmware/src/app.c $(FIXED) firmware/src/panel_params.h firmware/src/presets.h $(wildcard firmware/src/ui/*) tests/panel_host.c
	mkdir -p build
	$(CC) -O2 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-missing-field-initializers -DIFW_HOST -Ivendor/dinghy/firmware/gen firmware/src/app.c $(FIXED) -o $@
panel-host: build/fm1-host
	mkdir -p build/panel
	build/fm1-host build/panel/openifw tests/panel-script.txt
sanitize:
	$(MAKE) CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -Wpedantic -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer' build/ifw-reference-sanitize
	ASAN_OPTIONS=detect_leaks=0 IFW_REFERENCE=build/ifw-reference-sanitize node tests/compare.mjs
build/ifw-reference-sanitize: $(REFERENCE) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(REFERENCE) -lm -o $@
fixed-test:
	sh tests/test_fixed.sh
firmware-check:
	sh tools/check_firmware_env.sh
clean:
	python3 -c 'from pathlib import Path; import shutil; shutil.rmtree(Path("build"), ignore_errors=True)'
