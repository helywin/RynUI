# Design

## Context

061 的 ModalComponentHost 已拥有完整 retained 窗口层、模态栈、延迟焦点恢复、正文滚动、生命周期通知和 child popup 归属。动机见 proposal；新增第二个 host 会令两套栈争抢同一 input boundary。

## Goals / Non-Goals

Goals：将现有 host 的模态容器能力同时用于 Modal/Drawer，保留 Modal 的 API 与默认行为。Drawer 独立公开 API 和 Theme，增加四边几何与固定区域布局。

Non-Goals：不建立通用 portal/service 抽象，不改变 renderer/ABI；未承诺能力见 proposal。Gallery、真实 Windows/Linux 窗口与人工 IME 验收由父任务独立负责，不作为本任务已完成证据。

## Decisions

- 在 Modal host 中增加 Drawer mount 与小型 panel 配置；共享全部栈和输入代码，拒绝复制 1000 行实现。Drawer 无默认确认按钮，只挂载可选 typed footer。
- Drawer placement/width/height 在纯模型中原子验证，窗口几何独立计算；正文与固定 header/footer 共用原有 retained nodes、滚动和焦点显露。
- 独立 Drawer ThemeToken、override、snapshot identity 和五组订阅；颜色、字体、effects 可共享既有强类型值，布局字段仅公开 Drawer 实际支持部分。窗口层与 Modal 使用同一排序规则。
- 使用锁定 Ant Design 6.6.5 源码；默认 378，body padding24，header16/24，footer8/16，标题16/24及600字重。首版静态呈现。
- 使用 MSVC、Ninja Multi-Config、windows-msvc-headless preset 和当前 worktree 自有输出，最多4编译作业；只构建和执行 Drawer、Modal 及相关边界聚焦测试，不重复全仓库测试。

## Risks / Trade-offs

- [共享 host 回归] → 执行原有 Modal model/API/theme/boundary/component 测试及双向混合嵌套测试。
- [超小窗口固定区域争用] → footer/header 高度夹紧，剩余正文可为0，所有绘制与输入受窗口clip限制。
- [主题跨组件污染] → Drawer 独立存储、订阅与identity测试；Modal 默认值保持。
