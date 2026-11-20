# Linux 正式 pilot 首次通过 Input

源码 `058b48085440f026985e70ca48ab6093f9dda500`，Ubuntu 24.04，`linux-gcc-release`。使用私有 Wayland、隐藏 Vulkan（Mesa llvmpipe 25.2.8）、锁定 Chromium 141.0.7390.37 和 Fontations/Skia 灰度字体链，实测 DPR 1，`desktop_fallback=false`。

- 同一源码的 Gallery 和控制程序构建成功，定向 CTest 3/3。使用 `calibrate --repetitions 5 --offline --dpr native` 通过独立校准并生成 profile。
- 使用 `run --suite pilot --reference locked --offline --dpr native` 完成三例，Input controlled-edit 全部三个检查点通过，机器失败项为 0。Button 为 256 项，Select 为 2174 项，套件退出 1。
- 本次普通运行耗时约 959 秒，其中采集 131 秒、对比 736 秒。原始证据保存在 Linux 工作区的 `out/formal-pilot-linux-058b4808/`。

三例联合通过和实际 `meta-test` 继续待验收。此次未记录 cold/warm 矩阵或 Windows 结论。
