# 0.4.1：硬件声音异常的软件排查

本轮针对「网页正常，真机预设声音低沉且怪异、UI 卡顿」进行代码审查和软件实验，未连接硬件。审查版本为 `4d268389cd23c37d729fc3ae5dde3016cdb0d3f8`。

**已证实：当前音频路径在超时后会丢掉新到的 DMA 中断，造成合成时间推进减半，并把尚未完成格式转换的样本暴露给 DMA。** 这能在软件里制造严重音色异常。**尚未证实：用户机器上的每个预设实际耗时是多少，以及是否还存在芯片指令或时钟差异。** 不能据此宣布真机问题已修好。

## 1. 超过期限 1% 就能复现严重异常

`tests/audio_deadline.py` 提取并编译当前 `vendor/dinghy/firmware/src/platform.c` 中的完整 `fm1_alnk0_irq` 函数体。测试替换外设为逐采样推进的 DMA 模型，给音频回调注入明确的处理时间，生成 440 Hz 正弦。未改写该中断函数的清零、渲染、MASTER、USB tap、Q15 转换或 ACK 顺序。

256 帧 / 44.1 kHz 的期限为 **5.805 ms**。

| 注入的处理时间 | 合成状态每秒推进的帧数，约 | 输出 |
|---|---:|---|
| 4.354 ms（75%） | 44,052 | 正常 440 Hz |
| 5.747 ms（99%） | 44,037 | 正常 440 Hz |
| 5.863 ms（101%） | 22,050 | 440 Hz 主音消失，出现大量边带 |
| 7.256 ms（125%） | 22,042 | 同类严重失真 |
| 10.449 ms（180%） | 22,025 | 同类严重失真 |

有限时长统计含启动的空缓冲，因此正常结果略低于 44,100。上述时间是**实验输入**，不是芯片测量值。

具体顺序如下：

1. 半缓冲 A 播完，中断开始填 A，此时 DMA 正在播 B。
2. 处理超过 5.805 ms，B 也播完，硬件置位一个新的 HALF pending，并开始读尚未准备好的 A。
3. 旧中断最后调用 `fm1_audio_ack_half()`，把这个新 pending 一起清掉。
4. 下一次处理要等又一个半缓冲播完。因此每播 512 帧，合成器只生成 256 帧。

同时，当前代码直接在 DMA 缓冲上清零、写 Q15，最后整块 `<<=7` 转为输出格式。超时时 DMA 会读到清零或转换前的数据。**合成内部时间减半，不等于听到一个正常低八度的音符**：测试产生的频谱含约 86.1 Hz 间隔的边带，最强分量约 478.5 Hz，而原本的 440 Hz 分量几乎消失。

对照音频是纯软件测试音，**不是真机录音**：

- [按时完成的 440 Hz](evidence/audio-deadline-on-time.wav)
- [仅超时 1% 的输出](evidence/audio-deadline-late.wav)

这说明「爆音减少了」不能证明音频已经满足实时期限；稍微超时就可能变成持续的异常声音。

## 2. 计算结果与实时播放分别测试

新增 `tests/dsp_irq_stream.c`，在真正的 ALNK 汇编入口和 C 中断里分块执行 DSP；TIMER5 扫描中断和 UI 主循环继续运行。之前启动自检中的 DSP 在主循环运行，没有覆盖这个上下文。

覆盖 6 个工厂预设，各测带效果和直通，共 12 个场景、196,608 帧。每个场景包含重叠音符、Legato、放开高优先级音符回到前一个音符、呼吸控制及释放尾音。校验 PCM、音高、振荡器相位和包络状态，得到 24 个校验值。

| 执行方式 | 校验结果 |
|---|---|
| 本机 C 参考 | 24 个黄金值 |
| 本机 ASan / UBSan | 24 / 24 一致 |
| 目标固件指令解释器，除法一周期 | 24 / 24 一致 |
| 目标固件指令解释器，除法注入 48 周期，出现超时 | 24 / 24 一致 |

**在这个软件复现里，DSP 仍然算出了正确的样本，坏的是按时交给 DMA 播放的过程。** 这支持优先排查实时性和输出调度；它不能证明物理芯片上的每条指令都与解释器相同。

本轮没有改预设、振荡器、矩阵、包络、效果算法或单音 / Legato 行为。普通构建的 `.fwsc` 与已发布的 0.4.1 **逐字节一致**，SHA-256 仍为：

`cf7ab1e35378ae7797ab817c7e32aaa8ce0b49d3bbbe55185aa18ded7ae2a9b8`

测试模式使用 `-irqtest` 后缀，未作为发布固件或网页安装包。

## 3. 上一轮性能模型漏掉了关键成本

反汇编确认，目标编译器没有消除混响中的大量 `/32`，它们仍是除法指令。`ifw_fixed_fx_tick` 含 29 个除法指令位置，`amp_gain` 另有 `/10`、`/20` 等 3 处。不能把这些全部按一周期来证明真机达标。

在固定 240 MHz 模型下，只改变除法周期假设，保持同一固件、预设和按键脚本：

| 除法成本假设 | 完整音频中断最大耗时，约 | 模型内 DMA 超时 |
|---|---:|---|
| 1 周期 | 4.48 ms | 无 |
| 32 周期 | 5.76 ms | 无，余量仅约 0.04 ms |
| 48 周期 | 6.41 ms | 持续出现，处理块数减半 |

**32 和 48 是敏感性实验输入，不是已知的 pi32v2 除法延迟。** 其余指令仍按一周期计，Flash、cache、总线等待、真实 USB 主机负载仍没有精确建模。这组结果说明旧结论对成本假设很敏感，不能据此换算成设备 CPU 使用率。

相同高延迟假设下，诊断 SINE 的音频中断最大约 0.172 ms、MUTE 约 0.108 ms，均无超时。正常合成路径约 6.41 ms 且超时。这是软件中的路径隔离实验。

## 4. UI 卡顿有独立、确定的带宽问题

当前主循环每 50 ms 请求一次刷新。平时更新中间 128 行和顶部 24 行，共 **152 行**；交互导致 dirty 时更新 **240 行整屏**。

按当前 HAL 注释的 SPI 12 MHz、RGB565 两字节像素：

- 平时一次仅像素传输就需 `240 × 152 × 16 / 12 MHz = 48.64 ms`。
- 整屏需 `240 × 240 × 16 / 12 MHz = 76.8 ms`。

这还没计入绘图、LCD 窗口命令和音频抢占。一次 `draw_panel()` 连续绘完这些条带才回到按键处理，因此操作响应会明显延迟。

另一个测试盲点在第三方模拟器：FM-1 的 SPI1 原先立即完成传输，只有 SMK-37 模式等待线上的字节时间。新增审计补丁为 FM-1 也启用这段时序。在仍然理想化的 CPU 模型里，正常合成时刷新完成间隔已能达到约 116 ms 中位数、192 ms 最大值；除法注入 32 周期后达到约 220 ms 中位数、366 ms 最大值。这些也不是设备实测。

## 5. 尚未排除的设备配置差异

面板八度 `saved.octave` 属于全局配置，切换工厂预设不会重置；硬件会读取已保存的配置，网页新会话通常是默认值。软件隔离测试确认：同一面板 C4 键从 MIDI 60 降三次八度后，六个预设都发送 MIDI 24；从外部传入的 MIDI 60 则仍是 60。这是正常配置行为，不能用它解释失真和 UI 卡顿，但需要与调度问题区分。当前未读取用户设备的实际配置，不能排除它叠加造成“所有预设都低沉”。

## 6. 修复应按什么顺序验证

1. 先减少确定存在的热点成本，例如用严格保持负数向零截断的位移替代 `/32`，对 `/10`、`/20` 测试等价的常数除法实现，并验证全部黄金 PCM / 状态；不能直接改成普通算术右移，先前的 DC 尾音问题会回来。
2. 整理 DMA 缓冲所有权与 pending 的确认顺序，给超时定义明确的恢复行为，避免清掉新事件和播放转换前的数据。**只把 ACK 提前，不能让已经超时的 DSP 自动变快，也不能单独视为完整修复。**
3. UI 改为分步渲染和变更区域刷新，让输入处理在条带之间继续，避免每次等待整幅动态区域。
4. 保留 IRQ 内的校验测试；最终确认实际余量仍需要真机计时。当前软件证据足以定位超时故障机制，尚不能唯一确定用户设备上的触发成本或排除所有硬件差异。

## 7. 复现

本机 DMA 与黄金流测试不需要杰理工具链或硬件；Python 标准库即可，NumPy 若已安装会额外输出 FFT 峰值：

```sh
make hardware-path-test
```

指令模型使用 [fm1-emu](https://github.com/joakimeriksson/fm1-emu) 的 `19177bd79914fcdf527ac423f6689946a0557273` 和 Rust 1.90.0。在干净源码上应用 `tools/fm1-emu-audit.patch`，**该补丁已包含旧 profiling 补丁，二者选用一个**：

```sh
git -C dependencies/fm1-emu checkout 19177bd79914fcdf527ac423f6689946a0557273
git -C dependencies/fm1-emu apply ../../tools/fm1-emu-audit.patch
cargo build --manifest-path dependencies/fm1-emu/Cargo.toml -p fm1-web --release --target wasm32-unknown-unknown

make firmware
python3 tools/target_symbols.py build/firmware/openifw.elf > build/target-audit/symbols.json
IFW_SIM_DIV_CYCLES=48 IFW_SIM_SPI_TIMING=1 node tools/target_perf.mjs \
  dependencies/fm1-emu/target/wasm32-unknown-unknown/release/fm1_web.wasm \
  build/firmware/OpenIFW-FM1-0.4.1.fwsc build/target-audit/symbols.json \
  build/target-audit/overload.json 0 4500
```

上面的 `dependencies/fm1-emu` 可放在仓库内；路径需按自己的目录调整。`make firmware` 使用 README 中的 `JIELI_TOOLCHAIN` 和 `AC79_SDK`。

IRQ 内回归构建：

```sh
IFW_DSP_ISR_TEST=1 make firmware
python3 tools/target_symbols.py build/firmware/openifw.elf > build/target-audit/irq-symbols.json
IFW_SIM_SCRIPT='' IFW_SIM_DIV_CYCLES=48 IFW_SIM_SPI_TIMING=1 node tools/target_perf.mjs \
  dependencies/fm1-emu/target/wasm32-unknown-unknown/release/fm1_web.wasm \
  build/firmware/OpenIFW-FM1-0.4.1-irqtest.fwsc build/target-audit/irq-symbols.json \
  build/target-audit/irq-model.json 0 11000
```

查看 `irqRegression.done` 是否为 1，并将 24 个 hashes 与 [黄金值](evidence/irq-stream-golden.json) 比较。所有测试强制关闭 JIT。

完整数据：[hardware-audio-audit-0.4.1.json](evidence/hardware-audio-audit-0.4.1.json)。现有 `make test` 亦已通过，包括 166 个 DSP 场景、1,170 个 UI 检查及 66 个安装器检查。
