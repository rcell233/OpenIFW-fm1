# 0.4.6-rc1 实机验收

安装、回退、诊断字段和本轮测试边界统一见 [调度与显示重做说明](REWORK_0.4.6.md)。

打开 [安装页](https://rcell233.github.io/OpenIFW-fm1/install/?v=0.4.6-rc1)，确认显示 Install 0.4.6-rc1；保持 USB 连接，安装和重启时松开按键，尤其 OCT−。若返回 FM-1_000，仍算正常固件未启动，可使用保留的 0.4.4 回退入口。

进入 GLO → PERFORMANCE，RESET 后测试六个工厂音色、重叠按键的 Legato、松键回退、旋钮与切页。记录 DSP MAX、IRQ MAX、XRUN、DMA、UND、MAIN。后三个音频异常计数 XRUN / DMA / UND 应为 0；检查实际声音和完整文字更新，不能仅凭计数认定通过。SINE / MUTE 仅用于诊断，退出页面恢复 SYNTH。

当前没有连接实体设备，本轮软件通过不构成实机验收。
