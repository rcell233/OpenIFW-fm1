# JUDD 正常、Daisy / Tresure 仍爆音：逐模块负载分析

用户最新实机反馈：当前恢复候选可以运行，JUDD-ish 2000 正常；Daisy Area 2、Tresure Island 仍会爆音。本轮分析锁定已发布的 **0.4.7-rc2**，SHA-256 `4f27b9f8fbadc935b9f376fdf506794d79d2f7f31d5c4e319d443a502efa9186`。

**软件测试确认了负载差异及其主要来源，但尚未取得实机欠载计数，不能把“计算较重”直接写成“实机爆音根因已证实”。优先简化混响最划算；LFO、包络以及直接砍掉硬同步都不是当前首选。** 本轮提交分析与可复现测量工具，已发布合成器、预设和固件包保持原值。

## 三个音色实际运行了什么

原始参数见 `vendor/openifw/presets/`；`Bypass: On` 表示关闭效果，不能反读。

| 模块 | JUDD-ish 2000 | Tresure Island | Daisy Area 2 |
| --- | --- | --- | --- |
| 实际求值的波形 | 两路 SAW；其余静音 OSC 仍推进相位和 RNG | 四路 OSC，多种 SAW / SIN / PWM 混合；OSC2 的 SIN 还作为 FM 源 | 三路有音量的周期 OSC，加 EG1 调制的 OSC4 噪声 |
| Hard Sync | 关闭 | OSC2、3、4 开启 | OSC3 开启 |
| 音频速率 FM | 无 | OSC2 双极性波形 → OSC3，深度 1.9 | 无 |
| Filter3 | Stage=22，但 Mix=0，采样循环直接跳过 | Stage=22，但 Mix=0，采样循环直接跳过 | **16 级实际运行**，Mix=5.5，负反馈参数 −8.7，音高调制截止频率 |
| Phaser 效果器 | 关闭 | 关闭 | 关闭；它与 Filter3 是两个不同模块 |
| Chorus / Delay1 / Delay2 | 全部关闭 | Chorus + Delay2 | Chorus + Delay2 |
| Reverb | 开启 | 开启 | 开启 |
| EQ | 低架 +6.4 dB | 低架 +4.3 dB | 0 dB，走恒等快捷路径 |
| LFO / EG | 都推进 2 个 LFO、4 个 EG；没有 LFO 矩阵路线 | 同左 | 同左 |

Daisy 的 Parallel 1 会先相加两个主滤波器的输出，再经过 Filter3 与 AMP1；AMP2=0 不代表其周期 OSC 不参与声音。Tresure 中 OSC2 的 SIN 音量虽然为零，FM 仍需要求它的波形。不能通过删掉“面板音量为零”的调制源省算。

## 实际执行目标固件的旁路测量

使用锁定且带审计补丁的 fm1-emu，执行 **pi32v2 固件指令**。A4 在 1 秒处按下并持续发声；750 ms 时通过原 AudioEvent 队列修改一个待比较参数，由正常 `app_audio_begin()` 消费和重配缓存。1.75–3.25 秒统计，所有实验验证实际 preset 与非零音频。额外基线检查实际单音 held note=69；高音压力用同一事件队列注入 note=108，避免把面板之外的无效琴键当作发声测试。

模型假设：CPU 240 MHz，普通指令 1 周期、除法 48 周期，SPI 外设 60 MHz。**不模拟实际 Flash / cache / 总线等待或 USB 主机，不是周期精确测试。** 下表单位为每 256 帧的模型 DSP 平均毫秒；44.1 kHz 下该音频块长 5.805 ms。DSP 时间含片段内嵌套中断，不含片段之间的 UI / USB 前台工作。

| A4 持续发声 | JUDD | Tresure | Daisy |
| --- | ---: | ---: | ---: |
| 原始音色 | 2.953 | 3.957 | 4.049 |
| 仅关混响 | 1.972 | 2.975 | 3.067 |
| 仅关 Chorus | 2.953 | 3.831 | 3.923 |
| 仅关 Delay | 2.953 | 3.674 | 3.766 |
| Phaser / Chorus / Delay / Reverb 全关 | 1.972 | 2.566 | 2.659 |

后两者比 JUDD 分别多 **34% / 37%** 的模型 DSP 时间。所有有效场景的模型 UND 都为零，说明这些实验没有重现用户实机的爆音；它们用于定位负载与评估简化收益。

| 单项旁路 / 简化 | 节省的模型时间 | 含义 |
| --- | ---: | --- |
| 混响关闭 | 三者都约 **0.982 ms** | 最大单项；约占 Tresure / Daisy 总时间的 25% / 24% |
| Delay2 关闭 | Tresure / Daisy 约 **0.283 ms** | 次于混响；两个 Delay 原本就以 22.05 kHz 处理内部延迟与滤波 |
| Chorus 关闭 | Tresure / Daisy 约 **0.126 ms** | 有收益，优先级低于混响 |
| Daisy Filter3 全关 | **0.264 ms** | 有开销，但解释不了全部差异 |
| Daisy Filter3：16 → 8 级 | **0.113 ms，2.8%** | 会明显改变滤波相位与共振结构，收益偏小 |
| Tresure FM 深度归零 | **0.180 ms，4.6%** | 不能把整段节省量理解为纯 FM 数学成本；也不再需要生成原来只供 FM 的 OSC2 SIN |
| Tresure Sync 全关 | A4 约 **0.005 ms，0.13%** | 这项在本场景很便宜，没有证据支持优先删同步 |
| 低架 EQ 归零 | JUDD / Tresure 约 **0.068 ms** | Daisy 本来就是恒等 EQ；EQ 不是两款共同的主要差异 |
| Exciter Mix 归零 | 约 **0** | 仍然读取两个插值延迟、推进历史和执行混合；改参数不等于实现了旁路 |

各项旁路会改变下游信号和部分求值需求，所以差值是该场景的边际开销，不能机械相加。完整原始样本、高音结果、函数指令计数和输入脚本见 [原始证据](evidence/preset-cost-0.4.7-rc2.json)。

音高 108 的补充测试仍有同样排序：JUDD 2.961 ms、Tresure 4.083 ms、Daisy 4.071 ms。Tresure 关闭同步节省约 0.050 ms，关闭 FM 节省约 0.180 ms；同步开销会随音高变化，但这里仍远小于混响。

按执行指令数，FX+EQ 函数约占三款 DSP 的 **43%–48%**；四个 EG 的 tick 合计约 **1%–2%**。波形辅助函数在 JUDD、Tresure、Daisy 分别约 3%、10%、9%，但它不包含 OSC 的相位、混音、FM 和内联控制计算，不能当作整个 OSC 模块占比。未被路线消费的 LFO 不求调制波形，仍推进相位并每 16 帧更新步长；其成本在本场景很小，省掉它不能解决当前负载。

## 为什么当前混响重，为什么别人的多轨能更轻

`fixed/effects.c` 的混响在 44.1 kHz 每个采样执行 **双声道共 16 个反馈梳状延迟 + 8 个全通延迟**。仅梳状部分每帧就有 48 次阻尼 / 反馈乘法，外加缓冲读写、绕回、截断和立体声混合。缓冲是 16 位，但滤波和乘法依然多用 Q20 与 64 位中间结果，带有通用饱和处理。它是常见拓扑，昂贵之处在规模、运行速率和目标上的实现成本。

Chorus 还要逐采样生成调制延迟、两次插值读取与混合；Delay2 每隔一帧处理两路插值延迟和高低通，并在每帧混合。Tresure 增加了两倍内部采样下的更多波形与 FM。Daisy 增加 16 级 Filter3，每级两次乘法与状态更新。电脑跑同样的 C / WASM 速度充足，无法暴露板上实时余量不足。

其他项目通常采用更窄的有界整数运算、更小的效果器网络、共享效果总线和块级控制。它们的“轨道数”不能直接和完整 VA / 矩阵 / 滤波链数量相比。本项目已经全局共享效果器；还需要改善的是效果器本身。Felucca 也有连续超过 85% 预算后淡出一个声部的过载处理，不能假设其他固件在任意参数下都不会过载。

## 可借鉴的快速实现与建议顺序

源码均已实际克隆并阅读，固定提交见 [参考锁定表](evidence/fm1-references-2026-10-22.json)。

| 实现 | 可借鉴部分 | 在本项目中的取舍 |
| --- | --- | --- |
| [Felucca fx.c](https://github.com/hugelton/Felucca/blob/63f88ec18b71c6ed4371d03d6f04dd93f4193ad3/firmware/src/fx.c) | ROOM：4 comb + 2 allpass；HALL：22.05 kHz 的 8 线 FDN、输入 1–2–1 低通、插值回全速；有界整数 / 块级处理 | **首选参考**。小网络节省大，尾音密度、空间感会变化；半速保留更完整网络，牺牲混响高频 |
| [OberVave obv_fx.h](https://github.com/zvenson/obervave/blob/7e1ccf9ca9e96a14a961e48ea8fe6efa907422df/firmware/src/obv_fx.h) | Q15 延迟与反馈；可选 `REV_DIV=2`，输入成对处理并插值输出 | 参考降速与定点范围管理；其默认 `REV_DIV=1`，不是默认半速。Chorus 用四点 Hermite，比本项目线性插值更复杂，不应直接替换来“提速” |
| [Felucca-Engines dsp.c](https://github.com/hugelton/Felucca-Engines/blob/c46e0086f69bdc26df1cd144dca9e683cc4531dd/src/dsp.c) | Q15 波形、Q13 SVF、明确范围下的 32 位乘法、32 帧增益斜坡 | 借鉴有界运算、系数与采样循环分离；本项目已查表、缓存 BLEP 倒数和固定系数，直接换其 OSC 不保证更快，并会改变音色 |
| [DXsloop dx7_core.c](https://github.com/zvenson/dxsloop/blob/eeec0a6c1c7f9a93865983a8c7f7cc42102e7472/firmware/src/dx7_core.c) | Q24 正弦查表、32 采样块与增益渐变的 FM kernel | 可参考循环组织；固定六算子 FM 与本项目多波形 VA、硬同步、任意矩阵是不同工作量，不能直接换内核 |

建议先实现 **22.05 kHz 的混响湿声网络 + 输入低通 / 输出插值 + 有界 Q15 运算**，干声与核心合成保持全速；按真实采样率缩短各条延迟线，重新计算阻尼，保持尾音时长。不能简单每隔一个采样调用原函数，否则延迟时长翻倍，还会产生镜像。只减半网络计算，理论上可节省这约 0.98 ms 中的接近一半，扣除重采样和全速混合成本后才是实际收益；这不是已完成实现的测量值。

如余量仍不足，再考虑每侧 4 comb / 2 allpass 或共享单声道 tank 加立体声抽头，并校正反馈和 wet 增益。它比只降低 WetDry 真正减少运算。Chorus 的慢调制可块级计算并插值，音频延迟仍逐采样；Delay 已半速，不应盲目继续降到 11 kHz。先省效果器，保留单音、Legato、OSC / FM、EG 曲线、原矩阵及其更新速率；Filter3 减级会改动音色且本次节省很小。

“不爆音”的验收必须使用设备真实 TIMER4、UND / XRUN / DMA 计数，在重音色持续演奏、USB 工作与切页同时进行时测最坏余量。欠载后填零或加限幅器都不能解决算力不足。运行中切换效果质量需要平滑状态与输出，不能临时硬切形成新的点击声。

## 削波与算术检查

直接运行生产 DSP：三款 × MIDI 音高 36 / 48 / 60 / 69 / 72 / 84 / 96 / 108 × 吹气 25% / 50% / 100%，每组 4 秒，共 **72 组、12,700,800 帧、25,401,600 声道采样**。读取真实 FX 输出和 MasterGain，在最终限幅前检查。

- 最终数字满幅削波：0。最大峰值 Tresure −16.75 dBFS、Daisy −10.57 dBFS、JUDD −11.43 dBFS。
- ASan / UBSan 完整重跑通过；正常与 sanitizer 的 72 组结果逐项相同。
- 生产源码冻结检查通过；没有为本轮分析修改核心算法、原始音色或黄金值。

这些结果削弱了“工厂音色最终数字输出自身已经过载”的解释，但不能排除内部非线性、模拟输出失真、实际参数不同或供数中断。已有实机反馈支持负载相关性的推断；根因仍需 [设备性能页](https://rcell233.github.io/OpenIFW-fm1/diagnostics/) 的原始计数确认。

## 复现

先按 README 构建目标固件，使用与该包对应的 ELF 生成符号表。目标模型的锁定版本与审计补丁见 `docs/PERFORMANCE.md`、`tools/fm1-emu-audit.patch`。不要把旧 ELF 的符号用于新包。

```sh
python3 tools/target_symbols.py build/firmware/openifw.elf > build/symbols.json
node tools/dsp_cost_probe.mjs /path/to/fm1_web.wasm firmware-bin/OpenIFW-FM1-0.4.7-rc2.fwsc build/symbols.json build/tresure-baseline.json 0 baseline A4
node tools/dsp_cost_probe.mjs /path/to/fm1_web.wasm firmware-bin/OpenIFW-FM1-0.4.7-rc2.fwsc build/symbols.json build/tresure-no-reverb.json 0 no_reverb A4
cc -O3 -std=c11 -Wno-misleading-indentation -iquotefirmware/src/fixed -iquotefirmware/src tests/preset_headroom.c firmware/src/dsp.c -lm -o build/headroom
build/headroom > build/headroom.json
```

更多探针名见工具中的 `modifications`；preset 0 / 3 / 5 对应 Tresure / Daisy / JUDD。数字音高如 `108` 使用测试工具向固件 AudioEvent 队列注入 note / breath，不是对 UART MIDI 硬件的验证。
