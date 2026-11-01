# 0.4.7-rc2 面板与操作

rc12 保留 rc11 的 SLOOP 风格：黑底、细分隔线、四旋钮分色与大号数值，使用同源 Terminus Bold 8×16 字体及整数放大的 16×32 字体。57 页的参数映射、音色、OSC / FM、调制、包络、Mono / Legato、混响保持不变；输出按用户要求恢复为 rc8 NORMAL，AUDIO → OUTPUT 只读，所有主题的页面背景固定黑色。SELECT / ALGORITHM 的导航分工按用户要求调整如下；实际屏幕见 [界面与配色](OUTPUT_RESTORE_0.4.7-rc12.md)，逐页映射在 `ui-pages.json`。

| 控件 | 操作 |
| --- | --- |
| 四旋钮 | 从左到右编辑屏幕四个卡片；当前旋钮高亮，数值和枚举按步进变化 |
| 页面按钮 | 首次返回该组上次使用的子页；重复按依次翻页 |
| SELECT | 当前组上一页 / 下一页；列表选择；菜单中选分类；命名时移动光标 |
| ALGORITHM | 所有页面均循环切换 8 套配色，包含菜单、快捷层和确认窗口；不改声音或导航 |
| PRESETS | 打开音色列表并加载音色；菜单中选行 |
| MASTER | 原有硬件总音量；始终独立于当前页 |
| HOME | 返回 HOME；在 HOME 再按切到 MIX。长按打开 / 关闭分类菜单 |
| EDIT | OSC1–4 的混合、调音、输出；Filter1–3、路由与滑音 / 平滑 |
| ENV | EG1–4 的 ADSR，各 EG 的 Retrigger，以及 Breath × EG 绑定 |
| LFO | LFO1 / LFO2 的速率、波形、触发、同步与 Beat |
| SEQ | 八槽调制：第一页 SLOT / SRC1 / DEST / DEPTH，下一页编辑 SRC2 |
| FX | 总混合、Phaser、Delay1、Delay2、Reverb、Chorus、Exciter、Enhancer 与 EQ |
| SCL | Transpose、Cents、A4 调音与 Bend Range |
| ARP | 两组 AMP 的电平、气息深度与来源 |
| GLO | 气息模式、MIDI 通道、八度、保存槽和 MIDI Learn 提示 |
| SAVE | 用户槽界面；再次按进入预设列表。长按撤销 / 重做上次加载 |
| REC | 学习最后编辑参数对应的下一个非保留 CC；再按取消 |
| OCT− / OCT+ | 普通页改八度；确认框中取消 / 确认；命名中删除 / 保存 |
| PLAY | 无音符时试听 A4，正在发声时停音 |

长按默认 500 ms，可在菜单设为 300–1000 ms。FX / GLO / SCL / EDIT / SEQ / REC 长按打开对应快捷层，松开关闭；双击锁定，按页面按钮关闭。快捷层中的琴键用于操作，不触发新音符，已有音符松开仍正常释放。

| 快捷层 | 前八个白键 F3–F4 |
| --- | --- |
| FX | Phaser / Chorus / Delay1 / Delay2 / Reverb 开关，随后进入 Exciter / Enhancer / EQ |
| EDIT | OSC1–4、Filter1–3、VOICE |
| SEQ | 选择调制槽 1–8 |
| REC | 前三个白键分别开始学习、取消学习、请求清空映射 |

FX 每页只映射该效果器使用的字段。Delay 的 TIME / FDBK / MIX / BYP 在一页，滤波在下一页；Delay1 与 Delay2 独立。BYP=ON 表示旁路，中央状态同时显示 EFFECT ON / BYPASSED。界面开关会正确处理原有总旁路掩码和单个效果器开关。OSC1–3 的上游噪声占位字段不放进常用页；原始数据仍保留，在 ALL PARAMS 可查看。

预设页的旋钮为编号、固定 IFW 核心、收藏和 ALL / FAV / FACT / USER 过滤。空用户槽不进入浏览列表。六个原始 JSON 的名称、ProgramIndex 和数值原样保留；收藏和过滤不改写音色。

SAVE 页的四旋钮为 SLOT / LOAD / ERASE / SAVE。旋钮向右选择操作，OCT+ 执行；覆盖、擦除或加载时丢弃当前编辑需要 OCT+ 确认，OCT− 取消。保存时可用琴键输入 A–Z、G5 输入空格，PRESETS 或第一旋钮选字母，SELECT 或第二旋钮移光标；最多 12 字符。OCT+ 排队保存，声音静音至少 1 秒且音频事件队列为空后写 Flash。擦除槽不会改变当前发声音色。长按 SAVE 交换最近加载前后的音色，支持一次撤销 / 重做。

HOME 菜单分 DISP / CTRL / AUDIO / MIDI / SYS。SELECT 选分类，PRESETS 选行，任意参数旋钮或 OCT± 改值；HOME 关闭。配色有 SLOOP / FOREST / AMBER / MONO / OCEAN / VIOLET / EMBER / ICE；ALGORITHM 向两侧循环。配色、实心 / 线框、REFRESH NORMAL / LOW、示波器模式、音名、长按阈值、加速、收藏和用户名称存在原 settings sector 尾部，保留旧版 patch 格式和 CRC。旧的样式和动画位沿用存储位置；REFRESH 的 NORMAL / LOW 分别把动态更新限制在最高 20 / 10 Hz，两者均继续刷新波形。SCOPE OFF 才关闭波形，不能把 LOW 当作停止波形。

MIDI 保持原有语义：Note On / Off、最后按键优先、CC7 Breath、CC9 Vibrato、CC5 Glide、14 位 Pitch Bend、CC64 Sustain、CC120/123 Panic。Program Change 使用 4、18、14、10、8、21。CC20–23 仍可对当前真实参数做绝对值编辑；列表动作不通过该通道触发。NRPN 0–228 的参数传输格式未改。

浏览器由真实 C 固件运行上述逻辑，JavaScript 只传入控件输入并显示 LCD / LED / 状态。浏览器和目标共享 `app_step`、参数事件队列与存储逻辑；USB 外设、杰理指令和实时 CPU 不在仿真范围内。


0.4.7-rc2 恢复前台 32 帧调度和音频余量绘图；一块余量时波形仍有最高 10 Hz 的更新机会。另可在网页诊断中读取启动信息，救援模式也支持。

GLO 的第三页为 PERFORMANCE；也可长按 HOME，在 SYS 选 PERFORMANCE 并向右操作打开。K1 为 SYNTH / SINE / MUTE，K2 向右清零统计，K3 冻结读数，K4 为只读状态。SINE 为低电平 440 Hz，MUTE 为全零，均临时跳过 DSP；退出该页自动回到 SYNTH，重启也默认 SYNTH，不保存到音色或配置。网页显示 TIMING: DEVICE ONLY / N.A.，不把电脑 CPU 当作设备测量。真机统计项见 [硬件验证](HARDWARE_TEST.md)。

切配色只更新 RAM 中的显示设置，不排队音频事件，不修改当前页、音色或编辑参数，不在演奏中写 Flash。保存仍须 SYS → SAVE SETUP 确认。偏好记录保持原 60 字节和 CRC，基础 palette 字段保留 0–3；扩展主题写入原记录末尾的预留字节，旧版回退仍能读取其他设置。连续切色更新现有待绘制区域，不把刷新游标不断重置到页首；音频优先级、DMA gate、六个矩形和动态刷新节奏保留。
