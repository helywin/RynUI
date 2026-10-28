# TreeSelect

TreeSelect 的目标是紧凑字段中的单值层级选择。当前正在实现首版，尚未完成原生组合与整体验收，不能作为已交付组件使用。

公开 API 以 `TreeSelectProps` 和固定 `TreeSelectItem` 描述数据。key 是唯一非空 String，空 value 表示未选，可直接接入 `FormField<String>`。标题、disabled、visible 使用 reactive Prop；最多 1024 个节点、64 层。

选择与展开分离：点击 switcher 只展开，激活行提交其 key；再次选择当前值请求关闭而不清空。未知外部 value 保留并显示原 key。被隐藏或禁用的已接受值保持，不自动改写受控状态。

首版计划复用 retained Input、Popover 和 Tree，提供 clear、受控 value/open/展开、键盘、有限列表、Form 和独立 Theme。多选、搜索、异步、动态拓扑、富节点、虚拟化、动画、RTL/Web/AT 以及 Gallery 不在此范围。

平台验收与准确完成状态见 `openspec/changes/097-20261028-add-native-tree-select/tasks.md`；不得把模型测试或规划状态描述为完整组件实现。
