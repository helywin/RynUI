# Design

## Context

动机见 proposal。当前 Card/Feedback 已验证 typed slots、semantic typography、ThemeScope 分相位订阅与 retained logical scene；Text/Icon 共用 glyph 渲染与内容 clip。源基线为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，本地 `D:/code/RynUI/out/upstream-ant-design-6.6.5/components/{empty,result}` 可核对，style 文件 SHA256 由 sources.lock.yaml 锁定。

## Goals / Non-Goals

目标是沿用现有 WindowComponentServices 和场景资源生命周期，普通更新只改变 signal、布局或材质。不新增 renderer/backend 路径，不变更 Gallery，也不重复建立状态、交互或泛型容器框架。

不实现上游 DOM/CSS API、大型 exception 插画、动画、网络资源与动态 slot 替换。GPU、Linux、系统 IME 不在本次平台通用完成条件中。

## Decisions

- 使用一个私有 EmptyResultComponentHost 管理两种相邻静态展示。公开头文件独立，typed slots 不互换；内置 image/icon 使用既有 Icon（Empty 使用 InboxOutlined，Result 使用状态图标；exception 使用 Lock/FileSearch/CloudServer 等语义图标），没有 raster 依赖。
- `ResultStatus = std::variant<Status, ResultException>`；Status::Default 对应 info，Success/Warning/Error 对应同名语义；None/Processing 也静态映射 info，不引入新的通用状态枚举。
- 每个区域固定挂载一次，空标签只切换 branch active；image/icon slot 优先于默认图标；Empty 的 None 关闭整个 image 区域。extra 区域使用带 gap 的居中、可换行 Flex，支持多个原生控件；content 左对齐并带 token padding/background。拒绝在响应式更新中重跑 callback。
- 组件内使用直接 ComponentLayout 纵向测量和放置，有限宽度占满可用宽度，区域内容居中；图标分配限制于可用宽高，实际 glyph 由区域 clip 限制。每个内容区域有独立 passive clip viewport，extra 使用独立外层 viewport，留出正常 focus 装饰余量；实际短高分配不依赖 root 无限 clip。Text 使用现有自动换行。
- Empty/Result 各自提供 colors/metrics tokens 与 override，复用 Theme component seed/algorithm 配置惯例。新增 snapshot、诊断序列化、runtime token identities 与颜色/度量订阅；新 token 不改变旧字段。
- 由官方 `windows-msvc-headless` preset 配置独立 `out/build/empty-result-headless`，Ninja Multi-Config/MSVC，最多 4 jobs。平台通用行为在本 Windows 机器验证一次，Debug/Release CTest 为工具链和优化模式回归。

## Risks / Trade-offs

- 文本/原生控件自身最小尺寸超出短区域 → 区域 clip 限制实际 glyph 与 surface；正常高度额外保留装饰空间，测试实际覆盖率与 clip。
- Theme 大量已有诊断字段 → 从新可执行文件重建五份 golden，递归比较旧字段（identity 除外）。
- 原生图示与上游插画不同 → 独立说明其语义映射，拒绝声称视觉等价或 native GPU 验收。

## Migration Plan

新增 API，无既有调用迁移。按规划、实现与测试证据形成英文 Conventional Commits，仅本地提交；父任务负责集成和后续评审，不 push、不建 PR、不 archive。
