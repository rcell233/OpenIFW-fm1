> **当前 0.4.4：** 持续超时会让旧版音频中断占满 CPU、主循环无法喂看门狗；已加入有界让出保护并完成 10 秒过载对照。保留原平台 XIP 布局。见 [救援与本版说明](RECOVERY_0.4.4.md)。历史模型结果不能代表实体启动或实际 CPU 耗时。

# 0.4.1 性能评估与硬件爆音诊断

**后续硬件反馈：0.4.1 爆音减少，但声音仍异常、UI 卡顿。无硬件排查已复现超时后丢失新 DMA 事件的机制；详见 [音频路径审查](HARDWARE_AUDIO_AUDIT.md)。下文的指令模型通过不代表真机已达标。**

0.4.0 的 Demo 正常，用户实机反馈 UI 正常而声音严重爆音。旧版没有验证杰理 CPU 的实时预算，电脑端通过不能说明目标芯片能按时填完 DMA。0.4.1 对同一合成算法做等价优化，并加入真机完整音频中断计时和独立输出诊断。

**下面的毫秒数据来自实际编译固件的 pi32v2 指令模型，不是真机实测。** 模型按 240 MHz、每条指令一周期推进外设；不精确模拟双发射、乘除法延迟、cache、Flash/总线等待或连接的 USB 主机。它用于比较实现和发现明显预算风险，最终应以设备 PERFORMANCE 页及实际声音为准。

## 固件指令模型

采样率 44,100 Hz，每半区 256 帧，名义预算 **5.805 ms**。A/B 基线使用锁定 0.4.0 DSP、原来的独立文件 `-Os` 编译，并接入与新版相同的完整 IRQ 统计。新版使用本次优化代码及 `-O3` 合并编译。两者均保留原预设与输入逻辑。

每个预设模拟 4.5 秒，PRESETS 编码器实际选择对应工厂程序，1 秒后收集耗时；脚本包含 C4 / E4 重叠、松键回退及尾音。表中峰值为此采集窗口的最大完整 IRQ 耗时；XRUN / DMA 为该次启动全过程的计数。测试工具另外读取设备程序编号，确认音色已加载。

| 工厂音色 | 0.4.0 模型峰值 | 0.4.1 模型峰值 | 减少 | 新版 XRUN / DMA |
| --- | ---: | ---: | ---: | ---: |
| Tresure Island | 13.274 ms | 4.483 ms | 66.2% | 0 / 0 |
| Primer | 11.633 ms | 3.873 ms | 66.7% | 0 / 0 |
| Ai no Labyrinth | 12.597 ms | 4.103 ms | 67.4% | 0 / 0 |
| Daisy Area 2 | 15.296 ms | 4.479 ms | 70.7% | 0 / 0 |
| Wanna Go To Hawaii | 11.639 ms | 4.018 ms | 65.5% | 0 / 0 |
| JUDD-ish 2000 | 11.736 ms | 3.966 ms | 66.2% | 0 / 0 |

六个工厂预设在该模型场景均保持 XRUN / DMA 为 0。旧版运行超过半区周期，确有导致设备读取尚未完成音频块的风险；模型证据支持性能不足这一排查方向，尚不能单独证明用户实机的全部噪声原因。

## 压力场景与边界

压力配置为原 Tresure Island 加 **8 路 LFO × EG → FM、Filter 3 的 24 级、五个效果器全开、MIDI 108 高音**。振荡器、2 倍内部采样、效果器历史和所有调制链路继续运行。

旧版模型完整 IRQ 峰值 **23.486 ms**，新版 **6.705 ms**；新版稳定窗口中位数 **6.522 ms**，371 个回调中 XRUN / DMA 为 **370 / 370**。

这个场景在 0.4.1 **仍未达到实时预算**，不是“所有自定义组合均能实时运行”的证明。版本保留其音色与计算行为；编辑重负载组合时需要看设备读数。对普通工厂程序的改善也不能替代 USB Audio 并发、长时间演奏及真实芯片上的验证。

## 输出一致性

- 166 个既有场景的 **332 个 PCM / 演奏状态文件**，SHA-256 与 0.4.0 完全一致。
- 六个原始程序 × 演奏 / 切页共 12 组、**1,228,800 个立体声帧**，与冻结的 0.3 WASM 音频逐字节一致。
- 新增 **32,768 帧、1,024 次实时 patch / FX 切换**，包括全部波形 CV、FM 顺序、EQ 进入/离开零增益、延迟历史和单音状态；原生独立编译、`-O3` 合并编译，以及实际 pi32v2 编译固件的指令执行均匹配旧版校验值。
- 100,000 组随机运算场景及边界，检查饱和加减、宽整数饱和、64/32 除法、波形组合和插值归一化；目标自测错误数为 0。
- 1,098 个工厂参数、192 个矩阵字段、120 个包络字段继续通过检查。Mono / Legato、预设 JSON、EG 曲线和表格未改。

保留原 `ui-engine-baseline.json`，另外显式记录五个性能修改文件和两个目标适配文件，不把它们冒称为“源码未改”。所有音频黄金数据保持原值。`make test` 和 `make sanitize` 包含原生 C / WASM、面板、完整 UI、安装器以及 ASan / UBSan 检查。

## 主要改动

将逐采样的固定 FX 参数、Master gain、未变化的音高/滤波/滑音系数缓存；普通调制仍每 16 输出采样更新，FM 仍逐内部采样计算。EG / LFO 等在同一输出采样内不变的调制源可复用，振荡器和噪声 CV 按原顺序实时读取。

环形延迟读用等价的条件回绕替代取模；相同边缘的波形校正复用；固定 Reverb 拓扑展开以减少长度、偏移与索引工作。零系数和精确单位 EQ 走同结果路径，保留旧滤波状态。通过明确的数值范围去掉不可能触发的饱和检查，保留每个乘积的移位与舍入。

目标使用 pi32v2 饱和加减与精确 64/32 除法；寄存器/时钟/音频 HAL 仍沿用原平台。调制数组按对齐 32 位清零，DSP 模块合并编译以便内联。没有改变原有音频精度和算法配置。

## 主机相对测试

同一桌面环境、相同 `-O2`、每块 256 帧、每场景 768 个计时块，旧/新交替运行五轮；使用线程 CPU 时间，校验输出一致。14 个干/湿及压力场景的中位块耗时下降约 **19%–27%**。这些是桌面相对结果，不换算成 FM-1 CPU 占用；完整数值保存在证据 JSON。

## 资源与真机读数

正常发布包身份 FM-1_982，app **223,880 B / 581,564 B**，data+bss **28,048 B / 98,304 B**，pool **294,216 B / 344,064 B**，均在预算内。

固件包 SHA-256：`cf7ab1e35378ae7797ab817c7e32aaa8ce0b49d3bbbe55185aa18ded7ae2a9b8`。保留原 0.4.0 包供追溯；安装页发布的是正常 0.4.1 构建。

完整 IRQ 包含应用 DSP、事件与配置、示波器/电平统计、MASTER 缩放、USB tap、输出格式转换和 TIMER5 抢占。XRUN 检查处理时间；DMA 同时检查半区切换与超周期，避免漏掉跨越偶数次切换的长回调。

在设备打开 **GLO → PERFORMANCE** 或 **长按 HOME → SYS → PERFORMANCE**，K2 向右 RESET 后演奏；观察 MAX / XRUN / DMA。K1 的 SINE / MUTE 直接生成正弦音或全零以隔离 DSP；退出此页自动恢复 SYNTH，不保存诊断模式。网页显示 N.A.，不会把电脑耗时当作设备数据。具体步骤见 [真机验证](HARDWARE_TEST.md)。

## 复现

正常目标编译仍是 `make firmware`，JIELI_TOOLCHAIN 和 AC79_SDK 使用 README 中锁定的版本。模型依赖独立的 MIT 项目 [fm1-emu](https://github.com/joakimeriksson/fm1-emu)，固定提交 `19177bd79914fcdf527ac423f6689946a0557273`，Rust 1.90.0 与 wasm32-unknown-unknown target。

```sh
mkdir -p build/performance
# 在项目根目录；依赖目录不提交到本项目。
git clone https://github.com/joakimeriksson/fm1-emu dependencies/fm1-emu
git -C dependencies/fm1-emu checkout 19177bd79914fcdf527ac423f6689946a0557273
git -C dependencies/fm1-emu apply "$PWD/tools/fm1-emu-profile.patch"
cargo build --manifest-path dependencies/fm1-emu/Cargo.toml -p fm1-web --release --target wasm32-unknown-unknown
make firmware
python3 tools/target_symbols.py build/firmware/openifw.elf > build/performance/symbols.json
node tools/target_perf.mjs dependencies/fm1-emu/target/wasm32-unknown-unknown/release/fm1_web.wasm build/firmware/OpenIFW-FM1-0.4.1.fwsc build/performance/symbols.json build/performance/preset-0.json 0 4500
```

本工作区依赖放在项目同级目录，执行时相应调整路径。参数 0–5 选择六个工厂音色；`IFW_SIM_PROFILE=1` 可输出指令地址归属，`IFW_SIM_SCRIPT` 可指定物理按键/编码器脚本。工具强制关闭 JIT；补丁增加只读内存/错误访问、PC 计数，并依据杰理 objdump 补全标量饱和加减及旋转编码。目标黄金输出校验用于约束模拟器的运算正确性，时间模型仍非周期精确。

```sh
# 仅用于测试的自测 / 压力构建，产生带 -test / -stress 后缀的包。
IFW_DSP_SELFTEST=1 make firmware
IFW_DSP_STRESS=1 make firmware
# 压力包会自动演奏高音；配合 IFW_SIM_SCRIPT='' 避免脚本换音。
# 旧版 DSP 基线先从锁定提交提取，再由构建器校验原文件哈希。
mkdir -p build/perf-baseline/source
git archive 4ce7b7dd632cb08b3346a29ce0551293f71831da firmware/src | tar -x -C build/perf-baseline/source
IFW_DSP_BASELINE_DIR="$PWD/build/perf-baseline/source/firmware/src" make firmware
# 测试后用正常 make firmware 恢复生产构建；安装页只发布正常 0.4.1 包。
```

自测与压力包仅供模拟评估，不是安装页发布物。`tests/perf_bench.c` 配合 `tools/desktop_perf.py` 进行主机 A/B；`tools/summarize_performance.py` 核对固件 SHA、实际预设、模拟器版本和黄金输出再汇总证据。

完整结果：[performance-0.4.1.json](evidence/performance-0.4.1.json)。真实硬件 CPU、模拟输出、USB Audio、浏览器现场安装及板上长时间稳定性仍待用户设备验证。
