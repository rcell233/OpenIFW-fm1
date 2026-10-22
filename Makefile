CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -Wpedantic -ffp-contract=off
REFERENCE = src/reference/host.c src/reference/engine.c src/reference/effects.c src/reference/params.c firmware/src/control/mono.c
HEADERS = src/reference/engine.h src/reference/internal.h src/reference/params.h src/reference/envelope_curve.h firmware/src/control/mono.h
FIXED = firmware/src/fixed/synth.c firmware/src/fixed/effects.c firmware/src/fixed/math.c firmware/src/fixed/envelope.c firmware/src/control/mono.c
.PHONY: all test sanitize clean tables fixed-test firmware-check web web-test panel-test panel-host firmware factory-test performance-test hardware-path-test
all: build/ifw-reference
build/ifw-reference: $(REFERENCE) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(REFERENCE) -lm -o $@
tables:
	python3 tools/generate_tables.py
test: all fixed-test build/ifw-fixed panel-test web-test performance-test hardware-path-test
	python3 tools/verify_sources.py
	node tests/compare.mjs
	node tests/engine_fixed_compare.mjs
	python3 tests/performance_regression.py
	node tests/factory_regression.mjs
factory-test: build/ifw-fixed
	node tests/factory_regression.mjs
build/ifw-fixed: tests/engine_fixed_host.c $(FIXED) $(wildcard firmware/src/fixed/*.h)
	mkdir -p build
	$(CC) $(CFLAGS) tests/engine_fixed_host.c $(FIXED) -o $@
build/dsp-regression: tests/dsp_optimization.c tests/dsp_math.c $(FIXED) $(wildcard firmware/src/fixed/*.h)
	mkdir -p build
	$(CC) $(CFLAGS) -Wno-misleading-indentation -DIFW_SELFTEST_MAIN -Ifirmware/src/fixed -Ifirmware/src tests/dsp_optimization.c $(FIXED) -o $@
build/dsp-regression-unity: tests/dsp_optimization.c tests/dsp_math.c firmware/src/dsp.c $(FIXED) $(wildcard firmware/src/fixed/*.h)
	mkdir -p build
	$(CC) $(CFLAGS) -O3 -Wno-misleading-indentation -DIFW_SELFTEST_MAIN -Ifirmware/src/fixed -Ifirmware/src tests/dsp_optimization.c firmware/src/dsp.c -o $@
build/source-capture: tests/source_capture.c $(FIXED) $(wildcard firmware/src/fixed/*.h)
	mkdir -p build
	$(CC) $(CFLAGS) -Ifirmware/src/fixed -Ifirmware/src tests/source_capture.c $(FIXED) -o $@
performance-test: build/dsp-regression build/dsp-regression-unity build/source-capture
	python3 tests/performance_regression.py --live-only
	python3 tests/source_capture_test.py
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -DIFW_SELFTEST_MAIN -Ifirmware/src/fixed -Ifirmware/src -fsanitize=address,undefined -fno-omit-frame-pointer tests/dsp_optimization.c firmware/src/dsp.c -o build/dsp-regression-sanitize
	ASAN_OPTIONS=detect_leaks=0 build/dsp-regression-sanitize > build/dsp-regression-sanitize.json
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -Ifirmware/src/fixed -Ifirmware/src -fsanitize=address,undefined -fno-omit-frame-pointer tests/source_capture.c firmware/src/dsp.c -o build/source-capture-sanitize
	ASAN_OPTIONS=detect_leaks=0 build/source-capture-sanitize > build/source-capture-sanitize.json
	python3 -c 'import json; assert json.load(open("build/source-capture-sanitize.json")) == json.load(open("docs/evidence/source-capture-rc1.json"))["hashes"]'
build/dsp-irq-stream: tests/dsp_irq_stream.c $(FIXED) $(wildcard firmware/src/fixed/*.h) firmware/src/presets.h
	mkdir -p build
	$(CC) $(CFLAGS) -DIFW_IRQ_STREAM_MAIN -Ifirmware/src/fixed -Ifirmware/src tests/dsp_irq_stream.c $(FIXED) -o $@
build/audio-scheduler: tests/audio_scheduler.c tests/audio_output_rc1.h firmware/src/audio_perf.h
	mkdir -p build
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -fno-omit-frame-pointer tests/audio_scheduler.c -o $@
build/audio-scheduler-rc2: tests/audio_scheduler_rc2.c tests/audio_output_rc2.h firmware/src/audio_perf.h
	mkdir -p build
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -fno-omit-frame-pointer tests/audio_scheduler_rc2.c -o $@
build/audio-scheduler-production: tests/audio_scheduler_rc2.c firmware/src/audio_output.h firmware/src/audio_perf.h
	mkdir -p build
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -DIFW_PRODUCTION_AUDIO_TEST -fsanitize=address,undefined -fno-omit-frame-pointer tests/audio_scheduler_rc2.c -o $@
hardware-path-test: build/dsp-irq-stream build/audio-scheduler build/audio-scheduler-rc2 build/audio-scheduler-production
	mkdir -p build/target-audit
	ASAN_OPTIONS=detect_leaks=0 build/audio-scheduler > build/target-audit/scheduler.json
	ASAN_OPTIONS=detect_leaks=0 build/audio-scheduler-rc2 > build/target-audit/scheduler-rc2.json
	ASAN_OPTIONS=detect_leaks=0 build/audio-scheduler-production > build/target-audit/scheduler-production.json
	python3 tests/timer_services_test.py
	python3 tests/boot_diagnostics_test.py
	node tests/diagnostics_test.mjs
	node tests/diagnostics_page_test.mjs
	build/dsp-irq-stream > build/target-audit/irq-native.json
	python3 tests/irq_stream_test.py
	python3 tests/usb_retry_test.py
	python3 tests/usb_sie_deadline.py
	python3 tests/audio_deadline.py
	python3 tests/audio_deadline.py --pipeline
	python3 tests/audio_deadline.py --pipeline-042
panel-test:
	mkdir -p build
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -Ivendor/dinghy/firmware/gen tests/panel_test.c $(FIXED) -o build/panel-test
	ASAN_OPTIONS=detect_leaks=0 build/panel-test
	$(CC) -O1 -g -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -Ivendor/dinghy/firmware/gen tests/display_scheduler.c $(FIXED) -o build/display-scheduler
	ASAN_OPTIONS=detect_leaks=0 build/display-scheduler
web:
	python3 tools/build_web.py
web-test: web
	python3 tests/ui_engine_unchanged.py
	$(CC) -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-misleading-indentation -Ivendor/dinghy/firmware/gen tests/emu_native.c $(FIXED) -o build/emu-native
	node tests/web_test.mjs
	node tests/ui_test.mjs
	node tests/octave_config.mjs
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
