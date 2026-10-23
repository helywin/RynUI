# Design

## Context

`InputComponentHost` 是单行编辑、session、Focus/ref、键盘和 scene 的共同入口；Input affix 已使用 PressableBehavior。锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 `components/input-number/index.tsx`、`index.en-US.md`、`style/index.ts`、`style/token.ts` 已读取：默认 step=1、keyboard、可关闭 controls、Input variants、共享 Input 样式、controlWidth=90、handleWidth=controlHeightSM-2*lineWidth、handleFontSize=fontSize/2。

## Goals / Non-Goals

目标是在同一 Input identity 上提供可用的数值编辑。非目标见 proposal；不新建 editor/session 或泛型 Form 基础设施，不修改公共 scene/renderer 合同。

## Decisions

- 数值 API 为 `optional<double>`。支持绝对值不大于 9007199254740991、最短十进制表示最多 12 位小数的有限数；内部拆分整数与 12 位小数进行整数加减/舍入，避免以 double 连加。显式 precision 为 0..12，四舍五入远离零；范围向内量化，不能表示任何值的范围拒绝。
- value/defaultValue mount 时互斥，模式固定。完整且范围内的草稿发 change；空值发 nullopt；不完整或非法草稿保留且不发数值。blur/Enter 处理尾点、量化及范围夹取；无法解析时恢复权威值。controlled 不写入权威值，外部回写相同候选值保留正在输入的草稿；拒绝后提交规范化恢复外部值。
- 在 Input 内挂接局部 numeric config/model，复用既有 commit/blur、Keyboard 和 cleanup；step 从权威值计算，空值从 0 开始并限制到范围。preedit 时按键及按钮均不发布。controls 使用 retained 两行 Up/Down actions 和 PressableBehavior，鼠标保持输入焦点，键盘 Up/Down 由 Input 处理；不注册 wheel consumer。
- Astra R1 修复：最近舍入若使步进反向，改为沿请求方向量化；夹取后仍检查非空权威值的实际方向，无合法结果则不变。Input 文本同步比较编辑器 composition 状态与上次资格快照，只在变化时刷新数值按钮，不依赖各取消入口逐一补调用；disabled/readOnly 和共同逻辑门禁保持原合同。
- 输入表面继续消费现有 Input tokens，新增 InputNumber 专属宽度、handle metrics/colors，独立 identity/dirty 分组；始终可见的 controls 是首版原生适配。typed prefix/suffix 使用现有 Input slot 合同。
- Windows 使用独立 `out/build/windows-msvc-headless`、Ninja Multi-Config、MSVC Debug/Release，最多 4 编译作业。仅运行聚焦数值、Input 回归与 Theme golden/boundary checks；本机 helper 不入库。

## Risks / Trade-offs

- double 不能表达任意精度 → 支持域和拒绝行为显式文档化；十进制内部运算后只在 typed API 边界转 double。
- callbacks 可同步回写或销毁组件 → callbacks/value 按值保存，重新查 generation，销毁后的步骤不继续操作。
- headless 合同不能证明系统 IME/GPU → 此次只记录自动注入 preedit/commit 和 native 事件路由；真实窗口、Linux、人工 IME 由父任务独立验收。

## Migration Plan

新增 API，不改变现有 Input 行为；父任务可按本地阶段提交集成或回退。仅本地提交，不 push/PR/archive。
