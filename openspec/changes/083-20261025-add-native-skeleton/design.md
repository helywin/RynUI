# Design

## Context

动机见 proposal。现有 Spin 用 retained content、`set_input_blocked` 与参与者 boundary reconciliation 清理输入；Collapse 用 branch active 保留内容身份。Skeleton 将组合这两个既有路径，独立 host 接入 WindowComponentServices，不改 FocusManager、renderer 或旧组件合同。

## Goals / Non-Goals

目标是可实际绘制的静态结构、有限 constraints 下的布局和 retained loading 往返。边界见 specs。首版不做通用 placeholder 框架，也不借助 React/CSS、新依赖或每帧 timer。

## Decisions

1. **Typed API 与宽度**：`SkeletonTitle`/`SkeletonParagraph`/`SkeletonAvatarOptions` 为普通等值配置，通过 `Prop<T>` 原子响应。标题宽度及每行宽度用 typed `SkeletonWidth`（Auto 或 0..1 比例）表达结构比例；尺寸、颜色、间距、圆角全部来自 Theme。段落 `widths` 非空时按行索引，未给出的行满宽；单个 `lastLineWidth` 只控制尾行。默认标题 38%/50%/满宽，行数 3/2，尾行 61% 或满宽，精确按锁定 Skeleton.tsx/Paragraph.tsx 组合规则。拒绝 CSS string 和任意视觉 style。
2. **Packing 与有限区域**：逻辑 CPU quad 顺序固定为 avatar、title、各 paragraph 行。布局计算 avatar、gap、body 的各自有限区域，逐段截断高度，不通过裁短带圆角 quad 冒充保留溢出。动态行数只更新本组件 surface topology；theme/普通 loading 不重挂真实 children。有界宽取可用宽，无界宽使用 Theme intrinsic width；standalone avatar 按 size，button 默认 2×高度、circle 为 1×，input 为 5×，block 取父可用宽。square avatar 零圆角；button Square 保持上游 default 矩形比例与小圆角（上游 square class 无独立尺寸规则）。round title/paragraph capsule，button round capsule。
3. **Retained 生命周期**：真实 content 一次挂载到独立 region，loading 时 branch inactive 并注册该 generational ComponentId 的 input block；先发布状态、dirty 与 branch，再 reconciliation。回调后重新获取同代 state；cleanup 只通过 registry 注销自己拥有的完整 generational ID，避免回调访问旧 state。宿主 root 永远不被自身隐藏，恢复操作不被锁住。无 content 且 loading=false 时 root layout_visible=false，消除父 Flex/Space 的 gap；有 content 保持外部 LayoutStyle。内容 region 用 BoxLayout 保持原生子控件，root 的 content clip 限制分配区域，子控件外扩 focus decoration 不误裁在 editable viewport 上。
4. **Theme**：独立 `SkeletonColorToken`/`SkeletonMetricToken`/override/config；seed/algorithm/inherit 遵循邻近组件，但 algorithm=false 忽略局部 seed。在 override 后验证所有最终派生 metrics 及组合 extent。Identity 仅追加颜色和度量项；snapshot 序列化新 Skeleton 字段，旧 31 个 IIFE 保留原文及顺序，静态 default config、heap descendant preflight 与 self guard 不改。用新 binary 生成 5 份 goldens，再与父基线深比较，旧 typed values 必须逐项相等。
5. **验证与提交**：规划提交后直接 apply（用户明确持续授权）。三段实现提交为 API/模型、Theme/host/scene、输入组合/文档验收。Windows 正式 `windows-msvc-headless`，MSVC、Ninja Multi-Config，max 4 jobs；focused Debug/Release 加一次去重 affected union。真实窗口/GPU/Linux/system IME/AT/Gallery 不在本机通过声明中。

## Risks / Trade-offs

- [输入清理触发卸载/重建] → 回调前完成 state 使用，回调后按完整 ComponentId 查同代 state，并测试 refs、preedit、controlled popup 和其他 owner block。
- [有限或零尺寸恢复及 clip 交错] → 测试真实 Quad/Glyph/Hit clip、短高、零宽高恢复及三种 Theme；内容与占位分支分开。
- [Theme 解析 Windows 栈边界] → 新增 token 使用既有 heap/分离调用模式，不扩大 stack、不改旧 IIFE；实际失败才定位。
- [静态 partial 合同] → 文档明确不含 active/shimmer、Image/Node、完整 Ant API；不接受无效果字段。

## Migration Plan

新增公开 API，不替换既有组件。仅本地阶段提交，可通过逐提交回退撤销；父线程负责后续集成和独立审查，不在此 push、PR、archive 或写其他树。
