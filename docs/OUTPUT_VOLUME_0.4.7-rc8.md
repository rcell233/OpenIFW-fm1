# rc8 音量复核：与当前 Felucca、Ghoulbox、Melodee 比较

2026-11-01。结论：**rc8 的 +6.02 dB 校准仍然有效，但部分出厂音色的平均输出仍比参考固件的响亮音色低约 5–8 dB。应优先评估 DAC 输出标定，而不是继续提高现有 Q15 限幅之前的增益。** 旋钮全开时，改旋钮曲线解决不了最大音量。不同固件并不是所有音色都更响。

本轮交付分析、可复现脚本和数据；发布固件仍为 rc8，安装器、DSP、预设与网页输出没有修改。rc8 包 SHA-256 仍为 `1f3555ab029408ae1c7e947a92b14f28bca68fbc5a73e0918445f0af9c8af75f`。

## 实际测了什么

重新编译 rc8 的生产 C DSP：六个原厂音色 × 八个音高 × 两个力度 × 三个 breath 值，共 **288 个四秒场景**；另运行原有的 **600 秒持续音、Legato、重复音、气息变化和释音测试**。

克隆并锁定以下当前源码，编译各自的原生音频实现，保留原音色、默认轨道电平、当前全局效果器、总线限幅与软削波。选择 21 个出厂音色，在 C3/A4、力度 110/127 下各持续四秒，共 **84 个对照场景**。Ghoulbox 已从菜单退休的 SAW LEAD 等未作为当前预设使用。

| 项目 | 锁定提交 | 场景数 |
|---|---|---:|
| [Felucca](https://github.com/hugelton/Felucca/tree/406b328ee43ffb8fd3d2eb4cce1a741db4a4cea1) | `406b328` | 28 |
| [Ghoulbox](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth/tree/9f2b9a22e456d7fe9552b40f7a1d0c13200913bb) | `9f2b9a2` | 20 |
| [Melodee](https://github.com/keremimo/melodee/tree/c14912ba911d4f4fefaa07120eb068a91d7ccb78) | `c14912b` | 36 |

原始数据、源码哈希、子模块提交和增益投影在 [测量证据](evidence/output-volume-2026-11-01.json)。测的是数字样本；没有测量设备的模拟输出电压、声压或 DAC 重建后的真峰值。这里的 RMS 不是 LUFS，频谱和包络不同的音色不能视为等响素材。

## 满档输出对比

下表统一为 A4、力度 110、四秒持续音、MASTER 理论单位增益。OpenIFW 使用键盘模式对应的满 breath。计入各项目实际的 Q15 ×128 → 24 位 DAC 缩放，数值越接近 0 dBFS，数字电平越高。

| 固件 / 原厂音色 | DAC 峰值 dBFS | DAC RMS dBFS |
|---|---:|---:|
| OpenIFW rc8 / Tresure Island | −19.51 | −25.79 |
| OpenIFW rc8 / Primer | −16.57 | −24.39 |
| OpenIFW rc8 / Ai no Labyrinth | −17.15 | −26.32 |
| OpenIFW rc8 / Daisy Area 2 | −10.78 | −20.58 |
| OpenIFW rc8 / Wanna Go To Hawaii | −10.60 | −22.55 |
| OpenIFW rc8 / JUDD-ish 2000 | −14.52 | −25.78 |
| Felucca / SAW LEAD | −10.99 | −17.97 |
| Felucca / SOFT PAD | −11.75 | −19.51 |
| Felucca / FM6 PAD | −17.77 | −28.19 |
| Ghoulbox / HURDY GURDY | −11.41 | −20.41 |
| Ghoulbox / DARK STRINGS | −13.55 | −23.58 |
| Melodee / It's a Prophet 5 | −14.60 | −20.10 |
| Melodee / CZ-1 BRASS 1 | −11.62 | −18.08 |
| Melodee / FM6 PAD | −17.67 | −29.03 |

Tresure/JUDD 比 Felucca SAW LEAD 低约 7.8 dB，比 Melodee 的启动 Prophet 音色低约 5.7 dB；Daisy 已接近 Ghoulbox 的 HURDY GURDY。参考固件自己的 FM6 PAD 反而比当前这些 OpenIFW 音色更轻。不能把某个响亮音色的差值当成整个固件统一欠缺的增益。

平均电平还受动态影响：同场景 JUDD 峰均比约 11.26 dB，SAW LEAD 约 6.98 dB。只把峰值对齐，不会得到相同平均响度。

## 增益链路核查

1. **原合成标定仍然保守。** 原 OpenIFW 混合级的 `0.037` 在定点实现中仍等效保留，之后经过效果器和音色 MasterLevel。MasterLevel 按原版使用平方曲线；大多数出厂音色已经设为 10，Primer 为 8.1、JUDD 为 9。这些值本轮没有改。
2. **最终 +6.02 dB 没有丢失。** `output_level.h` 中仍为 `IFW_OUTPUT_GAIN=2`，与物理 MASTER 合并后才限幅到 ±32767。rc7/rc8 的性能优化未撤掉此增益。
3. **MASTER 满档不是固定减半。** `master_q12=(k10*k10)>>8`；理论 ADC=1023 映射到 4088/4096，只有约 −0.017 dB。整数平滑的稳态量化可略低，实际 ADC 最大读数未测量。这不足以解释测得的数 dB 差距。四个项目都使用平方曲线。
4. **DAC 与 USB 的归一化电平相差约 6 dB。** 现有 Q15 样本乘 128 送 24 位 DAC，只使用其约一半幅度；乘 256 才对应完整 24 位范围。USB 直接取 Q15，因此按各自数字满幅归一化，USB 比 DAC 高 6.02 dB。网页除以 65536，模拟的是当前 DAC 电平。这个差值不等于电脑耳机一定响 6 dB，电脑还有独立的音量和模拟输出级。
5. **参考项目也使用 ×128。** 它们较响不是因为 DAC 全部开到了 ×256。Felucca 新增的 `uac_tap_q15()` 内有 ×256，但那是 USB 格式适配，随后会还原；其物理 DAC 仍由 `OUT_SHIFT=7` 缩放。Ghoulbox、Melodee 也相同。
6. **参考固件的总线处理不同。** 三者都有阈值 18000 的峰值限幅与最终软削波。原生渲染中，Felucca SAW LEAD、SOFT PAD、Ghoulbox GURDY 和 Melodee CZ Brass 的部分块末限幅器确实处于作用状态；不是只在源码中存在但从不运行。配合音色本身的电平和包络，它们能得到较高平均电平。这不代表给 OpenIFW 加压缩器就能保持原动态。

相关源码：[OpenIFW 输出校准](../firmware/src/output_level.h)、[DAC/USB 分流](../firmware/src/audio_output.h)、[Felucca 总线](https://github.com/hugelton/Felucca/blob/406b328ee43ffb8fd3d2eb4cce1a741db4a4cea1/firmware/src/fx.c)、[Felucca 输出与 USB 适配](https://github.com/hugelton/Felucca/blob/406b328ee43ffb8fd3d2eb4cce1a741db4a4cea1/firmware/src/audio.c)、[Ghoulbox 输出](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth/blob/9f2b9a22e456d7fe9552b40f7a1d0c13200913bb/firmware/src/audio.c)、[Melodee 总线](https://github.com/keremimo/melodee/blob/c14912ba911d4f4fefaa07120eb068a91d7ccb78/firmware/src/mix.c)。

## 两种加大音量的方法，结果不同

最响测试峰值来自 Wanna Go To Hawaii 的低音，已达到 **0.963562 Q15**。现有 Q15 限幅边界只剩约 **0.32 dB**。直接把限幅前的最终软件增益调大，在出厂场景中已经会产生削波：

| 在 rc8 现有软件增益上追加 | 超出 Q15 边界的场景 / 288 | 超限声道采样数 |
|---|---:|---:|
| +3.01 dB（×√2） | 6 | 4,900 |
| +3.52 dB（×1.5） | 10 | 7,918 |
| +6.02 dB（×2） | 28 | 67,988 |

另一种方法是在已经限幅的 Q15 输出之后，调整 **Q15 → DAC 的线性标定**：

| DAC 换算系数 | 相对 rc8 | 已测最大 DAC 幅度 | 峰值余量 |
|---|---:|---:|---:|
| ×128，现状 | 0 dB | 0.4818 | 6.34 dB |
| ×181 | +3.01 dB | 0.6813 | 3.33 dB |
| ×192 | +3.52 dB | 0.7227 | 2.82 dB |
| ×256 | +6.02 dB | 0.9636 | 0.32 dB |

这些是同一批真实 PCM 的线性投影，并非声卡/扬声器实测。数学上，只要现有 ±32767 输出边界继续成立，乘 256 的最大绝对值为 8,388,352，仍在有符号 24 位范围内。这种改法不会新增数字削波，也不改变原有削波；不能据此保证模拟放大器、DAC 重建峰值或小扬声器在最大音量下没有失真。

## 建议的改动

**下一版优先增加独立的输出校准档：原电平、约 +3 dB、约 +6 dB。先以 +3 dB 作为候选默认值，+6 dB 作为高输出档进行实机验收。** 增益应放在效果器、音色 MasterLevel、物理 MASTER 和既有 Q15 限幅之后的 DAC 转换处。它是普通线性电平调整，无需加入压缩器或另一套效果算法；×256 只需把原常量移位改一位。

这样的 +6 dB 档会把 Tresure/JUDD 的本场景 DAC RMS 提到约 −19.77/−19.76 dBFS，接近参考固件中约 −18～−20 dBFS 的响亮音色；Daisy 则会达到约 −14.56 dBFS。因此保留选择档位比假定所有音色都欠同样的增益更合适。

- **OSC、FM、滤波器、包络、调制、Mono/Legato 和原始预设保持原值。** 提高 OSC/效果器输入会改变非线性处理和反馈，不能当作透明的总音量校准。
- **网页需要同步所选 DAC 电平。** 避免硬件更响而 Demo 仍停在旧缩放。
- **USB 不跟随 DAC 档位再放大。** 它已经按 Q15 满幅输出。若反馈来自 USB 录音，应单独检查宿主输入电平与录音模式；直接再加 +3/+6 dB 会遇到上表的 Q15 余量限制。
- **底噪不会凭空消失。** 线性增益同时提高现有数字信号与数字噪声，二者比值不变。提高相对于后级固定模拟底噪的信号电平可能有益，但实际底噪和失真需要设备测量。

实机验收重点是耳机/线路和扬声器分别比较、低音 Wanna 的满 breath、密集 Daisy、长混响尾音，以及 MASTER 满档的 ADC 实际读数。数字范围足够不等于模拟级已经验收。

## 验证与复现

`make output-level-test` 已通过：524,288 个增益算术边界、288 个出厂场景、600 秒长演奏均无当前输出边界采样；ASan/UBSan 通过。新增两档增益投影的测量保留原有全部结果，逐字段比较一致。原始来源校验及核心/音色冻结检查通过；没有重写黄金音频。

先克隆上表三个仓库到与 `OpenIFW-FM1` 同级的 `Felucca`、`Ghoulbox`、`Melodee` 文件夹，分别检出锁定提交；Felucca 另执行 `git submodule update --init`。然后：

```sh
make output-level-test
python3 tools/review_output_volume.py
python3 tests/ui_engine_unchanged.py
python3 tools/verify_sources.py
```

输出在 `build/volume-review/report.json`，原生编译日志在同目录 `build.log`。脚本拒绝错误的上游提交或修改过的上游源码，并核对之前生产电平测试对应的源码哈希。
