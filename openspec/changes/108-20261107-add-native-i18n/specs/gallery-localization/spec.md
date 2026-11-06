# Spec Delta

## Purpose

使 Gallery 作为真实使用公共国际化 API 的原生应用，完整展示中文与英文的界面和示例说明，并能通过后台自动输入和局部截图验证语言切换、状态保留及组件集成行为。

## ADDED Requirements

### Requirement: Gallery 使用公共双语消息能力
Gallery SHALL 使用公共语言包和应用词典翻译壳、导航、页面说明、示例操作说明和实际内置组件文案，不另建私有全局翻译机制。

#### Scenario: 页面实时切换
- **WHEN** 用户在任意当前组件页切换 zh-CN/en-US
- **THEN** 已挂载页面的界面和说明更新，不重新构建整页，不重置示例业务状态、编辑草稿、选区、滚动或已打开弹层

### Requirement: 消息身份与业务内容分离
Gallery SHALL 保留稳定 sample/action/target ID、源码引用、API 标识符与明确的多脚本文本 fixture；普通业务 String 不被批量重写。

#### Scenario: 翻译 key 不泄漏
- **WHEN** 词典缺少一个已声明的 Gallery 消息
- **THEN** 显示声明的 fallback 文案，自动化 ID 保持原样，不在普通界面泄漏未解析的 gallery key

#### Scenario: 固定示例内容
- **WHEN** 示例用于 CJK/bidi/业务数据或代码展示
- **THEN** 明确登记的 literal fixture 与用户编辑内容保持不变，覆盖检查按具体来源分类而非禁止某种文字的正则

### Requirement: 可独立运行的国际化示例
系统 SHALL 提供只使用公开 API 的应用示例，演示嵌套 Provider、消息参数、复数、格式语言和布局方向独立覆盖，以及 retained 富内容。

#### Scenario: 真实操作
- **WHEN** 运行公开示例并编辑内容、打开弹层、切换语言
- **THEN** 中英文可见状态正确，应用不依赖内部 host API，富内容中的真实输入与 slot 身份保留

### Requirement: 后台原生自动验证
Gallery SHALL 提供有界隐藏窗口输入与局部 GPU readback 序列，分别记录实际 Windows/D3D12 与 Linux/private Wayland/Vulkan 结果，CPU 合同不能替代 GPU 证据。

#### Scenario: 隐藏捕获
- **WHEN** 自动验收切换两种语言、恢复当前页面并捕获局部图像
- **THEN** 不显示桌面窗口、不抢焦点、不弹崩溃框，原始输出只写 ignored out，结果明确实际平台、样例和未验证边界
