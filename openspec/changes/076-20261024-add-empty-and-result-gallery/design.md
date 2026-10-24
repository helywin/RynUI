# Design

## Context

当前 Gallery 在完整文档尾部追加 common/data/panel 样例，拥有 262 个稳定标识和 273 个 live samples。074 已有聚焦 Recording 测试及实际 D3D12 启动 smoke。073 的 typed Empty/Result 使用原生 Icon、Text 和保留 slots，正在修复 margin 放置与零尺寸 Theme 边界；本 change 在其完成集成后编译运行。

## Goals / Non-Goals

**Goals:** 复用实际组件创建可操作样例；有限视口检查实际 glyph/节点；保持既有标识、顺序和输入身份。

**Non-Goals:** 复刻大型 SVG 插画、图片下载、网络状态服务、全量上游 Web API；不建立新的 UI 容器或通用样例框架，不修改 renderer；不把 headless 或启动 smoke 提升为人工视觉/GPU 矩阵或 Linux 系统输入验收。

## Decisions

1. 在既有 panel 样例之后追加状态样例，状态保存在 GalleryState 的 Signal 中，保留 slot lambda 只挂载一次。直接使用 EmptyProps/ResultProps 与 typed slots，避免模拟静态卡片或手动主题绘图。
2. 默认 Empty 提供创建按钮改变说明，紧凑 Empty 展示说明切换，自定义 Empty 使用带 margin 的原生 Icon。Result 提供受控状态及状态切换按钮、异常展示、富 icon 和详情/草稿。有限但互补的样例控制完整文档长度。
3. 在 frame test 中添加聚焦入口并纳入完整 frame 流程；通过实际 input event 和 Text scene 验证描述/状态、草稿、身份及默认/Dark/Compact 1200/560 宽布局。更新 inventory 断言采用实际运行计数，不删除旧断言来掩盖变化。
4. build 使用 CMakePresets.json 的 windows-msvc / Ninja Multi-Config / MSVC。组件共同合同由 073 的 windows-msvc-headless 验收；本 change 运行 Gallery 相关 Recording/contract 测试和 Windows 原生 --smoke，证据分别标明范围。
5. 保留现有 Empty/Result 组件目录按钮及 identity，将这两个组件的跳转锚点解析到实际交互样例的稳定标题；其他组件、分类与章节仍使用原参考锚点，组件总览继续保留全部参考卡片。运行时与 Recording 测试共用锚点解析和组件请求跳转函数，测试通过真实目录按钮进入样例，不用手工平移替代该入口。

## Risks / Trade-offs

- [完整长文档增加高度和执行时间] → 复用现有有限 viewport 与独立聚焦入口；完整 frame 只在最终新增样例后执行一次，后续无行为变化不重复。
- [Theme 更新与 Input 草稿丢失] → 保存真实 Component/interaction/Text scene 身份，使用实际编辑和三个 Theme 更新回归。
- [目录夸大能力] → Empty/Result 保持 partial，明确 Icon 替代插画及系统平台验收边界。
