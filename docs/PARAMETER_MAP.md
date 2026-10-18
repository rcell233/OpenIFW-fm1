# 参数与传输映射 · 0.2

`parameters.json` 为 156 个稳定 DSP 参数的 ID、原 JS 默认值和已知范围。`panel-controls.json` 与 `firmware/src/panel_params.h` 由 `tools/generate_panel.py` 同时生成：229 个 patch 字 + 4 个硬件配置 + 4 个监测项目。

| 类别 | 本机入口 | 语义 |
| --- | --- | --- |
| OSC1–4 | SEL / ALGORITHM | Sync、Semi、Tune、SAW / Wave / PWM / Noise、PWM Width、Waveform2、双输出 |
| Filter1–3 | EDIT / SELECT / ALGORITHM | 频率、共振、气息子源、KeyTrack、路由和 Filter3 级数／混合 |
| AMP1–2 | ARP / SELECT | Level、Breath、BR×EG 来源 |
| EG1–4 | ENV / ALGORITHM | ADSR 与每包络 Retrigger |
| LFO1–2 | LFO / ALGORITHM | 波形、Speed、KeySync、两种 BPM Sync 拼法、Beat；按原版固定 120 BPM 换算 |
| MOD1–8 | SEQ / ALGORITHM | Source1 × Source2 × Depth → Destination |
| 五组 FX | FX / ALGORITHM / SELECT | 14 个固定字 / 单元，含上游未消费的占位字段；重要参数与范围与原版相同，Delay 最长 1 秒 |
| 全局 DSP | SELECT / ALGORITHM | Master、Transpose、平滑、Filter3、EQ、Exciter / Enhancer 等 |
| 演奏输入 | 27 键与 USB MIDI | Note / Velocity、Breath、Bend、Vibrato、Glide、Gate |
| 硬件配置 | GLO | Wind 模式、接收通道、键盘八度、保存槽 |
| 监测 | GLO 后 SELECT | render 最大 µs、超时块数、采样率和版本；host 不伪造芯片时钟 |

Patch 结构为 229 × 32 位（916 B）：前 156 字为静态参数，接着 5×14 字为 FX，最后为 Tuning、ChorusPhase、FxBypass bitmask。连续数值 Q16，枚举以稳定整数值编码。硬件设置和三个用户槽在原有 settings sector 内存储，版本 2、CRC32、最后写头。二进制不是可直接拖入网页的音色文件；网页导入 JSON / .ifwprog 后经 NRPN 转入当前音色。

NRPN 编号为 patch 字号 0–228。CC99 / CC98 发送编号高／低 7 位，CC6 / CC38 发送数值高／低 7 位，收到 CC38 后才应用。数字参数把 lo–hi 映射到 0–16383；枚举把 option index 映射到同一范围。定义与选项列表以 panel-controls.json 为准。发送方每个参数间隔 10 ms，避免淹没输入队列；229 个参数约 2.3 秒。通道 1，接收机须为通道 1 或全通道。

Mod 目的地沿用原版大小写／空白规范化和别名，清单为 modulation-destinations.json。Source 编号、双极标志和虚拟目的地由 fixed_patch.py / web/patch-codec.js 编解码。默认气息 CC7、Vibrato CC9、Glide CC5，CC64 延音，NoteOn velocity 0 转松键，CC120/123 panic。当前没有 TRS MIDI 或 Aftertouch 接线。

`.ifwprog` 导入只在浏览器进行；保留其已实现 DSP 参数和五个效果区块。未参与 DSP 的 Waveform3 不参与新音色处理。设备侧通过 NRPN 接收参数；SAVE 后才写 Flash。数值越过硬件编辑边界时，发送方限制到边界并提示。
