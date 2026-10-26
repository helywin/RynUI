# Proposal

## Why

RynUI 缺少原生统计值展示组件。此次增加真正 retained 的 Statistic，使标题、数值和富前后缀可响应更新并保留编辑状态。

## What Changes

- 增加 typed StatisticProps、数值/String value、precision、分隔符、formatter 和 title/value/prefix/suffix 富 slots。
- 根据锁定 Ant Design 6.6.5 源码（4a39f54842eade4e565ab336ef6097cd7e723cdd）映射截断补零、分组和 loading；loading 复用已有 Skeleton。
- 增加完整 Statistic Theme 派生、继承、独立 seed、验证、订阅和诊断序列化；旧 token 与 identity 顺序保持。
- 增加有限空间/隐藏恢复、真实 glyph/quad/hit clip、输入边界与清理验证，以及可编译文档示例。
- 非目标：Timer、Countdown、Countup、数值动画、locale 国际化、DOM/CSS 接口、Gallery 与父线程维护的完成度文档。

## Capabilities

### New Capabilities

- `native-statistic`：原生统计值格式化、retained 富内容、Skeleton loading、Theme 与资源生命周期。

### Modified Capabilities

无。

## Impact

影响公开组件和 token header、component/model/theme、窗口服务注册、CMake、定向测试及 docs/statistic.md；不改 Core/renderer 依赖方向或 GPU ABI。风险集中于大整数与小数保真、formatter 异常原子性、狭小 clip、loading 输入边界及巨大 Theme 栈占用。平台通用逻辑在本 worktree 的 Windows/MSVC HEADLESS Debug/Release 定向验收；无 GPU、原生窗口、Linux、系统 IME/AT 验收声明。仅本地 Conventional Commits，交父线程审查集成。
