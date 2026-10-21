# Third-party notices

New integration code is distributed under GPL-3.0-only; see LICENSE.

The DSP algorithms and the unchanged JavaScript oracle derive from OpenIFW by rcell233, Copyright (c) 2026 rcell233, MIT. Its complete permission notice is retained in `vendor/openifw/LICENSE`. The translated C code identifies this source; the oracle files remain unchanged.

The unchanged Dinghy reference platform is GPL-3.0-only, including work by Leo Kuroshita / Hügelton Instruments, SLOOP (isod89), and zvenson. See `vendor/dinghy/LICENSE` and `vendor/dinghy/LICENSING.md` for exact attribution. Terminus font material retains its SIL OFL notice under `vendor/dinghy/assets/fonts/`.

Dinghy's designburg logo and the upstream screenshot containing it are excluded from this distribution. OpenIFW-FM1 uses its own code-drawn interface and wave icon. The reference platform, HAL, loader, font and build/packaging files in vendor/ remain unchanged and pinned in sources.lock.json. OpenIFW's explicit platform/USB fork and HAL adaptations live under firmware/: foreground PCM production, asynchronous USB retry and ADC polling, bounded SIE timeouts, and a DMA memory fence. Original attribution is preserved there; the loader and package format remain derived from Dinghy/Felucca.

The JieLi toolchain and SDK source checkout are not bundled. Firmware packages include the SDK's uboot.boot, cfg_tool.bin and eq_cfg_hw.bin from AC79NN_SDK_V1.2.1_2023-12-13 (commit d179b4484759423312073f5fbb232501aa491047), under Apache-2.0. The full license is retained in licenses/JieLi-SDK-Apache-2.0.txt. The build verifies the three file hashes before packaging.

The six original JSON programs from OpenIFW are retained unchanged in `vendor/openifw/presets/`, including author metadata and ProgramIndex. The factory names and parameters are credited to STUDIO-R and are not relicensed under MIT or GPL; see their NOTICE.md. OpenIFW is itself an independent approximation of IFW. Product names remain the property of their respective owners; this project is not endorsed by M-VAVE or Hügelton Instruments.

The physical browser panel and AudioWorklet host are adapted from Felucca's `web/emu/index.html` and `web/emu/worklet.js`, Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0-only, following Charles Vestal's X0X browser build. Source notices remain in those files. The browser host now runs this project's shared C app and DSP. DotGothic16-subset.woff retains its SIL OFL 1.1 notice in `web/fonts/OFL.txt`.

The 0.4 screen windows, stone texture, palette and sword cursor are adapted from Ghoulbox by Jason Persinger (2026), GPL-3.0-only, based on Felucca by Leo Kuroshita / Hügelton Instruments. The page groups, four-card controls, hold layers, menu and user-slot operations follow that project's UI. Reference commit: f23fbc593f8af29dfa01174278f08ee79155ed27 at https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth . No Ghoulbox synthesis engines, sound programs, product wordmark or logo artwork are included.

VT323 by Peter Hull is licensed under SIL OFL 1.1. The original font and complete notice are in `assets/fonts/VT323-Regular.ttf` and `assets/fonts/OFL-VT323.txt`. The generated bitmap data retains that license.

The USB-MIDI installer transport and package parser in `vendor/felucca-web/` are unchanged from Felucca, commit 5738617968a22b5d4c0d3470cfcc95559052f446 (web/fm1ota.js and web/fm1pkg.js), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0-only. The mock device in `tests/installer_test.mjs` follows its web/test_web.mjs. OpenIFW's installer page and release validation wrap that protocol; no Felucca synthesis code is imported.

The optional CPU-instruction performance harness uses fm1-emu by Joakim Eriksson (2026), MIT, pinned to commit 19177bd79914fcdf527ac423f6689946a0557273 at https://github.com/joakimeriksson/fm1-emu . The emulator is a separate test dependency, not part of the Demo or device firmware. `tools/fm1-emu-profile.patch` adds PC counters/read-only inspection and completes scalar encodings using the JieLi disassembler; the MIT notice is retained in `licenses/fm1-emu-MIT.txt`. The instruction clock model is not cycle accurate and does not replace hardware measurements.
