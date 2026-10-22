# OpenIFW-fm1

**2026-10-22 启动回归：0.4.7-rc1 实机安装后进入 USB RESCUE，未通过启动验收，已撤出默认安装。当前网页默认提供原始 0.4.6-rc2 恢复包；它仍有破音。后续启动修复尚在调查，下文 rc1 模型通过不能证明实机可启动。**

OpenIFW 的 FM-1 专用单音合成器移植。**0.4.6-rc2 实机 UI 已流畅，爆音仍在。0.4.7-rc1 在克隆研究多个 FM-1 实现后，改用常规中断音频、完整块 DMA 提交、RAM 输入扫描和分段 USB 供数，并换用清晰的朴素界面。音色、矩阵、包络与单音 / Legato 保持旧输出。六个工厂音色通过软件预算场景，但极端压力音色仍超时；实机声音尚未验收。详见 [研究与测试报告](docs/RESEARCH_0.4.7.md)。**

公开网页：[在线 Demo](https://rcell233.github.io/OpenIFW-fm1/) · [USB 网页安装](https://rcell233.github.io/OpenIFW-fm1/install/) · [设备性能采集](https://rcell233.github.io/OpenIFW-fm1/diagnostics/)。Demo、安装器与固件下载均由 GitHub Pages 提供，无需登录 ChatGPT。浏览器通过 WebAssembly 运行与目标固件相同的 `app.c`、定点 DSP、显示和存储逻辑，模拟琴键、按钮、旋钮、240×240 RGB565 LCD 与 Flash。它属于固件主机仿真；不模拟杰理 CPU 指令、USB 外设、升级器或电池。

## 从网页安装

在桌面版 Chrome / Edge 打开 [安装页](https://rcell233.github.io/OpenIFW-fm1/install/?v=0.4.7-rc1)，用 USB 数据线将 FM-1 直接接到电脑并打开电源，点击 **Install 0.4.7-rc1**，允许浏览器使用 MIDI / SysEx。网页先核对固件 SHA-256、包内 CRC、升级 loader 与设备型号，再执行准备、写入、重启与身份确认。无需安装 Python、驱动工具或命令行升级器。

安装会替换当前固件；请先备份要保留的设备音色 / 配置，写入时不要断电、拔线或关闭页面。兼容的升级模式可在重连后再次点击 Install 恢复。每次只连接一台要安装的 FM-1。

网页安装协议已通过模拟设备测试。0.4.4 已确认可以启动；0.4.5 的后续实机反馈仍有严重问题。当前候选版只报告软件回归与目标指令测试结果，不把它们当作设备声音、帧率或安装成功的证明。

如果设备当前显示 **OPENIFW FM1 USB RESCUE / CONNECT USB**，保持 USB 连接、松开全部按键，尤其 OCT−，在安装页确认按钮为 **Install 0.4.7-rc1** 后安装。可 [回退到 0.4.6-rc2](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.6-rc2&v=0.4.7-rc1) 恢复上一版；它的 UI 已有实机流畅反馈，仍存在破音。也可 [回退到 0.4.4](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.4&v=0.4.7-rc1) 恢复本次已确认能启动的原包；它仍有卡顿和少量破音。更早的 [0.4.1 恢复包](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.1&v=0.4.7-rc1) 也保留。救援身份 FM-1_000 不会被当作安装成功。

## 保留的合成行为

- 单音、最后按键优先、重复音去重、松键回到仍按住的音和相应力度；Legato 不增加声部。
- 四个独立 EG 的 Retrigger，BR × EG 的松键尾音，Time / Rate 滑音，气息、门控和弯音平滑。
- 四振荡器、2 倍内部采样、SIN / TRI / SAW / PWM、噪声、中心交点 Sync，以及改变读取相位而不改变同步时钟的 FM。
- 静音振荡器继续作为调制源；八槽双来源调制，普通调制每 16 输出采样更新，FM 在内部采样率计算。
- 双主滤波器和三种路由、Filter3、两组 LFO、Exciter、Enhancer、Phaser、Chorus、双 Delay、Reverb 和 EQ。

硬件效果器保留链路，两个 Delay 改为 22.05 kHz / 16 位 / 最长 1 秒，Reverb 缓冲改为 16 位。主合成器采用 Q20 音频、Q16 控制、Q31 包络及整数查表；量化、插值和饱和会带来小差异。详见 [移植记录](docs/DSP_PORT.md)。

## 编译与测试

电脑端需要 C11 编译器、Make、Python 3、Node.js 22+、binutils，以及编译浏览器固件的 WASI SDK 34。下载脚本校验官方发布 SHA-256，编译器不放入源码包。屏幕 PNG 转换可选 Pillow。

```sh
python3 tools/get_wasi_sdk.py
make test              # 桌面对照、定点对照、面板 ASan/UBSan、浏览器适配测试
make sanitize          # 桌面参考引擎 ASan/UBSan
make panel-host        # 实际 app.c 在模拟面板运行，生成 WAV 和 PPM 屏幕
make web-test          # 编译实际 C 固件为 WASM，验证 PCM / LCD / 参数 / Flash / AudioWorklet
make factory-test      # 六个原始音色各 4 秒干湿对照，包含包络和单音状态
make performance-test  # 旧版逐采样回归、实时编辑历史和等价运算边界
python3 -m http.server 8000 --directory build/web
```

本地浏览器打开 `http://localhost:8000`，点击 Switch on。页面采用 Felucca 的 FM-1 实体布局，显示固件真实帧缓冲；没有独立网页合成器。PRESETS 选择 Tresure Island、Primer、Ai no Labyrinth、Daisy Area 2、Wanna Go To Hawaii、JUDD-ish 2000 与三个用户槽；JSON 文件保持原样并保留 STUDIO-R 归属。浏览器保存模拟 Flash；Web MIDI 由 Connect MIDI 按钮启用。

### FM-1 目标构建

使用 Linux x86-64 的杰理 pi32v2 工具链。已验证归档 `jieli-linux-toolchains-20260730.1.tar.xz`，SHA-256 见 `sources.lock.json`。工具链不放入此源码包。

```sh
sh tools/get_sdk.sh dependencies/ac79-sdk
export JIELI_TOOLCHAIN=/absolute/path/to/jieli-linux-toolchains-20260730.1
export AC79_SDK=/absolute/path/to/ac79-sdk
sh build.sh --firmware
```

SDK 锁定 `AC79NN_SDK_V1.2.1_2023-12-13`，三个打包文件需通过 SHA-256 校验。构建在 `build/target-source` 中临时接入新 app；vendor 中 Dinghy 平台、HAL 和升级 loader 的原始源码保留不动；自有 platform / USB / ISR 与带放置属性的 HAL 在 firmware 下。链接脚本沿用原分区和启动复制机制，将热 DSP、查表、输入扫描和 ISR 包装放入预算内 RAM，其他代码仍在 Flash。构建重新创建暂存源码并检查链接布局、复制镜像与 DSP 地址。HAL 算术适配封装 pi32v2 饱和加减，不改设备寄存器或时钟。

已编译的实验固件：[OpenIFW-FM1-0.4.7-rc1.fwsc](firmware-bin/OpenIFW-FM1-0.4.7-rc1.fwsc)，资源与 SHA-256 见同目录 build.json。

输出 `build/firmware/OpenIFW-FM1-0.4.7-rc1.fwsc`、`openifw.elf`、`openifw.bin`、反汇编和资源报告。实验固件身份为 `FM-1_982`；没有提交到固件 Hub。不要把 Dinghy baseline 包当成本项目固件。

当前大小与预算见 `firmware-bin/build.json` 和 [研究证据](docs/evidence/research-0.4.7-rc1.json)；data+bss 75,600 B / 98,304 B，pool 324,936 B / 344,064 B，RAM code 22,552 B / 24,576 B。编译成功和主机测试不代替真机测试。启动、OCT− 开机救援、USB MIDI、USB Audio、升级协议和 Flash 分区沿用平台；本项目只在原有 4 KiB settings sector 保存三个用户音色和配置。

真机验证步骤和实时预算见 [硬件验证](docs/HARDWARE_TEST.md)。优先使用本项目的 [网页安装器](https://rcell233.github.io/OpenIFW-fm1/install/)，也可沿用 [Dinghy 文档](vendor/dinghy/README.md) 的 `tools/fm1_install.py`；此开发环境没有连接或刷写任何设备。

## 面板与音色传输

按 Ghoulbox 的四卡片布局操作：四个旋钮从左到右直接对应 LCD 四个参数，重复按当前页面按钮或转 SELECT 翻子页，ALGORITHM 跳模块。EDIT 为振荡器与滤波器，ENV 为包络，LFO 为低频振荡器，SEQ 为八槽调制，ARP 为 AMP，SCL 为调音。长按 HOME 打开分类菜单；长按 FX / EDIT / SEQ 使用琴键快捷操作；双击锁定快捷层，再按关闭。

SAVE 打开用户槽页，旋钮选择槽和 LOAD / ERASE / SAVE 操作，OCT+ 执行；覆盖或丢弃编辑需要确认，可输入 12 字符名称。长按 SAVE 撤销 / 重做上次加载。收藏、列表过滤、配色、显示和操作偏好可保存。没有增加音序器、多轨或和弦声部。

USB MIDI 默认全通道接收，CC7 气息、CC9 Vibrato、CC5 Glide、CC64 延音，CC120/123 停音；CC20–23 编辑当前四参数。REC 学习最后编辑参数的 CC，用户槽和设置在声音和事件队列静止后写入。GLO 可设置键盘模式 / CC 气息模式、MIDI 通道、键盘八度和保存槽。GLO 的 PERFORMANCE 页（或长按 HOME → SYS → PERFORMANCE）显示完整 IRQ 的耗时与超时计数。也可在 [设备性能页](https://rcell233.github.io/OpenIFW-fm1/diagnostics/) 只读采集十秒原始数据并下载 JSON。详见 [面板说明](docs/UI_SPEC.md)。

原始 ProgramIndex 为 4、18、14、10、8、21；MIDI Program Change 按这些值选择对应程序。浏览器与目标使用同一处理逻辑。面板中所有矩阵源均可显示，包含原始 Primer 使用的 Pitch Bend (+-)；ARP 实际跳 AMP 参数。

## 验证范围

`tests/installer_test.mjs` 验证真实固件包的完整 USB-MIDI 传输、恢复、错误分支及网页权限流程；不连接真实设备。`tests/ui_test.mjs` 验证 57 页映射、按钮时序、效果器和矩阵隔离、保存确认与撤销，并重放操作做 ASan / UBSan 检查。`tests/ui_audio_unchanged.mjs` 将六个原始程序的演奏和切页过程与冻结的 0.3 音频哈希比较。`tests/web_test.mjs` 检查六个原始音色的数值、矩阵和包络字段，并对相同输入下原生 C / WASM 的 PCM、LCD、patch 和演奏状态逐字节比较；另测模拟 Flash 保存重启和真实 AudioWorklet 的快速按键。`tests/factory_regression.mjs` 直接以未经修改的 OpenIFW JS 为参照，测试每个原始音色各 4 秒干／湿声，保留成对 WAV 和详细数值。效果器误差作为单独测量，不宣称完全一致。

运行结果、已知差异和未验证项见 [测试结果](docs/TEST_RESULTS.md)。未连接实体 FM-1；预览 supervisor 不可用，因此尚不能声称已完成本页面的浏览器点击、触屏和视觉 QA。C 绘制屏幕已导出并检查；Node 中实际 WASM / AudioWorklet 测试不替代浏览器交互测试。

## GitHub 与 Pages

源码仓库：[rcell233/OpenIFW-fm1](https://github.com/rcell233/OpenIFW-fm1)。包含 DSP、固件宿主、交互回归和 GitHub Pages 工作流。Pages 已启用，Source 使用 GitHub Actions。所有资源使用相对路径，支持 `/OpenIFW-fm1/` 项目路径。工作流会构建真实 C / WASM 并先执行测试，再部署。安装器从同一次 Pages 构建读取发布信息和固件；不依赖另一个站点或跨域代理。更新代码 push 到 main 后自动测试并发布。

## 代码与许可

- `firmware/src/`：定点引擎、面板、MIDI、存储与原始音色编码。
- `src/reference/`：桌面 C 对照引擎。
- `web/`：实体面板宿主、模拟 HAL 与 AudioWorklet；`make web` 生成完整静态站点。
- `vendor/openifw/`：未经修改的 MIT 参考引擎及另保留归属的 STUDIO-R 原始音色；`vendor/dinghy/`：未经修改的 GPL 平台文件，排除不可再分发的 logo 图稿及其截图。
- `sources.lock.json`：上游提交、文件哈希和 SDK / 工具链版本。

新增整合代码 GPL-3.0-only；OpenIFW 的 MIT 授权、Dinghy / Ghoulbox / Inter Tight / VT323 / Terminus / JieLi SDK 归属均保留，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。OpenIFW 本身是 IFW 的独立近似，本项目不宣称复刻原生 IFW 的内部实现。
