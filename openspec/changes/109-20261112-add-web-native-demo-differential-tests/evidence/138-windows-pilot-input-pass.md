# Windows 正式 pilot 首次通过 Input

源码 `02e4551f5b2e8a417c9237885ba8b1c184d039c7`，`windows-msvc-release`。使用隐藏 D3D12、锁定 Chromium 141.0.7390.37 和 DirectWrite 灰度字体链，实测 DPR 1.5，`desktop_fallback=false`。

- Gallery 和控制程序构建成功，定向 CTest 3/3。使用 `calibrate --repetitions 5 --offline --dpr native` 通过独立校准并生成 profile。
- 使用 `run --suite pilot --reference locked --offline --dpr native` 完成三例，Input controlled-edit 全部三个检查点通过，机器失败项为 0。Button 为 125 项，Select 为 2120 项，套件退出 1。
- 本次普通运行约 2941 秒，其中采集 331 秒、对比 2313 秒。原始证据保存在 `out/nov19-differential/nov20-formal-windows-02e4551f/`。

同一次运行没有执行故障注入或 cold/warm 矩阵。较新的文字、阴影和 Button 修复需使用其对应源码快照复验。
