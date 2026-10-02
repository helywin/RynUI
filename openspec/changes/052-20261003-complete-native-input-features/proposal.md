# Proposal

## Why

已有 Input、Password、Search 共用单行编辑器，但公开 API 和原生外观仍缺少 Ant Design 6 的变体、统计、操作配置和焦点方法。用户已要求补齐已有组件的原生桌面能力，本 change 先完成单行输入家族的共用合同，为后续 TextArea、OTP 和双向文本编辑提供基础。

## What Changes

- 增加 reactive Outlined、Borderless、Filled、Underlined 变体，覆盖状态、禁用、焦点可见性、Theme、SpaceCompact 与 Search 连接外观。
- 增加字数显示、Unicode scalar/grapheme 或自定义计数、软上限和自定义超限裁剪；用户编辑的裁剪属于同一历史事务，IME preedit 不裁剪，受控回写不被软上限修改。
- 增加 InputRef 的 focus、blur、select、bound，焦点光标选项、autoFocus、onFocus/onBlur，以及 typed 原生输入用途、大小写和自动纠正提示。
- 完成清空操作的独立禁用、自定义 IconSource 和 onClear；Password 转发共用属性及 prefix/suffix，支持 reactive 显隐开关、键盘停靠策略、自定义显隐图标及 hover 操作；Search 转发共用属性、支持自定义搜索图标和 Clear 回调来源。
- 保留现有 API 的默认行为和组件/editor/scene 身份，补充 Gallery、文档、平台通用测试和分平台真实窗口证据。

## Capabilities

### New Capabilities

- `native-single-line-input`: 单行输入家族的原生变体、统计、焦点、系统提示和操作行为。

### Modified Capabilities

无；当前主规格目录没有已同步 capability。实现须保留 026 输入编辑合同、050 Compact 合同与 051 Icon 合同。

## Impact

- 影响 include/ryn 的输入家族 API、component/input/search/affix action、输入事务、Input Token 与 Gallery。
- 只更新 logical CPU scene/stores，不增加 renderer 私有上传路径，不引入新依赖。
- 风险在于 formatter 重入或卸载、受控值回写、组合输入与动作焦点、变体切换时的裁剪和 Compact 接缝；均须有独立合同测试。
- 本 change 不移植 DOM、CSS、React、HTML 表单/自动填充 API；原生 TextArea、OTP、RTL/混合文字视觉导航仍在用户总收尾范围内，分别继续开发，不能据本 change 把整个 Input 标为完成。
- 平台通用验收在 Windows MSVC headless 完成一次；Windows 与 Linux 的系统输入、字体、DPI 和 GPU 证据独立记录。规划不代表实现或平台通过。

参考基线：2026-10-03 核对 [Ant Design 6.6.5 Input API](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/input/index.en-US.md)、[variants](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/input/style/variants.ts)、[Password](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/input/Password.tsx)、[Search](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/input/Search.tsx)。
