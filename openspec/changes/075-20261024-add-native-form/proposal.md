# Proposal

## Why

RynUI 已有 Input、Select、Checkbox 和 InputNumber，但尚无把真实编辑、校验、提交及重置连接起来的原生表单。首版提供小而完整的 typed 表单闭环，避免业务逐页重复注册与错误管理。

## What Changes

- 新增 `Form`、`FormItem`、typed Props/slots、`FormField<T>` 与 `FormRef`；显式绑定 String、可空 double、bool 和既有 SelectSelection。
- 支持字段初值、保留值身份、受控值/回调、同步 required/custom 校验、提交成功/失败、程序及原生 Button 提交/重置。
- 提供纵向标签、必填标记、帮助及错误，通过 Theme/Component Token 控制视觉；错误更新保留控件与输入草稿。
- 使用真实控件 ref 聚焦第一个具备资格的无效字段，遵守 Modal、Spin、隐藏分支及销毁边界。
- 首版不做任意 slot 自动采值、JSON schema、异步校验、深路径、Form.List、数组、横向/inline 布局、自动 Enter 提交、自动滚动、DOM/CSS 或完整上游 API。

## Capabilities

### New Capabilities

- `native-form`: 原生 typed 字段注册、验证、提交重置与 retained 表单呈现。

### Modified Capabilities

无。当前主 specs 清单为空。

## Impact

新增 Form public/model/host/tests、`docs/form.md` 和 075；只做必要的 Theme、window host、facade 与 CMake 注册。不修改 Gallery、全局 README、support overlay、wheel routing，不增加依赖。平台通用验收使用独立 `windows-msvc-headless` MSVC Ninja Multi-Config Debug/Release 构建，最多 4 jobs；不以此宣称 native GPU、Linux 或系统 IME 已验收。风险集中于用户回调同步销毁/重入、受控值未回写及隐藏字段资格，通过 transaction revision 和真实控件集成测试验证。
