# Windows 原生验收

2026-10-02，Windows 11 / Visual Studio 2026 MSVC / Ninja Multi-Config，presets `windows-msvc-debug`、`windows-msvc-release`。实际 SDL GPU driver `direct3d12`，shader format `DXIL`；系统 display scale=1.25。完整 build 成功；受影响 CTest Debug 汇总 53/53、Release 53/53（15.59 s）。共同 headless 40/40 Debug/Release 记录于 `../common/integration.md`，本阶段不重复共同合同。

## 实窗矩阵

`validate_native.py` 使用当前配置的真实 Gallery EXE，执行 `--tooltip-content-acceptance`。Debug/Release 各 system、1、1.25、1.5、2，共 10 次运行；全部退出码 0，每次 28 个 GPU BMP 回读转换为 PNG，共 280 张。`runs.json` 保存完整参数、EXE SHA256、各 PNG 尺寸与 SHA256，脚本再次校验所有图像和本阶段 EXE 哈希。

每次经过 SDL 事件队列、平台归一化和公共 pointer/focus 路径：33 个 normalized events、2 次 controlled request、1 次旧按钮 activation、3 次富标题按钮 activation、56 次 GPU submission。富标题文本更新只重新测量既有节点，根 Content 和 TooltipTitle 均运行一次。覆盖 unavailable/恢复、popup 内点击不穿透、再次点击切换、空白关闭、右键固定指针锚点、Escape、pointAtCenter、四向箭头、Default/Dark/Compact、无箭头、1420x900 resize 与 inactive 无 deadline。字体由 Windows 系统字体链和打包 fallback 解析。

测试专用 marker/filter 排除桌面物理输入与非脚本 focus 抖动，仍保留真实 resize/lifecycle 消息；所有脚本鼠标/键盘消息都经过 SDL 平台适配，不直接调用组件事件。此证据为实际 Windows 窗口/GPU 自动验收，不等同于用户人工操作或 Linux/Wayland 验收。

## 检查发现与修复

- 预验收右键无法打开：SDL 适配器只归一化左键。补 secondary down/up 映射，保留 DPI、click count、兼容鼠标过滤；`rynui.sdl_event_adapter` 回归通过。
- 首次 Debug CTest 52/53：Gallery 的静态 Button call-site 合同仍期望旧的两个 Tooltip anchors。更新为五个 anchors 后单独复验 1/1（0.04 s），Release 完整受影响 53/53。原始和最终 build/test 日志保留，未把初次失败报告为通过。
- 视觉检查 `rich-updated`、`arrow-left`、`arrow-up`：中文/英文/图标可读，四向三角使用连续向量 glyph；默认 body 保留 token 的 0.85 alpha。几何与其余方向由共同合同和此次真实 capture 覆盖。

格式检查 395 个自有源码、0 failures；doctor healthy；full strict 43/43；`git diff --check` 通过。Linux checkbox 保持未完成。
