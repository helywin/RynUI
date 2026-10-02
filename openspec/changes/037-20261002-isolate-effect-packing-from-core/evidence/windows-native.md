# Windows 原生验收

2026-10-02，实际机器 Windows 11 专业工作站版 10.0.26300，MSVC 14.51.36231、Ninja Multi-Config。原生 preset 为 `windows-msvc-debug` / `windows-msvc-release`，真实 SDL GPU 为 Direct3D 12，shader format 为 DXIL。

## 构建与测试

- 两种配置的全部 native 目标构建成功。隔离 Core 后发现 Button 测试原先隐式获取 GPU reference 符号，已显式链接 renderer/common；最终构建日志见 `windows/native-debug-build.log`、`windows/native-release-build.log`，归档副本仅去除了行尾空格。
- Debug 受影响 CTest：21/21，77.04 秒，覆盖 Effect math/store/scene/packing/resources/allocation、Input/Button/Selection 场景、局部更新、字体、shader、frame renderer、glyph resources 与 platform lifetime。
- Release 平台集成与 packing CTest：9/9，0.64 秒，覆盖 libdecor isolation、字体、logical/Effect packing、Effect resources、platform lifetime、frame renderer 与 generated/deployed shaders。平台通用逻辑已在 headless Debug/Release 完整验证，不重复要求 Linux 执行。
- SceneBackend 恢复与 upload 前 metrics 拒绝由本 change 的 headless Recording 测试验证；SDL native 构建不注册该测试，真实窗口结果单独列于下方。

## 真实窗口

通过 `windows/validate_native.py --root D:\code\RynUI` 串行运行最终构建的两种配置，共 8 次运行，全部退出码为 0。

- Gallery resize：每种配置实际调用 Win32 `SetWindowPos` 调整外框至 1200×840、900×700；client 为 1182×793、882×653，分别保存真实窗口截图。
- Typography：每种配置各运行系统 scale 1.25 与验收 scale 2.0。clipboard、ellipsis、edit、link、divider 全部 passed；每次保存 9 张 GPU readback 截图。
- Selection：每种配置在 scale 2.0 的真实 Gallery 执行 31 个自动输入事件；`selection_acceptance`、keyboard、pointer、blocked 均为 true，保存最终窗口截图。
- 系统字体日志记录 Segoe UI Variable Text、Segoe UI（strong/italic）和 Cascadia Mono；中文 fallback 为 Microsoft YaHei UI。请求 weight 500 时系统未匹配，使用 regular face，与既有行为一致。
- 目视检查代表截图：Debug Gallery wide、Release Gallery narrow、Release Selection、Debug system Typography semantics、Release scale-2 editing/dividers。文本、选择控件、焦点圆角与分隔线在这些截图中显示正常。

`windows/runs.json` 记录每次运行的退出码、日志、client 尺寸、最终 executable SHA256 与所有截图 SHA256。`windows/verify_evidence.py --root D:\code\RynUI` 确认 8 次运行、42 张 PNG 与当前 Debug/Release executable 哈希全部一致，结果见 `windows/hash-verification.txt`。

`openspec doctor --json` healthy；全量 strict 37 passed、0 failed；working/staged diff check 通过。此证据覆盖实际 Windows/D3D12 路径与自动窗口验收，不包含手工 IME 验收或 GPU 时间测量。Linux 原生验收仍未完成，在实际 Linux 机器独立记录。
