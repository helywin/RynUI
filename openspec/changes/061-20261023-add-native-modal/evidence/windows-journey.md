# 原生 Modal 验收入口

2026-10-23 已实现 `rynui_token_gallery --modal-acceptance --evidence-dir=<目录>`；可选 `--acceptance-theme=dark`、`--acceptance-scale=1|1.25|1.5|2`，省略 scale 使用系统缩放。

入口实际创建 SDL/D3D12 窗口，使用生产 SceneResources 与 DXIL，逐阶段 readback BMP。自动验证受控拒绝、确认/遮罩/Escape、加载、背景焦点隔离、Tab/Shift+Tab、Input composition 与 commit、草稿/身份保留、Select/Tooltip Escape 优先、嵌套焦点层、正文滚动/末尾编辑显露、居中/透明遮罩、resize、idle 和销毁。

首轮实际 Windows MSVC x64：

- `windows-msvc` Debug 构建目标 `rynui_token_gallery`，Light/render scale 1，通过。
- 同 preset Release 构建目标，Dark/render scale 2，通过。
- driver `direct3d12`，shader `DXIL`，系统 scale 1.5；实际字体 Segoe UI Variable Text / Segoe UI。
- 本机日志 `out/modal-native-first.log`、`out/modal-native-release-first.log`，逐阶段 readback 保留在相应 out 目录；已检查首轮 opened PNG 的遮罩、标题、正文、关闭与页脚布局。

本提交只交付可重复验收入口与这两组实测结果。Task 6.1 保持未完成，完整 native build/受影响平台测试与20组主题/缩放/配置矩阵另行收口；自动 composition 不代表人工系统 IME，Linux 独立待验收。
