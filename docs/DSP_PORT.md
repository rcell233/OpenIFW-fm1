# DSP 移植记录 · 0.2

基准 OpenIFW `a478abd0645225add3f2a7751522da1bced7a2e6`；硬件平台 Dinghy `d44df7db09b98ccad2f1233b8f83f5f09c04829e`。以该版 OpenIFW 的行为为准，包括其占位参数；OpenIFW 本身是 IFW 的独立近似，不宣称原生 IFW 内部实现。

| 模块 | 桌面参考 | FM-1 定点实现 |
| --- | --- | --- |
| 按键栈、去重、回退 | engine.c + mono.c | control/mono.c + fixed/synth.c |
| 振荡器、Sync / FM、Noise、2×采样 | engine.c | fixed/synth.c + math.c |
| 主滤波、Filter3、控制律 | engine.c | synth.c + math.c / 整数表 |
| EG / LFO、普通调制、Glide / Breath / Bend / Gate | engine.c | synth.c + envelope.c |
| Exciter / Enhancer / Phaser / Chorus / Delay×2 / Reverb / EQ | effects.c | fixed/effects.c |
| 实时事件与面板 | 测试调度器 | app.c 的主循环→音频 SPSC 队列 |
| 浏览器声音 | 锁定原版 JS | 同一个 app.c / 固定 DSP 编译为 WASM；仅替换 HAL，不模拟 CPU 指令 |

## 表示与数值

定点核心固定 44.1 kHz。音频 Q20，控制参数 Q16，EG / Breath / Gate / Velocity Q31，U32 周期相位，主滤波与 allpass 系数 Q30，EQ 系数 Q28。EQ 内部状态额外保留小数位并采用舍入，避免低频 DC 偏置。SIN、音高、控制律、Glide 指数与 EQ 系数用离线生成的整数表与线性插值。

包络保留原 601 点下降曲线；Attack / Decay / Release 的 CV 在 0.01 网格的系数表之间插值，并用小数余数累计进度，避免极慢速率冻结。独立百进位接口保留用于旧测试，整机用 Q16 CV 接口。配置需使用的 64/32 除法由移位算法实现；目标没有浮点、超越函数、libgcc 数学或堆分配依赖。

## 每采样顺序

Breath 线性斜坡 → Bend 一阶平滑 → Gate 线性斜坡 → Glide → 四个 EG → 两个 LFO → 每 16 采样更新普通调制和滤波控制 → 两个内部振荡器采样 → 输出采样率滤波与 AMP → FX → 平方 MasterLevel → 限幅。没有通过跳过静音振荡器来节省运算。

每个内部采样、每个振荡器前都计算该振荡器的 FM。因此前序振荡器可提供本次的新 CV，后序振荡器仍提供上次的 CV。Sync 在中心交点 0.5 触发并保留分数经过量；FM 只偏移波形读取相位，不能改变后级同步时钟。改变 Semi / Tune 或清空调制的实时 patch 编辑会立刻刷新振荡器读取配置，同时保留相位。

## 单音与表达

NoteOn 先根据旧 held 列表判断 Legato，再去重并追加当前音；同音重复 NoteOn 也属于 Legato。NoteOff 回到最后一个仍按住的音及其原力度；全部松开才关闭门和释放 EG。各 EG 的 Retrigger 独立控制；LFO KeySync 即使 Legato 也重置。BR×EG 释放不依赖 Gate 仍开，尾音保留。Glide 保留 Time / Rate 两种模式及输入平滑。

原版 setPatch 会清空普通调制值、重建配置，但保留振荡器相位、EG 当前电平／阶段／进度及门状态。定点实现同样保留这些状态。MIDI / 面板的编辑在下一个 256 帧块起点应用，属于硬件输入延迟；块内调制时序保持原样。

## 效果器的硬件适配

效果链仍为 Exciter → Enhancer → Phaser → Chorus → Delay1 → Delay2 → Reverb → EQ。干声模式只绕过五个 insert，不绕过 Exciter / Enhancer / EQ。Exciter Mix 为 0 时仍保留原有延迟。

| 单元 | 保留与差异 |
| --- | --- |
| Exciter | 两级分数延迟与原混合结构，Q20 缓冲 |
| Enhancer | 原左右短延迟，Q20 缓冲 |
| Phaser | 每边 6 级 allpass、反馈、低通和立体声偏移，整数查表近似 |
| Chorus | 三角调制和左右对称延迟，Q20 缓冲 |
| Delay 1 / 2 | 两个独立立体声 delay，改为 22.05 kHz / Q15，输入相邻采样平均，湿声隔帧保持，最长 1 秒；原 JS 最长 2 秒 |
| Reverb | 每边 8 comb + 4 diffuser 及原调音长度，缓冲改 Q15，滤波／反馈状态用 Q20 |
| EQ | 原低／高架构，系数表覆盖 ±12 dB |

FX / 短延迟 / Reverb 缓冲共 263,496 B，平台 canvas 30,720 B，pool 总计 294,216 B。整数数学表约 81 KB 放 XIP。Tuning 限制 300–500 Hz，平滑时间限制 0–1000 ms；数值过载时饱和，不把整数回绕当成声音。

## 上游边界

- Mod Source1 为 None 时关闭该槽；Source2 为 None 时乘以 1。
- `OSC4 Noise Lo-Fi` 调制目标在上游接受但未消费，保留无效果行为；直接 Osc4NoiseLoFi 参数生效。OSC1–3 的 NoiseLoFi 字段同样不参与实际噪声保持。
- `Osc*Waveform3` 未被 DSP 消费；不伪造实现。Ext In 在原版输出 0。
- 原版噪声 CV 在第一轮噪声生成前可能未初始化。桌面参考保留其行为；硬件在第一份噪声样本生成前忽略噪声调制贡献，避免无效值。初始化后的 RNG 和保持计数按原版更新。

已运行的 166 组定点对照与实际原项目音色测量见 TEST_RESULTS.md；精确单音事件语义与有限精度音频必须分开判断。没有实机 CPU、音频或浏览器交互验证结果。

0.3 修正：Q15 延迟／混响缓冲写入向零取整，避免负反馈残留直流；Note 双极性源先减 64 再一次除以 127，避免过早整数截断；Glide 使用 Q24 动态速度与 Q39 系数，降低长音累计误差。参数本身仍按原 Q16 格式存储。六个原始 JSON 未加音色专用补偿。
