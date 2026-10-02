# Windows 原生验收

2026-10-03，实际 Windows / MSVC / Ninja Multi-Config，`windows-msvc` Debug、Release。D3D12，DXIL，Win32 窗口；系统字体 Segoe UI Variable Text / Microsoft YaHei UI，系统 display scale 1.25。

- 受影响原生 CTest：Debug 18/18（31.74 秒）、Release 18/18（11.61 秒），日志见 `native-ctest.log`。
- 默认 1 MiB stack 的 Gallery `--smoke`：两个配置退出 0，110 stable IDs / 129 live samples，日志见 `gallery-debug-smoke.log`、`gallery-release-smoke.log`。
- `--space-acceptance`：Debug/Release × 系统及 1/1.25/1.5/2 render scale，共 10 个实际窗口、180 个 GPU 读回；`runs.json` 保存 EXE 与逐 PNG SHA256，`validate_native.py --verify-only` 验证当前二进制/证据身份。
- 每个窗口完成 separator、真实字体 baseline、Compact H/V/RTL/三尺寸、hover/focus seam 优先级、四 Addon 变体、error/dark、混合控件/嵌套删除、编辑会话身份、浮层关闭清理、1420×900 resize、实际 SDL 指针命中、三次 idle poll 无 deadline、销毁检查；36 次提交，6 个 pointer events / 2 个 text events / 1 次激活。自动程序通过 SDL 队列注入 composition/commit，经平台桥与 session stamp 路由；不将它描述为人工操作系统 IME 候选窗验收。

已目视检查 `debug-system/initial.png`、`release-scale-2/vertical-rtl.png`、`debug-scale-1.5/popup-edit.png`。修复真窗口发现的 Button 文字溢出后重新生成全部读回：Button/Addon/Radio 的自动最小尺寸沿嵌套 Compact 传递；显式 min_width/min_height 保持优先。Gallery 混合组在窄窗口切换纵向排列，原有明确宽度保留。

该修复的 Windows HEADLESS 受影响合同 Debug/Release 各 4/4（5.01 / 3.42 秒），包含嵌套 Button 的 intrinsic minimum 与 Input 剩余宽度分配。050 集成前完整 HEADLESS 71/71 的记录仍见 `../common/integration.md`；最终修改另行执行了这里的受影响回归与原生 Gallery。

复现：从仓库根目录使用带 Pillow 的 Python 运行本目录 `validate_native.py`。构建通过 `windows-msvc-debug` / `windows-msvc-release`；源码入口为 `examples/token_gallery/space_acceptance.cpp`，资源经共同 SceneResources 事务与 backend readback。

Linux 平台项独立待验收，未用本机 Windows 结果勾选。
