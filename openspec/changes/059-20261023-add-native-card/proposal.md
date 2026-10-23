# Proposal

## Why

用户要求优先补齐常用组件并持续实施。Select 已可用于表单，业务页面还需要统一的标题、正文和操作内容容器；先完成 Card，随后接入同组优先级的 Tabs。

## What Changes

- 新增原生 `Card`，typed Props 与 body/title/extra/cover/actions slots；提供默认/小号、outlined/borderless、hoverable 和 reactive loading。
- Card Token 贯穿主题算法、继承、覆盖、dirty identity、hash/序列化；不开放 Props 视觉样式或 renderer 私有接口。
- 各 slot 一次挂载，loading 切换预建占位与正文可见性，普通更新不重建用户子树；保留子控件真实输入、焦点和回调。
- 加入真实 Gallery 样例、公共合同和独立 Windows/Linux 原生验收。
- 首版不含 Card.Grid/Card.Meta、内置 Tabs/tabList、远程图片加载、骨架动画与 Web/DOM API；cover 是可组合的原生 slot。Tabs 在后续独立 change 实现。

## Capabilities

### New Capabilities

- `card-container`：可主题化的原生分区内容容器、保留子树、loading 与 hover 行为。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

影响公开 header、Theme/runtime、WindowComponentServices、logical CPU scene、Gallery、测试和文档；不新增依赖或修改 GPU ABI。风险主要是有限宽度 header 分配、loading 隐藏正文的焦点清理、hover 与子控件命中关系；验收覆盖这些边界。平台通用合同在 Windows MSVC headless 完成一次，原生平台分别记录。
