# Design

## Context

见 proposal.md。基线 8056de67953ee73015c7559f1782033d85e08573 已有 Skeleton、semantic text style、retained region clip 和 WindowComponentServices。设计合同来自本机 Ant Design 6.6.5 的 Statistic.tsx、Number.tsx、style/index.ts、文档及 snapshot；不引入新依赖。

## Goals / Non-Goals

目标：复用既有布局、文本、Skeleton、输入边界与场景；仅增加组件与 Theme 接入。非目标见 proposal，不增加计时占位 API，不修改 GPU ABI、父维护 Gallery 或通用输入规则。

## Decisions

- `StatisticValue` 保存 int64/uint64/double/String。整数和 double 使用 `to_chars`，浮点采用最短可往返表示；指数形式保持原样，与上游正则识别边界一致。String 十进制直接处理，不经过浮点转换。precision 为 optional int，负数按上游合同视为 0，超过 100 抛 invalid_argument，以限制无界补零。相比统一转 double 或 `to_string`，该方式保留整数范围和超过六位的小数。
- formatter 为返回 String 的 typed 回调，获得原始 StatisticValue；需要富值时使用独立 `StatisticValueContent` slot，通过读取外部 Signal 自行更新。slots 在初次挂载执行一次；不在普通数值变化时重建。富 slot 优先于相应普通文本。格式化在候选 model 上执行成功后才发布，用户回调后重新核对完整 generation。
- 标题区域与 Skeleton 分开，Skeleton 包裹 retained 水平值区域。按 prefix、value、suffix 顺序分配有限宽，垂直居中并逐区域裁剪；短高压缩分配框，不改自然字体。根裁剪 decoration，内部区域只裁 content，延续父的 focus 装饰边界。隐藏根 out_of_flow，并通过现有输入 boundary gate；Skeleton 自己拥有 loading gate。
- Theme 增加 title/content color、font size/line height、font family、title gap、affix gap、loading padding；默认 titleFontSize=fontSize、contentFontSize=fontSizeHeading3。全部最终派生值在发布前校验，追加两个 identity。旧 IIFE 原文和顺序不变；大型 ThemeConfig/Snapshot 使用 heap。现有 5 份 goldens 重新生成后深比较旧字段。
- 使用 `windows-msvc-headless`、Ninja Multi-Config、MSVC、最多 2 jobs 构建定向 Debug/Release 测试。平台通用逻辑只在本 Windows 机器验收一次并同时核对两 configuration；不代表 GPU/native/Linux/system IME/AT。

## Risks / Trade-offs

- formatter 可同步卸载 → 回调前复制候选 model，回调后用 ComponentId generation 重新查找，不保留 state 引用。
- 富内容自然尺寸大于可用区 → 每个 region 自有 hard content clip，检查实际 R8 coverage、quad/effect 与 hit。
- loading/hidden 引发 blur 并卸载 → 状态与 dirty 先发布，最后执行 input reconciliation，之后不访问旧 state。
- Theme 结构过大 → 新 token shared ownership，验证测试在默认 1 MiB PE 栈运行；不更改 stack reserve。

## Migration Plan

纯新增 API；按规划、实现与验证阶段保存本地英文 Conventional Commits。父线程审查后集成，worker 不访问远端、不 push、不创建 PR、不 archive。
