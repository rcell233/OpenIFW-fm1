# 0.4.7-rc3：效果器减负候选

针对用户实机上 JUDD 正常、Daisy / Tresure 仍爆音的反馈，本版实现较小的半速混响和更轻的湿干混合。**已提供安装包，实机声音仍待验收。**

[Install 0.4.7-rc3](https://rcell233.github.io/OpenIFW-fm1/install/?candidate=0.4.7-rc3&v=0.4.7-rc3) · [下载固件](https://rcell233.github.io/OpenIFW-fm1/firmware/OpenIFW-FM1-0.4.7-rc3.fwsc) · [回退 0.4.7-rc2](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.7-rc2&v=0.4.7-rc3) · [原始证据](evidence/fx-0.4.7-rc3.json)

## 实现

- 混响湿声从 44.1 kHz 的双侧共 16 comb / 8 allpass，改为 **22.05 kHz、共享 4 comb / 2 allpass、两个立体声抽头**。这是更轻的 room 效果，空间扩散与高频尾音会变化。
- 输入使用 1–2–1 低通抽取，输出线性插值回 44.1 kHz；延迟长度按采样率缩短，阻尼记忆系数平方。较短 comb 的反馈与两个抽头的音量经过校正，Size、Width、WetDry 继续使用原参数。
- Tank 用有界 Q15 状态和 32 位乘法，反馈向零截断，尾音最终回到精确零。没有在逐采样循环计算系数或申请内存。
- Chorus / Delay 的湿干混合使用一次凸插值乘法；有效参数下，该混合步骤的舍入差至多一个 Q20 LSB。没有改 Delay 时长、反馈或滤波拓扑。
- OSC、音频速率 FM、两倍内部采样、主滤波器、Filter3、EG、LFO、矩阵、单音与 Legato 的算法源码不改；六个原始音色的参数不改。DSP 仍由原来的 32 帧前台片段生产完整音频块，沿用 rc2 的原始 HAL、Flash 中断入口、链接布局和升级 loader。

实现借鉴 [Felucca 的小型 ROOM 与半速 HALL](https://github.com/hugelton/Felucca/blob/63f88ec18b71c6ed4371d03d6f04dd93f4193ad3/firmware/src/fx.c)、[OberVave 的 Q15 与可选半速处理](https://github.com/zvenson/obervave/blob/7e1ccf9ca9e96a14a961e48ea8fe6efa907422df/firmware/src/obv_fx.h)。本版只精简 FX，保留现有合成器。

## 目标指令模型中的改善

同一锁定模拟器、240 MHz、普通指令 1 周期、除法 48 周期、SPI 60 MHz 假设；A4 在第 1 秒按下并持续演奏，统计 1.75–3.25 秒。**这些是模型耗时，未模拟实际 Flash / cache / 总线等待和 USB 主机，不是设备实测。**

| 音色 | rc2 每 256 帧平均 DSP | rc3 | 降低 |
| --- | ---: | ---: | ---: |
| Tresure Island | 3.957 ms | **3.194 ms** | **19.3%** |
| Daisy Area 2 | 4.049 ms | **3.284 ms** | **18.9%** |
| JUDD-ish 2000 | 2.953 ms | **2.186 ms** | **26.0%** |

其余三款在同一模型中：Primer 2.225 ms，Ai no Labyrinth 2.612 ms，Wanna Go To Hawaii 2.539 ms。音高 108 的额外检查：Tresure 3.320 ms、Daisy 3.306 ms。八个场景都验证了实际 preset、单个 held note 和非零音频；UND 为零。

Daisy 连续演奏 **32 秒**：DSP P99 3.327 ms、最大 3.354 ms，UND / XRUN / DMA 均为零；主循环最大间隔 1 ms，绘图间隔中位数及 P95 都为 50 ms。这不是实机帧率承诺。

8 槽 LFO×EG→FM、24 级 Filter3、全部 FX 的 **10 秒压力模型**：DSP 最大 5.695 ms，UND / XRUN / DMA 为零，主循环最大间隔 2 ms。上一 rc2 压力记录有 174 次缺块、DSP 最大约 6.474 ms。**新压力结果距 5.805 ms 块预算仍很近，不能据此保证任意自定义音色在设备上都满足实时要求。**

## 声音与回归

- 生产半速路径：**166 个音乐状态轨迹、158 组干声 PCM** 与旧结果逐字节相同，覆盖各种波形、全部矩阵来源 / 目标、FM、滑音和单音事件。
- 原来的 332 份 PCM / 状态、实时编辑哈希、源捕获与 12 组 UI 音频黄金值保留原值，使用显式全速参考构建单独回归。全速参考 WASM 位于测试目录，不部署到 Pages；正式 Demo 与正式固件都使用新 FX。
- 实际 pi32v2 生产路径的 **196,608 帧、24 个流哈希** 与原生 C 对应半速构建一致；目标全速参考的 16 个旧哈希不变，数学错误为零。
- 三款 × 八个音高 × 三种吹气强度，共 **72 组、12,700,800 帧**，最终限幅前无数字满幅削波；ASan / UBSan 与普通构建的结果一致。生产 FX 的随机实时编辑也通过 ASan / UBSan。
- 三种混响 Size 的脉冲检查：湿声首次到达时间不变，1 kHz 低通加权能量相对旧版为 −0.06 / +0.20 / +1.22 dB。40 秒测试均衰减到精确零；混响高频和扩散结构不宣称与旧版一致。
- `make test`、`make sanitize` 与独立 `reverb-test` 通过。原生 C / WASM 的 24,502,072 字节一致；1,098 项工厂字段、192 项矩阵字段、120 项包络字段不变。
- 安装器 **125 项** 模拟检查通过，包含默认 rc3、显式候选、完整传输、救援恢复、身份错误拒绝及回退 rc2。实体 USB 刷写未在此环境执行。

## 包与安装入口

普通包 `OpenIFW-FM1-0.4.7-rc3.fwsc`：610,161 B；SHA-256：

```text
e5f1a8cf944debd3b79bd0f2939f7d8a5e3f8ecc0836644d68291234c9f70f7d
```

data+bss 81,712 / 98,304 B；pool 279,272 / 344,064 B，比 rc2 少 45,664 B；RAM code 19,064 / 24,576 B。每次编译使用独立暂存目录，编译前后验证原始 HAL 哈希；最终普通包重新构建后字节一致。测试和压力变体没有作为安装包发布。

网页默认及显式候选都选择 rc3。rc2 保留为回退包，原有更早恢复包继续可用。Demo 的 WASM 和安装页面脚本使用新版本缓存标记。安装后按钮和固件 ABOUT 都应显示 **0.4.7-rc3**，只读诊断 `firmwareCode=40703`；FM-1_000 仍表示救援，不能当作安装成功。

如果实机仍有爆音，可在[性能采集页](https://rcell233.github.io/OpenIFW-fm1/diagnostics/)持续演奏 Daisy / Tresure 并采集十秒原始数据，区分 UND / XRUN、输出削波与 USB 链路。当前候选已有软件与指令模型验证，实机声音验收仍需实际安装反馈。
