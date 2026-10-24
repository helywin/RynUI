# Proposal

## Why

RynUI 已有 Spin，但缺少加载首屏时表达内容结构的原生静态骨架。此变更新增最小常用 Skeleton，并在 loading 往返时保留真实内容的组件与编辑状态。

## What Changes

- 新增 typed reactive `SkeletonProps`、`SkeletonContent`，支持 title、paragraph 行数及 typed 宽度、可选 avatar 与 round。
- 新增静态 `SkeletonAvatar`、`SkeletonButton`、`SkeletonInput`，提供对应 size、shape、block 语义；占位本身无焦点和点击交互。
- loading 时隐藏并门禁 retained 内容，恢复时保持组件、editor、draft、refs 身份；使用既有输入边界清理 popup、preedit 和输入会话。
- 接入 Skeleton Theme seed/algorithm/override/inherit、identity/hash/serialization/equality、精确 dirty 和 logical CPU scene v3。
- 提供 focused headless 测试及 `docs/skeleton.md`。首版为 partial 静态支持，不暴露 active，不包含 shimmer、Image/Node、网络、Web/AT、Gallery。

## Capabilities

### New Capabilities

- `native-skeleton`: 原生静态骨架组合、standalone 形态、retained loading 门禁及 Theme/scene 合同。

### Modified Capabilities

无。当前主 specs 清单为空。

## Impact

涉及公开 Skeleton 头、独立 component host/布局模型、窗口服务注册、Theme 扩展、CMake/tests 和本 change 文档。无新依赖，无 renderer/SDL/平台 SDK/GPU ABI 改动。参考本地锁定 Ant Design 6.6.5 `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 skeleton docs/style/source。风险集中在有限区域分配、输入回调重入及 Windows Theme 栈容量；沿现有服务与 heap 解析模式实现并验证。仅本 worktree、本地英文阶段提交；README、Gallery、support overlay 与 global completion 由父线程负责。
