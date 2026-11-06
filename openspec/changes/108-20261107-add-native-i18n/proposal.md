# Proposal

## Why

RynUI 已有多个组件 Locale 和日期格式器，但缺少统一的语言包、应用消息词典及可响应的作用域；组件默认英文、Gallery 混合文案和部分硬编码提示无法一起切换。需要在保留用户编辑状态与组件身份的前提下，提供真实的 zh-CN/en-US 原生国际化。

## What Changes

- 复用现有 Picker、Pagination、Transfer、Upload、Image、Form、Descriptions、QRCode 和待集成 Statistic Locale，补全内置文案并提供不可变包与明确 fallback。
- 新增 typed 应用消息、命名参数、有限整数复数及精确数字/既有公历格式适配；不引入新的运行时依赖。
- 建立窗口隔离、嵌套继承的唯一 ConfigProvider/config context，支持语言、格式语言、消息目录、direction、size、disabled 与现有 Theme 的组合。
- 接入真实组件、保留编辑会话和弹层，并与 107 的 App/服务实现共享 captured context；服务生命周期依赖仍由 107 实现，不能用简单 Holder 冒充。
- Gallery 壳、导航和示例说明使用公共 i18n API，保留自动化稳定 ID、源码引用、业务输入和多脚本 fixture。
- **BREAKING**：为区分默认校验消息与显式英文，`FormRule.message` 改为 `optional<String>`；常见 String 赋值保留，直接按 String 读取需迁移。`.required()` 与 `.required(String)` 保留为来源明确的重载。

## Capabilities

### New Capabilities

- `native-i18n`: 不可变语言包、应用词典、fallback、参数、格式与响应式配置作用域及组件/服务接入。
- `gallery-localization`: 使用公共国际化能力的 Gallery 双语界面、稳定身份与后台自动原生验证。

### Modified Capabilities

无。当前主 spec inventory 为空；本变更与仍在实施的 107 `application-context-components` 共享 ConfigProvider/App 实现与证据，不复制 Provider 或提前完成 107。

## Impact

涉及公开 i18n/config_provider API、Core 数据与 runtime context、ComponentHost 的全部 slot/append 路径、内置文案组件、Form 消息来源、App/服务桥接、Gallery 和定向测试。保持 Theme golden、Core/renderer 边界、scene/GPU ABI 与显式业务 String 语义；所有截图与原始运行输出留在 ignored out。
