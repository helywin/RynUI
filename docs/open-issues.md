# RynUI 未解决问题总表

本文汇总 RynUI 当前已经确认但尚未解决，或尚未完成真实平台验收的问题。它是项目级问题入口，不替代 OpenSpec change 的 `tasks.md`、验收顺序或平台 evidence。

状态约定：

- `open`：问题已确认，仍在调查或等待正式方案。
- `blocked`：已有明确阻塞条件，当前 change 范围内不能继续完成。
- `pending`：计划内工作尚未执行完，但没有被证明为上游阻塞。
- `closed`：实现与对应验收均已完成；关闭后保留条目和证据链接。

## 当前摘要

| ID | 状态 | 问题 | 影响范围 | 临时方案 |
|---|---|---|---|---|
| `LINUX-WAYLAND-RESIZE-001` | `closed` | GNOME 原生 Wayland 窗口 resize 严重延迟 | 已解除 resize 阻塞 | patched bundled libdecor/SDL |
| `LINUX-FONT-RASTER-002` | `pending` | Linux Fontconfig raster policy 与原生 Wayland 前后截图尚未完成 | 6.6 | 无；必须保留真实平台证据 |
| `WINDOWS-DWRITE-RASTER-003` | `pending` | Windows 字形 coverage 尚未完成实际 DirectWrite grayscale raster path 与多 scale 复验 | 7.5 | 当前 FreeType 路径不能冒充 DirectWrite 验收 |
| `CHANGE-CLOSEOUT-004` | `blocked` | Change 尚不能最终收口 | 8.1 | 等待所有独立平台任务完成 |

## LINUX-WAYLAND-RESIZE-001

- 状态：`closed`（2026-08-28）
- 严重程度：高；直接影响窗口交互和原生 Wayland 验收。
- 现象：混合刷新率、分数缩放的 GNOME 50 环境中，窗口 resize 离散或带明显动画式延迟；左侧 240 Hz 输出更严重。
- 已排除：RynUI layout 耗时、事件队列堆积、Debug 优化差异、单一 Vulkan present mode、Flex/Space scene 复杂度。
- 已确认对照：同一 Release 二进制强制 X11/XWayland 后，用户确认 resize 正常。
- 解决方案：change `007-20260828-fix-linux-wayland-libdecor-resize` 锁定 patched build-local libdecor 0.2.5，并在 SDL3 中按 frame callback 合并确认最新 configure；另保留与 pointer input 同批到达的 expose redraw。
- 验收：原生 Wayland 上 240 Hz/scale 1.333、60 Hz/scale 1.0 和双向跨输出的 minimal、layout、Token Gallery 均由用户确认通过；协议计数全部闭合且正常退出。
- 详细复现、修复、计数和关闭证据：[linux-wayland-resize.md](issues/linux-wayland-resize.md)、[Linux native Wayland evidence](../openspec/changes/007-20260828-fix-linux-wayland-libdecor-resize/evidence/linux-native-wayland-validation.md)

## LINUX-FONT-RASTER-002

- 状态：`pending`
- 影响任务：6.6。
- 未完成内容：读取 Fontconfig matched pattern 的 antialias、hinting、hintstyle、rgba、lcdfilter、embedded bitmap，并保存原生 Wayland 修复前后相位截图与 12/14/16 px CJK/Latin 人工核对结果。
- 与 resize 问题的关系：原生 Wayland resize 阻塞已解除，但 task 6.6 的字体相位截图和 12/14/16 px 人工核对仍需独立完成。
- 关闭条件：实现通过相关单元/集成测试，并在原生 Linux Wayland 完成规定截图、telemetry 与人工核对。

## WINDOWS-DWRITE-RASTER-003

- 状态：`pending`
- 影响任务：7.5。
- 未完成内容：Windows system/custom face 的实际 glyph raster 尚需接入 DirectWrite grayscale bitmap path，使用目标 monitor rendering parameters 与推荐 rendering/grid-fit mode，并重新完成 100%/125%/150%/200% Token Gallery 对照、CTest 和 evidence。
- 当前边界：已有 DirectWrite 字体发现或 family 选择不等于字形 coverage 已由 DirectWrite 栅格化，不能把静态代码或 Linux 结果标记为通过。
- 关闭条件：在 Windows/MSVC/D3D12 实机完成实现、自动测试、多 scale 截图和独立 evidence。

## CHANGE-CLOSEOUT-004

- 状态：`blocked`
- 影响任务：8.1。
- 阻塞项：`LINUX-FONT-RASTER-002`、`WINDOWS-DWRITE-RASTER-003`。
- 当前禁止动作：不得 archive change，不得把 pending evidence 改为 passed，不得用 XWayland/Linux/静态审查替代缺失平台证据。
- 关闭条件：上述问题关闭后，运行 OpenSpec doctor、strict validate、完整差异检查，并确认工作区与平台清单一致。

## 维护规则

1. 新发现的未解决问题先在本文件登记稳定 ID，再链接详细诊断或 evidence。
2. 状态变化必须附上可复现命令、测试结果或真实窗口证据，不能只写“已修复”。
3. `closed` 条目不删除，以便后续回归时复用环境与验收条件。
4. 平台问题必须保留平台身份；XWayland、原生 Wayland、Windows、headless 结果不得互相替代。
