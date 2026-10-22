# 0.4.7-rc2 恢复候选验收

0.4.7-rc1 实机进入 USB RESCUE，已撤回。默认入口提供原始 0.4.6-rc2；新的恢复候选位于 [Install 0.4.7-rc2](https://rcell233.github.io/OpenIFW-fm1/install/?candidate=0.4.7-rc2&v=0.4.7-rc2)。检查按钮版本后安装，重启时松开所有按键，尤其 OCT−。正常身份为 FM-1_982，FM-1_000 仍为启动未确认。

新候选安装后可在 [诊断页](https://rcell233.github.io/OpenIFW-fm1/diagnostics/) 点击“读取启动信息”。即使仍处在 USB RESCUE，也可下载启动原因、上次阶段和保留的异常 PC。此前原始 0.4.6-rc2 包不支持该协议。

正常进入主界面后，测试六个出厂音色、Legato 和松键回退、旋钮/切页；用同页采集十秒实际性能，结合听感和 XRUN / DMA / UND / CLIP 判断。DSP 为各前台合成片段耗时之和，IRQ 为 DMA 复制与 USB 工作，不能与 rc1 的整块中断计时直接比较。过载仍可能缺块，当前未宣称爆音已修复。

完整修改、启动阶段、字段解释、原始证据及限制见 [恢复说明](RECOVERY_0.4.7-rc2.md)。未连接实体 FM-1，软件测试不构成实机验收。可随时使用 [原始 0.4.6-rc2 恢复入口](https://rcell233.github.io/OpenIFW-fm1/install/?recovery=0.4.6-rc2)。
