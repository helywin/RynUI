# Proposal

## Why

用户要求优先补齐常用业务组件并持续实施。Select 与 Card 已可用，接下来需要切换页面分区，同时保留每个页面的表单草稿和原生控件状态。

## What Changes

- 新增原生 Tabs、typed Props/TabItem/TabContent，支持受控/非受控 activeKey、响应式 label/disabled、onChange/onTabClick。
- 首版提供 line/card、三尺寸、top/bottom、centered；键盘与pointer导航、单一标签组Tab入口、跳过disabled、横向溢出箭头/滚轮及活动项自动显露。
- TabItem集合在挂载时固定，所有面板一次预建；切换仅改变可见分支，隐藏面板不可命中/聚焦，保留编辑值及owner identity，并清理旧输入会话和嵌套窗口层。
- 新增Tabs Component Token及精确失效、真实Gallery与平台通用/Windows/Linux独立验收。
- 首版不含editable-card/动态增删重排、vertical/RTL、富标签/额外栏、overflow popup、自定义indicator、动画、lazy/destroyOnHidden与Web/DOM API。这些不阻塞常见静态业务页面。

## Capabilities

### New Capabilities

- `tabbed-content`：原生页面分区、标签导航和保留面板状态的内容切换。

### Modified Capabilities

无；主规格目录为空。

## Impact

影响公开headers、Theme/runtime、WindowComponentServices、共同logical scene、Gallery及测试文档，不新增依赖或GPU ABI。主要风险为受控拒绝时焦点与选中状态区分、隐藏面板的输入/弹层清理、溢出命中与生命周期重入；测试覆盖这些实际路径。Windows与平台通用先验收，Linux须独立实际native证据。
