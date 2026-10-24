# Proposal

## Why

常用录入组件需要能够直接运行的组合样例，帮助使用者搭建实际业务页面。075 Form 已合入 main，077 DatePicker 正修复审查问题，078 TimePicker 正在实施；本变更按依赖逐阶段补齐真实填写、校验、提交和重置流程。

## What Changes

- 在已有样例末尾追加真实 Form/FormItem，显式绑定 Input、Select、InputNumber 和 Checkbox；通过原生按钮演示校验失败聚焦、成功结果、重置及禁用/只读。
- 077、078 分别通过组件验收并合入后，追加真实 DatePicker/TimePicker 样例，演示受控选择、清空、限制及输入草稿；使用 Form 已支持的 String 字段显式适配日期/时间格式，不冒充 Form 原生支持新的字段类型。
- 目录中的 Form/DatePicker/TimePicker 能直接定位实际样例，保留其他目录和既有样例身份；用真实目录、指针/键盘/输入及实际 glyph/clip 验证 Default/Dark/Compact 宽窄视口。
- 更新稳定标识、实际 inventory、partial 支持范围与 API 文档，最终统一运行完整 Gallery 与 Windows 启动 smoke。

不扩展 Form/日期/时间组件首版本身的合同，不增加后端保存、网络服务、业务 schema、完整上游 Web API 或平台无障碍桥接。未集成的依赖不以占位控件代替，不提前宣称对应样例完成。

## Capabilities

### New Capabilities

- `form-and-picker-gallery`: 可导航、可实际操作的原生表单及日期时间组合样例。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

涉及 token Gallery definition/runtime、共用导航锚点、Recording tests、support overlay/生成目录、相关 API docs 与 completion 清单。复用已验证的 076 导航和现有 Gallery/组件，不修改 renderer、GPU ABI 或引入 Web/时间库。075/077/078 的组件验收与本变更的 Gallery 验收分别记录。
