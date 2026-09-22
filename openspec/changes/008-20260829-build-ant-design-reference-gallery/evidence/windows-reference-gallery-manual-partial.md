# Windows Reference Gallery 人工验收记录（部分，未完成）

- 日期：2026-09-22；平台：Windows / Win32 / MSVC x64 / D3D12 / DXIL。
- 构建：正式 `windows-msvc` preset 的独立 Release 验证目录 `out/build/windows-msvc-scroll-validation`；本次不复用既有 `out/build/windows-msvc` 缓存。字体来源为系统，运行时报告 `Segoe UI Variable Text`、`Microsoft YaHei UI`；host display scale 为 1.5。
- 实际启动：无 scale 参数（`scale_source=window`，render scale 1.5），以及 `--acceptance-scale=1.0|1.25|1.5|2.0`。五次均显示 `window_system=win32`、`gpu_driver=direct3d12`、`shader_format=DXIL`、`font_source=system`，正常关闭且 `exit_code=0`。
- 已直接操作并观察：文档顶部、Foundation、General/Other 类别、Live Samples、滚轮、All/Planned 筛选、非交互目录卡片点击；在 2.0 档观察 Default/Primary/Danger hover 与 Tab 键盘 focus ring。非交互目录卡片点击后未出现 Button hover/focus 外圈，Default hover 仅现有边框/文字变蓝，solid hover 改变填充而未见独立蓝色外圈，Tab focus 出现外圈。
- 未完成：在四档 acceptance scale 重复完整文档浏览、Button 按下态/Pointer focus、所有 72 项逐一人工可读性检查。因此任务 7.2 仍未完成；任务 7.3 亦未通过。

## 确认问题

原始构建中 Foundation 色块压在 `ant.map.colorPrimary`、`ant.map.colorSuccess` 等 Token 标识文字的开头；系统 scale 1.5 和四档 acceptance render scale 均可见。修复后色块移至卡片标题行右侧、状态圆点左侧，五档实窗截图中均未再见遮挡，`rynui.reference_surface` 几何回归测试通过。跟踪项 `GALLERY-FOUNDATION-SWATCH-006` 保持 `pending`，直到完整 Windows 7.3 视觉验收结束；见[项目问题总表](../../../../docs/open-issues.md)。

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

## 色块修复后的局部复验

| 场景 | 截图 |
|---|---|
| 系统 scale 1.5，Foundation | [windows-foundation-swatch-fixed-system-1.5.png](screenshots/windows-foundation-swatch-fixed-system-1.5.png) |
| acceptance scale 1.0，Foundation | [windows-foundation-swatch-fixed-1.0.png](screenshots/windows-foundation-swatch-fixed-1.0.png) |
| acceptance scale 1.25，Foundation | [windows-foundation-swatch-fixed-1.25.png](screenshots/windows-foundation-swatch-fixed-1.25.png) |
| acceptance scale 1.5，Foundation | [windows-foundation-swatch-fixed-1.5.png](screenshots/windows-foundation-swatch-fixed-1.5.png) |
| acceptance scale 2.0，Foundation | [windows-foundation-swatch-fixed-2.0.png](screenshots/windows-foundation-swatch-fixed-2.0.png) |

修复后四档运行时仍报告 `window_system=win32`、`gpu_driver=direct3d12`、`shader_format=DXIL`、`font_source=system`，系统 host display scale 为 1.5，窗口正常关闭；本机原始诊断在 ignored `out/build/windows-msvc-scroll-validation/manual-swatch-*.stdout.txt`。这里只证明 Foundation 色块与标识分离，不代表已逐档检查完整文档、所有筛选及 Button 按下态。

## 系统 scale 1.5 的连续文档浏览（部分）

在修复后的同一真实窗口中，以 pointer wheel 从 General 顺序浏览至 Other，并观察各类末尾和下一类标题；末尾分别为 Typography、Splitter、Tabs、Upload、Tree、Watermark、Util。以下截图证明七类尾项均可进入 viewport，但不等于逐一人工核对 72 项全部文字。观察到的目录卡片无 Button 蓝色 hover/focus 外圈；长文档卡片未见跨卡片溢出。

| 分类 | 末尾截图 |
|---|---|
| General | [windows-system-general-tail.png](screenshots/windows-system-general-tail.png) |
| Layout | [windows-system-layout-tail.png](screenshots/windows-system-layout-tail.png) |
| Navigation | [windows-system-navigation-tail.png](screenshots/windows-system-navigation-tail.png) |
| Data Entry | [windows-system-data-entry-tail.png](screenshots/windows-system-data-entry-tail.png) |
| Data Display | [windows-system-data-display-tail.png](screenshots/windows-system-data-display-tail.png) |
| Feedback | [windows-system-feedback-tail.png](screenshots/windows-system-feedback-tail.png) |
| Other | [windows-system-other-tail.png](screenshots/windows-system-other-tail.png) |

同一运行中最大化进入宽布局、恢复窄布局，恢复后保持在 Other section；[宽布局截图](screenshots/windows-system-wide-other-live.png)、[恢复窄布局截图](screenshots/windows-system-narrow-restored-other.png)。逐一点击 `Implemented`、`Partial`、`Planned`、`Web only`、`Deprecated`、`Out of scope` 和 `All` 筛选；`Out of scope` 下七类标题仍在、目录条目隐藏且 Live Samples 可达，最后恢复 `All`。本次使用窗口关闭按钮正常退出，但未独立捕获该运行的进程退出码；不能以此前五档运行的 `exit_code=0` 替代。

## 1.0/1.25 档续验与 Input 位移修复

- 1.0 档从 General 顺序滚动至 Other/Util，并操作 Live Samples 的 Default、Primary、Danger 和 Tab 焦点；[Other 末尾](screenshots/windows-scale-1.0-other-tail-followup.jpg)、[键盘焦点](screenshots/windows-scale-1.0-keyboard-focus-followup.jpg)。窗口正常退出，进程 `exit_code=0`。这证明可到达与交互路径，不代表在 host 1.5× 下按 1.0× 渲染的细小文字已经逐项通过人工可读性检查。
- 1.25 档点击 General 后，两个白色圆角条覆盖 Button/Icon 卡片文字；滚轮下移后遮挡位置仍跟随变化。[修复前实窗](screenshots/windows-scale-1.25-blank-overlay-general.jpg)。原因是 Gallery 给滚动子树的每个节点都设置相同位移，而 Input 容器和文字 viewport 额外累加所有祖先位移，使文档末尾 Live Samples 的两个 Input 在前面卡片位置绘制。
- `InputComponentHost` 已改为和其他场景节点相同的单节点位移语义；新增同位移子树回归测试。MSVC Release 构建成功，相关 CTest 23/23 通过。相同 1.25 档 General 点击与滚轮路径中，白条未再出现：[修复后实窗](screenshots/windows-scale-1.25-general-overlay-fixed.jpg)。窗口正常退出，进程 `exit_code=0`。本机原始诊断位于 ignored `out/build/windows-msvc-scroll-validation/manual-input-overlay-fix-1.25.stdout.txt`。
- 1.25 档其余分类与 1.5/2.0 档的完整回归、72 项逐一可读性及 Button 按下态仍未完成；7.2/7.3/7.4 继续保持未勾选。此局部修复也不构成 240 Hz 滚动性能验收。

## 诊断边界

- 五次退出码均为 0；运行时诊断保存在本机 ignored `out/build/windows-msvc-scroll-validation/manual-scale-*.stdout.txt`。本文件保留可审查的关键信息，不能用这些输出代替屏幕视觉验收。
- 普通窗口保持运行时 `idle_after_animation=0`，同时 Live Samples 的 Loading 示例保持动画；这组数据不能单独证明 idle 回归，需在 Loading 停止或 motion-disabled 条件下另测。
- 当前记录是问题发现、局部修复与部分操作证据，不是 Windows passed evidence；`tasks.md` 的 7.2/7.3/7.4 保持未勾选，Linux 项目不受影响。
