# Windows Reference Gallery 人工验收记录（部分，未通过）

- 日期：2026-09-22；平台：Windows / Win32 / MSVC x64 / D3D12 / DXIL。
- 构建：正式 `windows-msvc` preset 的独立 Release 验证目录 `out/build/windows-msvc-scroll-validation`；本次不复用既有 `out/build/windows-msvc` 缓存。字体来源为系统，运行时报告 `Segoe UI Variable Text`、`Microsoft YaHei UI`；host display scale 为 1.5。
- 实际启动：无 scale 参数（`scale_source=window`，render scale 1.5），以及 `--acceptance-scale=1.0|1.25|1.5|2.0`。五次均显示 `window_system=win32`、`gpu_driver=direct3d12`、`shader_format=DXIL`、`font_source=system`，正常关闭且 `exit_code=0`。
- 已直接操作并观察：文档顶部、Foundation、General/Other 类别、Live Samples、滚轮、All/Planned 筛选、非交互目录卡片点击；在 2.0 档观察 Default/Primary/Danger hover 与 Tab 键盘 focus ring。非交互目录卡片点击后未出现 Button hover/focus 外圈，Default hover 仅现有边框/文字变蓝，solid hover 改变填充而未见独立蓝色外圈，Tab focus 出现外圈。
- 未完成：逐档浏览 Introduction、Foundation 和七类末尾、逐一操作全部筛选、Button 按下态/Pointer focus、窗口宽窄往返及所有 72 项人工可读性检查。因此任务 7.2 仍未完成；任务 7.3 亦未通过。

## 确认问题

Foundation 色块压在 `ant.map.colorPrimary`、`ant.map.colorSuccess` 等 Token 标识文字的开头。该问题在系统 scale 1.5 和四档 acceptance render scale 均可见，故不把 Windows 视觉验收标记为 passed。跟踪项为 `GALLERY-FOUNDATION-SWATCH-006`，见 [项目问题总表](../../../../docs/open-issues.md)。

| 场景 | 截图 |
|---|---|
| 系统 scale 1.5，Introduction | [windows-system-introduction.png](screenshots/windows-system-introduction.png) |
| 系统 scale 1.5，Foundation | [windows-system-foundation-swatch-overlap.png](screenshots/windows-system-foundation-swatch-overlap.png) |
| 1.0，Foundation | [windows-scale-1.0-foundation.png](screenshots/windows-scale-1.0-foundation.png) |
| 1.25，Foundation | [windows-scale-1.25-foundation.png](screenshots/windows-scale-1.25-foundation.png) |
| 1.5，Foundation | [windows-scale-1.5-foundation.png](screenshots/windows-scale-1.5-foundation.png) |
| 2.0，Foundation | [windows-scale-2.0-foundation.png](screenshots/windows-scale-2.0-foundation.png) |
| 2.0，Default hover | [windows-scale-2.0-default-hover.png](screenshots/windows-scale-2.0-default-hover.png) |
| 2.0，Danger hover | [windows-scale-2.0-danger-hover.png](screenshots/windows-scale-2.0-danger-hover.png) |
| 2.0，Tab focus | [windows-scale-2.0-keyboard-focus.png](screenshots/windows-scale-2.0-keyboard-focus.png) |
| 系统 scale，Planned 筛选下的 General | [windows-system-planned-general.png](screenshots/windows-system-planned-general.png) |

## 诊断边界

- 五次退出码均为 0；运行时诊断保存在本机 ignored `out/build/windows-msvc-scroll-validation/manual-scale-*.stdout.txt`。本文件保留可审查的关键信息，不能用这些输出代替屏幕视觉验收。
- 普通窗口保持运行时 `idle_after_animation=0`，同时 Live Samples 的 Loading 示例保持动画；这组数据不能单独证明 idle 回归，需在 Loading 停止或 motion-disabled 条件下另测。
- 当前记录是失败发现与部分操作证据，不是 Windows passed evidence；`tasks.md` 的 7.2/7.3/7.4 保持未勾选，Linux 项目不受影响。
