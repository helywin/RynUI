# Proposal

## Why

097 已完成原生单选 TreeSelect，但 Token Gallery 仍只有 planned 参考卡片，使用者无法从目录进入层级选择和表单流程。需要把已验收组件接入实际业务示例，直观展示 authority、键盘、有限列表和重置合同。

## What Changes

- 在现有样例末尾追加层级目标选择和 Form String 字段两个流程，不重排旧样例。
- 提供受控选择／开合拒绝与重试、清空、响应式标题／节点禁用、全局禁用／只读／显隐恢复，以及保存／必填反馈和 reset。
- TreeSelect 目录按钮通过共同生产锚点跳至真实样例；单独更新该组件支持条目为如实 partial。
- 新增纳入完整 frame 入口的 focused 检查：真实目录／指针／键盘、三主题双宽、实际 glyph/fill/hit/适用 focus coverage、refs／editor／行身份保持与同值 reset。
- 冻结已发布 Core/API/Theme/RC/系统字体/CMake 与旧 Gallery IDs、函数体／调用序和非目标支持条目。完整 frame/native 在独立统一验收中执行，不冒用旧批次结果。

## Capabilities

### New Capabilities

- `tree-select-gallery`: TreeSelect 真实导航、层级选择、输入门禁恢复与表单提交／重置示例及平台通用 Recording 验证。

### Modified Capabilities

无。当前 `openspec list --specs` 无已同步主 spec；新增 delta，不改历史 change。

## Impact

影响 Gallery definition、共同锚点、支持 overlay／生成目录、frame tests、TreeSelect 文档与完成记录。无新依赖、公开 API、Core、Theme、renderer、RC 或构建结构变化。主要风险是复用目录定位、有限视口下 popup 可达性及旧样例身份；规划不计入实现。
