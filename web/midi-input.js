// SPDX-License-Identifier: GPL-3.0-only
// Input only. The browser grants access to ports, not to a selected USB device.
export function attachMidi({nav, secure, policy, button, status, activity, forward}) {
  let access, powered = false, timer, latest;
  const ports = new Map();
  const name = p => p.name || p.manufacturer || "MIDI 输入";
  function resetActivity() {
    clearTimeout(timer); timer = latest = undefined; activity.textContent = "";
  }
  function show() {
    if (!access) return;
    const states = [...ports.values()];
    const active = states.filter(s => s.open), failed = states.filter(s => s.error);
    const msg = [];
    if (active.length) msg.push("已连接：" + active.map(s => name(s.port)).join("、") + "。"
      + (powered ? "请弹一个音或发送呼吸 CC。" : "点 Switch on 开启声音。"));
    if (states.some(s => !s.open && !s.error)) msg.push("正在打开 MIDI 输入…");
    if (failed.length) msg.push("无法打开：" + failed.map(s => name(s.port)).join("、") + "。请关闭可能占用设备的音乐软件，再点刷新。");
    if (!states.length) msg.push("已获得 MIDI 权限，但未发现输入设备。请接好 USB 数据线并开启控制器；接入后这里会自动更新。");
    status.textContent = msg.join(" ");
  }
  function message(port, e) {
    const d = e.data;
    if (!d || d.length < 2 || d[0] < 0x80 || d[0] >= 0xf0) return;
    if (powered) forward(d);
    // Monitor the browser's raw input independently of the synth's CC/channel filters.
    latest = [port, d[0], d[1], d[2] ?? 0];
    if (timer !== undefined) return;
    timer = setTimeout(() => {
      timer = undefined;
      const [p, st, a, b] = latest, op = st >> 4;
      const event = op === 11 ? "CC" + a + " = " + b :
        op === 9 && b ? "Note On " + a + " · Vel " + b :
        op === 8 || op === 9 ? "Note Off " + a :
        op === 10 ? "Poly Pressure " + a + " = " + b :
        op === 12 ? "Program " + a : op === 13 ? "Pressure " + a : "Pitch Bend " + (a + (b << 7));
      activity.textContent = "最近输入：" + name(p) + " · CH " + ((st & 15) + 1) + " · " + event;
    }, 75);
  }
  function sync(retry = false) {
    const present = new Set([...access.inputs.values()].filter(p => p.state !== "disconnected"));
    for (const [p,s] of ports) if (!present.has(p) || (retry && s.error)) {
      p.onmidimessage = null; ports.delete(p);
      if (latest?.[0] === p) resetActivity();
    }
    for (const port of present) {
      if (ports.has(port)) continue;
      const state = {port, open:false, error:false};
      ports.set(port,state); // open() itself may generate statechange.
      (async () => {
        try {
          await port.open();
          if (ports.get(port) !== state) return;
          port.onmidimessage = e => message(port,e);
          state.open = true;
        } catch {
          if (ports.get(port) !== state) return;
          state.error = true;
        }
        show();
      })();
    }
    show();
  }
  const blocked = !secure ? "MIDI 需要 HTTPS。请直接打开 Demo 的 HTTPS 页面。" :
    !nav.requestMIDIAccess ? "此浏览器不提供 Web MIDI。请用电脑上的 Chrome 或 Edge 直接打开 Demo。" :
    policy?.allowsFeature?.("midi") === false ? "当前页面不允许访问 MIDI。请在新标签页直接打开 Demo。" : "";
  if (blocked) { button.disabled = true; status.textContent = blocked; }
  else button.addEventListener("click", async () => {
    if (access) { sync(true); return; }
    if (button.disabled) return;
    button.disabled = true;
    status.textContent = "正在请求 MIDI 权限；若浏览器弹出提示，请选允许。";
    try {
      access = await nav.requestMIDIAccess({sysex:false});
      access.onstatechange = () => sync();
      button.textContent = "刷新 MIDI";
      sync();
    } catch (err) {
      access = undefined;
      status.textContent = ["NotAllowedError","SecurityError"].includes(err?.name)
        ? "MIDI 访问未获允许。请在地址栏的网站设置中允许 MIDI，或在独立标签页打开 Demo，再试一次。"
        : "无法访问系统 MIDI（" + (err?.name || "未知错误") + "）。请检查系统能否识别设备，然后重试。";
    } finally { button.disabled = false; }
  });
  return {setPowered(on) { powered = on; show(); }};
}
