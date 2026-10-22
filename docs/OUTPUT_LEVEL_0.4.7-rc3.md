# 0.4.7-rc3 音量偏小：输出电平对比

**结论：当前出厂音色的整体输出电平偏保守，用户需要把 MASTER 开得很大的反馈有代码和软件测量依据。主要差别在进入 MASTER 之前的信号电平；MASTER 曲线、DAC 初始化和 I2S 缩放沿用其他 FM-1 固件。**

本轮核查锁定当前发布的 0.4.7-rc3，SHA-256 `e5f1a8cf944debd3b79bd0f2939f7d8a5e3f8ecc0836644d68291234c9f70f7d`。本提交增加复现工具和测量结果，当前固件包与 DSP 保持原值。

## 对比方法

直接编译和运行各项目的 C 音频实现。OpenIFW 测量六个未修改的出厂音色、8 个音高（MIDI 36/48/60/69/72/84/96/108）、2 个力度（面板实际使用的 110，以及 MIDI 满力度 127）、3 个 breath 值（25%/50%/100%），每个场景持续发声 4 秒，共 **288 个场景**。另测 Dinghy、Felucca、Ghoulbox 共 **26 个单音场景**，保留各自音色、包络、音轨电平和效果器默认值。

参考源码固定在 [既有提交锁定表](evidence/fm1-references-2026-10-22.json)。原始输出、电平、溢出计数及本轮源码 SHA-256 见 [完整证据](evidence/output-level-0.4.7-rc3.json)。

以下统一采用 A4（MIDI 69）、力度 110、单音持续 4 秒、MASTER 理论单位增益，算入 Q15 → 24 位 DAC 的 ×128 缩放。真实 ADC 满量程 1023 对应的 MASTER 比单位增益再低 0.017 dB，未计入表格。数值越接近 0 dBFS，数字电平越高；RMS 是整个 4 秒的平均能量，不是 LUFS 或实测声压。

| 固件 / 原厂音色 | DAC 峰值 dBFS | DAC RMS dBFS |
| --- | ---: | ---: |
| OpenIFW / Tresure Island | −25.55 | −31.82 |
| OpenIFW / Primer | −22.61 | −30.41 |
| OpenIFW / Ai no Labyrinth | −23.24 | −32.40 |
| OpenIFW / Daisy Area 2 | −16.80 | −26.60 |
| OpenIFW / Wanna Go To Hawaii | −16.68 | −28.61 |
| OpenIFW / JUDD-ish 2000 | −20.59 | −31.81 |
| Dinghy / sine | −16.42 | −19.43 |
| Felucca / SAW LEAD | −10.99 | −17.97 |
| Felucca / ACID | −12.01 | −22.49 |
| Felucca / SINE KEY | −11.87 | −23.94 |
| Felucca / FM6 PAD | −17.77 | −28.19 |
| Ghoulbox / SAW LEAD | −10.96 | −17.66 |
| Ghoulbox / DARK STRINGS | −13.55 | −23.58 |

例如本场景 Tresure、JUDD 的 RMS 比 Felucca SAW LEAD 低约 14 dB，Daisy 低约 9 dB。但这些是不同音色，频谱、包络、峰均比不同，不能据此断言整个固件在任何情况下都小固定的几 dB。Felucca 的 PAD 本身也较轻，衰减型电钢琴在后半段会更轻。完整证据同时保留 2～4 秒的稳态 RMS，避免用一次攻击峰值代替平均音量。

## 音量在哪些地方降低

1. **原始合成输出标定偏低。** 原 OpenIFW 的 `OSCILLATOR_GAIN=0.037` 在整数引擎中按原值保留；效果器处理之后才乘音色的 `MasterLevel`。本轮没有把这个系数改大，避免改变效果器驱动电平与音色。出厂 MasterLevel 多数已经是 10；Primer 为 8.1，JUDD 为 9。
2. **MASTER 使用平方曲线。** `gain ≈ (ADC/1024)²`，约半程是 0.25 倍（−12.04 dB），四分之三程约 −5 dB，拧到底接近单位增益。启动变量 `master_q12=2048` 会被 ADC 读数替换，不能据此说固件一直固定减半。Felucca、Ghoulbox、Dinghy 采用相同平方曲线。
3. **DAC 输出保留 6.02 dB 余量。** Q15 样本乘 128 送往 24 位 I2S，而乘 256 才会使用完整 24 位范围。这是上述几个固件共享的输出约定。四者 `fm1_audio.h` 完全相同，SHA-256 均为 `22351440699748546e932098e3016c618d6137dd0ae34557e08a107641d4e62e`，没有发现本项目额外调低 DAC 寄存器增益。

例如 Tresure 在表中 RMS 约 −31.82 dBFS，MASTER 半程后约为 −43.86 dBFS。这能解释为什么旋钮要接近最大才够响。Felucca / Ghoulbox 的音色以较高电平进入输出总线，并带有总线峰值限制和软削波，输出电平的管理方式与目前 OpenIFW 的保守余量不同。

网页版也模拟相同的平方曲线与 6 dB 输出余量；电脑播放设备的模拟放大、系统音量和 FM-1 的扬声器 / 耳机输出不同，网页听着响不能证明设备端具有相同声压。

## 可以提高多少

把测得的实际 Q15 PCM 在最终输出级统一放大，保留现有 ±32767 输出边界，得到：

| 输出增益投影 | 288 个场景中会超出 Q15 边界的场景 | 超限声道采样数 |
| --- | ---: | ---: |
| 原值 | 0 | 0 |
| ×2，约 +6.02 dB | **0** | **0** |
| ×4，约 +12.04 dB | **26** | **65,948** |

最大峰值来自 Wanna Go To Hawaii、MIDI 36、满 breath，原峰值为 Q15 的 0.47894（−6.39 dBFS）；×2 后为 0.95789，距 Q15 边界约 0.37 dB。DAC 端仍有现有的约 6 dB 余量。×4 会使 Daisy、Wanna Go To Hawaii、JUDD 的部分场景超限。

**合理的第一步是效果器之后、物理 MASTER 之前统一增加约 6 dB 的输出增益。** 这样保留音色之间的相对关系，并且不改变 OSC/FM、滤波器、包络、矩阵、单音 / Legato 或效果器的输入电平。单纯改旋钮曲线只能改善中间位置，无法提高旋钮全开时的最大音量；直接使用 +12 dB 会引入已测到的削波风险。

上述增益结论只覆盖本轮 4 秒持续音的出厂场景，不覆盖任意编辑参数、所有音符切换或任意长尾叠加。更高输出或任意编辑场景仍需要独立验证最终输出保护，不能把 288 次无超限写成实机绝不破音。本轮没有测量模拟输出电压、耳机声压或扬声器失真。

## 复现

准备锁定表中的 Dinghy、Felucca、Ghoulbox 克隆，Felucca 的 engines 子模块，以及各项目生成采样头所需的 Python 依赖。在仓库根目录执行：

```sh
python3 tools/audit_output_level.py --references ../dependencies
python3 tests/ui_engine_unchanged.py
```

输出为 `build/volume-audit/report.json`，上游生成 / 编译日志为同目录 `build.log`。测量程序只运行原引擎，没有修改参数来追平不同音色。上游原始源码和本项目引擎冻结检查均通过；本提交不改变安装器默认固件或包的 SHA-256。
