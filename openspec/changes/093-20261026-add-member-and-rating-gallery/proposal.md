# Proposal

## Why

Avatar 已发布原生首版，Statistic 和 Rate 正在独立审查；三个参考目录仍标记 planned，使用者还不能从 Gallery 体验成员展示、数值格式化及实际评分。需要在组件审查并集成后接通真实业务操作，保留旧流程。

## What Changes

- 在现有样例末尾追加成员头像、统计指标与服务评分，使用公开 API 和真实原生 Input/Button/Popover/Tooltip。
- Avatar 展示文字自适应、形状/尺寸、Group 数量限制、溢出访问和显隐恢复；Statistic 展示真实 typed 值、精度/格式化、loading 与富内容保留；Rate 展示半星、清空、受控评分、编辑门禁及富提示。
- 三个组件目录直达实际样例；增加一组定向 Recording 验收，覆盖真实目录/指针/键盘、Default/Dark/Compact × 宽窄有限视口、实际 glyph/fill/hit/focus 与业务草稿保留。
- 更新仅三个目标 support overlay、生成目录及 API Gallery 文档；旧 390 IDs、36 个样例函数体/调用顺序和 70 个非目标支持条目保持。实施前再次实测函数数量，不把规划数值代替比较。

## Capabilities

### New Capabilities

- `member-rating-gallery`: 成员头像、统计指标与服务评分的可操作原生 Gallery 合同。

### Modified Capabilities

无；当前主 specs 目录为空。

## Impact

范围仅 Gallery definition、生产 anchor、frame tests、三个组件支持条目/生成目录、API 使用文档及本 change。必须先集成审查通过的 088/089/090，不能将未审源码或 worker 测试当作 main 通过。Core、公开 API、Theme、goldens、renderer 和 CMake 不由此 change 修改。主要风险是 overflow/Tooltip 的窗口层归属、半星事件位置、富内容的裁剪与 retained 生命周期，通过实际场景和事件验证。完整 Gallery/native 在用户指定收尾时统一运行，不将本组结果扩大为 GPU/人工视觉、Linux、系统 IME 或 AT。
