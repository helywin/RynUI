# Proposal

## Why

Gallery 的通用和布局分类仍包含仅有目录信息的组件，无法用于构建完整原生界面。按用户指定顺序先补全通用，再实现布局，并让各独立页面使用真实组件展示支持范围。

## What Changes

- 保留 Button、Icon、Divider、Flex、Space 已有原生合同；补充其 Gallery 覆盖，不重复实现。
- 实现 FloatButton、Group、BackTop 的原生交互、主题与生命周期。
- 为 Typography 增加 typed 富文本内容和 HTML/纯文本剪贴板发布。
- 实现 Grid Row/Col、Layout Header/Sider/Content/Footer、Masonry 和 Splitter。
- 添加针对性自动合同测试，更新支持目录、组件文档与独立页面。

## Capabilities

### New Capabilities

- `float-button`: 原生浮动操作、分组菜单与返回顶部。
- `typography-rich-content`: 带语义样式的行内内容和富文本复制。
- `grid-layout`: 响应式 24 列栅格。
- `shell-layout`: 可嵌套页面结构与可折叠侧栏。
- `masonry-layout`: 保持组件身份的变高多列排列。
- `splitter-layout`: 可交互调整与折叠的面板。

### Modified Capabilities

无；当前主 spec 库为空，已有组件合同继续由历史 change 和代码测试维护。

## Impact

影响公开 typed API、Core 布局/文本/输入、Theme、SDL 剪贴板适配及 Gallery；不引入第三方依赖，不改变 renderer ABI。DOM、CSS、React、URL 导航等 Web 平台专属接口不在原生合同内。主要风险是富文本换行/双向文本、响应式重排、分发期间销毁和捕获清理，须以自动测试覆盖。
