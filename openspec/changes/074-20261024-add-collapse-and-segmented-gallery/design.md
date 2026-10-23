# Design

## Context

动机见 proposal。现有 LiveSamples 可追加内容，frame fixture 已有按标签定位、文档滚动、真实指针/键盘、主题更新和身份断言；070/071 的公开组件保持各自首版边界。

## Goals / Non-Goals

复用公开 typed Props/slots 与 GalleryState Signal，展示实际回写和编辑保留，不增加组件 API、后台任务或动画。

## Decisions

- 末尾追加样例，保留既有组件索引；不重建现有目录或示例树。
- Collapse 使用有限固定面板，正文放实际 Input 与 Select，extra 使用独立 Button；外部按钮切换 accordion/disabled，验证展开状态和正文身份。
- Segmented 提供受控横向带图标、等宽和纵向示例，用反馈文字确认选择；默认标签与 rich slot 均由既有主题控制。
- 共同验收使用 Windows MSVC、Ninja Multi-Config、windows-msvc Debug Recording fixture；先新聚焦场景，再完整 token_gallery_frame 与目录合同。完整目录绘制使用现有 60,000px 测试视口，实际滚动交互仍用 900px。

## Risks / Trade-offs

新主题字段可能增加 Debug 栈占用 → 保留已复现问题对应的组件解析调用分离，运行主题切换和实际 Gallery。目录支持说明改变窄屏高度 → 更新实际 inventory 并保留整页可达性合同。收起正文可能残留输入/浮层 → 覆盖未聚焦 popup、重复开合和编辑器身份。

## Migration Plan

先提交精简规划，再实现并验证样例，英文提交并推送 main；可独立回滚 Gallery 提交。原生 GPU/系统 IME、Linux 和人工视觉不由 Recording 结果代替。
