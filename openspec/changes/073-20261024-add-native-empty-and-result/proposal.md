# Proposal

## Why

RynUI 已有 Button、Input、Icon 与 retained 容器，但缺少可直接承载真实操作的空状态和结果页。新增 Empty/Result，补齐静态反馈展示，并保持普通属性更新时的编辑状态与 slot 身份。

## What Changes

- 新增 Empty 的默认/紧凑图示、可关闭图示、reactive description、typed image/extra slots。
- 新增 Result 的 success/info/warning/error 与 403/404/500，reactive status/title/subtitle，以及 typed icon/content/extra slots；复用 Status，不复制状态基础设施。
- 补齐 Theme 默认/暗色/紧凑、继承、override、identity、dirty phase 与五份诊断 golden。
- 在有限、短和零约束内居中、换行并裁剪实际内容；slot 中允许原生 Button/Input，普通更新不重新执行 slot。
- 首版只做静态呈现，不引入动画、服务、网络、栅格素材、动态 slot 拓扑或 Web aliases。内置 exception 用现有 Icon 表达，不复制上游大型插画。

## Capabilities

### New Capabilities

- `native-empty-result`: 原生空状态与结果展示的公开 API、retained 布局和 Theme 合同。

### Modified Capabilities

无。

## Impact

仅修改本组件公开头文件、组件实现、WindowComponentServices 接入、Theme、CMake、相关测试、独立组件说明与本 change。遵循本地锁定 Ant Design 6.6.5 源码证据，不改 Gallery、support-overlay、README 或 component-completion。风险集中在文本换行、内容裁剪与主题派生，使用 MSVC headless Debug/Release 和实际 logical scene 几何验证；不声称 GPU、Linux 或系统 IME 验收。
