# 0.4.7-rc8：继续降低逐采样计算开销

相对 rc7，六个出厂持续音场景的模型平均 DSP 耗时减少 **3.1%–8.2%**。Daisy 密集演奏 P99 从 **3.397 ms 降到 3.134 ms**，再减少 **7.7%**；与相同模型、相同脚本的 rc5 历史记录相比，累计约减少 **10%**。这是软件模型比较，实机爆音根因仍未确认。

[安装 rc8](https://rcell233.github.io/OpenIFW-fm1/install/?candidate=0.4.7-rc8&v=0.4.7-rc8) · [下载固件](https://rcell233.github.io/OpenIFW-fm1/firmware/OpenIFW-FM1-0.4.7-rc8.fwsc) · [回退 rc7](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.7-rc7&v=0.4.7-rc8) · [原始证据](evidence/dsp-kernels-0.4.7-rc8.json)

## 两项实际改动

**原非线性函数的精确整数除法。** 滤波器、Phaser 和 Delay 反馈会反复调用 `ifw_soft20`。原实现每次计算两次变量整数除法。新版把分母归一化到 16 位，用 512 项种子表、一次整数 Newton 迭代和余数校正取得精确倒数，两处商共用该倒数。保留原来的整数商、余数和截位规则，非线性传递函数完全相同；没有增加输出限幅器、淡入淡出或换音处理。

对全部 32,768 个归一化分母、7,962,623 组除法边界，以及非线性函数的 **全部 4,294,967,296 个 int32 输入**独立对照原来的除法实现，结果一致。完整输入域的记录见 [算术证据](evidence/division-full-domain-rc8.json)。种子表可由 `python3 tools/generate_soft_division.py --check` 重算验证，占用 2 KiB RAM。

**波形按实际需要的输出执行原公式。** 锯齿和脉冲增加返回单个结果的入口；只需要 SIN/TRI 或其与锯齿、脉冲组合时，直接执行对应的原计算。减少通用多输出函数的判断和参数搬运。相位、两倍内部采样、BLEP、波形幅度、FM/Sync 的读取顺序、噪声及实时编辑时的原始 CV 捕获保持不变。

还比较了其他分支组织、调制乘法方案，部分预设变慢或收益太小，均未纳入。最终没有更改调制公式、控制更新频率、包络、Mono/Legato、预设、Q23 混响或输出增益。rc6 的输出过渡继续撤回。

## 同条件目标指令比较

实际运行 rc7/rc8 的 `.fwsc` 指令。下表为 A4 持续音、每 256 帧的模型平均 DSP 时间；假设 CPU 240 MHz、普通指令 1 周期、除法 48 周期、SPI 60 MHz。

| 预设 | rc7 | rc8 | 减少 |
|---|---:|---:|---:|
| Tresure Island | 3.212 ms | 3.064 ms | 4.60% |
| Primer | 2.239 ms | 2.169 ms | 3.10% |
| Ai no Labyrinth | 2.630 ms | 2.438 ms | 7.31% |
| Daisy Area 2 | 3.226 ms | 2.963 ms | 8.15% |
| Wanna Go To Hawaii | 2.552 ms | 2.389 ms | 6.40% |
| JUDD-ish 2000 | 2.200 ms | 2.131 ms | 3.15% |

考虑到除法延迟未经实测，另把除法成本设为 **1 周期**，六个预设仍减少 **1.48%–6.46%**。Daisy 的 16 周期假设下减少约 4.03%。因此组合优化也减少了普通指令开销，并非只依赖 48 周期假设。模拟器不模拟实际 cache、Flash、总线及连接的 USB 主机；这些数值不能换算成实机 CPU 使用率。

密集演奏沿用 rc7 的同一脚本：Daisy 每 25 ms 换音、每音保持 16 ms；Tresure 每 32 ms 换音、保持 45 ms。Daisy P99 从 3.396875 ms 降至 3.134375 ms，峰值从 3.398583 ms 降至 3.136083 ms。Tresure 峰值从 3.381125 ms 降至 3.234542 ms。采样 P99 会受事件对齐影响，持续音表和完整原始分布同时保留。模型 UND/XRUN/DMA 均为 0，仍不能据此认定实际设备不会爆音。

## 验证及边界

- `make test`、`make sanitize`、新增精确除法 ASan/UBSan 检查通过。旧的 166 场景、332 份 PCM/状态、1,024 次实时编辑、1,776 次非对齐编辑及原始预设黄金值没有修改。
- 锁定 rc5 的 24 个生产场景、2,359,296 帧，PCM、既有合成器状态和 FX 延迟内存校验一致；原生 C/WASM 一致，288 个输出电平场景和 600 秒长演奏无输出削波。
- **实际生产 RAM 路径**运行 196,608 帧，24 个目标指令校验值与 rc7、原生 C 一致。历史全速效果参考的 16 个校验值也一致，新增非线性函数/波形随机运算的目标自测错误为 0。
- 历史全速效果自测包比 24 KiB RAM code 限制多 248 字节，因此该未发布参考用现有 `IFW_DSP_SELFTEST=1 IFW_DSP_XIP=1` 模式检查完整原始音频；它不用于性能比较。正常生产包及生产流测试都在原 RAM 布局运行，未放宽任何资源检查或修改生产链接脚本。
- 32 秒模型运行后启动确认清零；OCT− 开机进入 USB RESCUE。正常包独立重编译的六项输出逐字节一致，安装器包含 rc7/rc5 原包回退检查。

包 610,161 B，SHA-256：`1f3555ab029408ae1c7e947a92b14f28bca68fbc5a73e0918445f0af9c8af75f`。app 272,676 B；data+bss 83,776 / 98,304 B；pool 285,208 / 344,064 B；RAM code 21,328 / 24,576 B。新增种子表经 ELF 地址检查位于 RAM；输入/时间/ISR 原始 HAL、生产链接脚本、输出路径和 Q23 混响源码哈希不变。

阅读了杰理官方 [AC79 MATH 文档](https://doc.zh-jieli.com/AC79/zh-cn/release_v1.1.0/module_example/system/math.html) 中的位宽、饱和、软件/汇编/硬件加速接口。当前处理含逐级反馈和固定截位顺序，直接替换成向量接口需要另行证明。此次只采用经过精确对照的整数和波形计算组织优化。

## 复现

正常构建、测试和发布路径仍按 README。全输入域验证较耗时，可在普通除法测试生成二进制后另行运行：

```sh
make soft-division-test
build/soft-division/production --full
make test
make sanitize

# 已锁定且应用 tools/fm1-emu-audit.patch 的 fm1-emu
python3 tools/target_symbols.py build/firmware/openifw.elf > build/symbols.json
IFW_SIM_DIV_CYCLES=48 node tools/dsp_cost_probe.mjs \
  ../dependencies/fm1-emu/target/wasm32-unknown-unknown/release/fm1_web.wasm \
  build/firmware/OpenIFW-FM1-0.4.7-rc8.fwsc build/symbols.json \
  build/daisy-cost.json 3 baseline A4
```

实机 CPU/中断超时与爆音是否消除仍需设备数据和听感确认；当前没有连接实体设备，未宣称根治。
