# 真机验证尚未完成

此开发环境只完成主机测试和 pi32v2 交叉编译，没有连接 FM-1，也没有执行刷写。0.2 包应作为实验固件评估。

1. 按平台文档通过现有安装器使用本项目 `.fwsc`；确认版本身份 `FM-1_982`。原有 OCT− 开机 USB rescue 与升级 loader 保留。
2. 默认 Tresure Island 按一个音，再重叠按第二、第三个音。应始终只听到最后音；依次松开应回退到还按住的音，最后松开有 EG 尾音。
3. 在 ENV 把某一 EG 的 Retrigger 开启，对比重叠音时该 EG 重启、其他 EG 连续。测试 Time / Rate Glide、弯音、Wind 气息和 CC64。
4. MIDI NoteOn 力度 0 应松键；CC120 / 123 应停音。跨八度按住物理键再变更八度，松键不能挂音。
5. 连接 USB Audio，先录 Tresure Island，再录 Primer、Daisy Area 2、Wanna Go To Hawaii 和满载双 Delay / Reverb 的音色。屏幕翻页、MIDI 和 USB Audio 并发下持续观测。
6. GLO 后 SELECT 下一页显示 RENDER MAX US 与 XRUN BLOCKS。44100 Hz / 256 帧的周期约 5804 µs；建议峰值尽量低于 2900 µs，超时块数应保持 0。这些阈值尚无真机测量值。
7. 从 Demo 选择 MIDI 输出并发送音色到 FM-1；核对几项 OSC、MOD、EG、FX 参数。设备通道为 1 或全通道。网页没有设备确认回包，必须核对屏幕／声音。按 SAVE，松开所有音并等尾音结束后再断电，重启应能恢复 USER 音色。
8. 最后检查升级／救援回归。此包未宣称通过这些真机步骤。

保存仅使用平台原有 4 KiB settings sector，不扩大写 Flash 范围。一次保存包含三个用户槽与配置，带版本和 CRC，最后写头；断电可能使该次保存无效，此时回到默认用户音色。
