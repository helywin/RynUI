# TreeSelect

TreeSelect 用于紧凑字段中的单值层级选择，组合 retained 原生 Input、Popover 和 Tree。公开头文件为 `<ryn/tree_select.hpp>`，也包含于 `<ryn/rynui.hpp>`。097 首版已完成平台通用验收与独立 Astra R4 复核，见 [最终记录](../openspec/changes/097-20261028-add-native-tree-select/evidence/06-final-review-and-closure.md)。

公开 API 以 `TreeSelectProps` 和固定 `TreeSelectItem` 描述数据。key 是唯一非空 String，空 value 表示未选，可直接接入 `FormField<String>`。标题、disabled、visible 使用 reactive Prop；最多 1024 个节点、64 层。

```cpp
ryn::TreeSelect(
    ryn::TreeSelectProps{}
        .placeholder(u8"选择项目")
        .allowClear()
        .treeDefaultExpandedKeys({u8"project"})
        .layout(ryn::LayoutStyle{}.width(ryn::dp(280))),
    {ryn::TreeSelectItem{u8"project", u8"项目",
                        {ryn::TreeSelectItem{u8"src", u8"源码"},
                         ryn::TreeSelectItem{u8"docs", u8"文档"}}}});
```

选择与展开分离：点击 switcher 只展开，激活行提交其 key；再次选择当前值请求关闭而不清空，也不重复触发 onChange。未知外部 value 保留并显示原 key。标题更新会刷新接受值的显示；被隐藏或禁用的已接受值保持，不自动改写受控状态。禁用父节点不连带禁用独立可用的子节点；隐藏父节点会隐藏其整条分支。

`value` 与 `defaultValue`、`open` 与 `defaultOpen`、`treeExpandedKeys` 与 `treeDefaultExpandedKeys` 各自互斥。受控父级拒绝用户意图时仍保留接受值／open，下一次意图可重试。同一次外部点击引发的焦点退出与弹层关闭只交付一次请求；程序化焦点退出立即交付，Tab 离开也只交付一次。选择合法 key 后请求关闭，清空通过 onChange 交付空 String。`onTreeExpand` 的 TreeExpandInfo 提供实际 key、展开集合与动作方向。

指针点击 selector 打开；键盘 Down／Up／Enter／Space 打开并将焦点交给真实 Tree 行。方向键、Home／End、Enter／Space 使用 Tree 的原生导航、展开和激活。键盘打开或进入已打开的弹层会请求焦点转移帧；受控关闭、拒绝、强制抑制或回调失败会终止本轮待聚焦意图，后续程序化打开不会沿用旧意图。键盘入口跳过禁用／隐藏行；已选项不可操作时回退到可用行，全部不可用时保留 selector 焦点。Escape 关闭子弹层并恢复 selector；Tab／Shift+Tab 按 selector 所在顺序离开到相邻控件。`onFocus/onBlur` 对应 selector 的原生 Input 焦点，因此转入树行也可能触发 selector blur。

`readOnly` 保持 selector 可聚焦但禁止打开、选择和清空；disabled、visible=false、失活、外部输入边界或零面积会取消操作并关闭实际弹层。恢复不自动复活已抑制的弹层，需新的用户打开动作或受控 open 的显式 false→true。祖先弹层构建过程的临时封锁与已提交关闭状态分开处理。

`listHeight` 是有限列表高度，必须正且有限，默认 256 logical px；实际 popup 受窗口限制并匹配 selector 宽度，Tree 自行滚动。LayoutStyle 仅控制外部 selector，不是视觉样式入口。

## Form 与 refs

使用 `FormField<String>` 的 value()/onChange()，在 FormItem 中显式 `.control(ref)`，再把同一 TreeSelectRef 传入 TreeSelect。readOnly()/disabled()/status() 可直接绑定。reset 恢复接受值与显示，保持原 selector/editor/Tree 身份；同值 reset 不产生选择事件。selector 是只读显示入口，不提供自由输入／搜索，也不会建立 IME composition session。

TreeSelectRef 复用 InputRef，支持绑定检查、focus/blur 及只读显示的文本选择；不是 Tree 行 ref。普通更新不重新执行组件组合。回调可销毁 owner；旧完整 generation 停止工作，替代 ref generation 保留。首次 Prop getter 或组合挂载异常可在 Content 内局部捕获，原 owner 会静默退役并保留原始异常。

## Theme

`ThemeConfig::tree_select` 独立于 Select 和 Tree。`.tokens.selector` 控制 selector 背景、hover/active border、active outline，以及 popup 背景、padding、radius、gap、shadow、order；`.tokens.tree` 控制节点内容和适用装饰。selector 的三种 ControlSize／InputVariant／InputStatus 继续复用 Input 原生视觉与字体，树和空状态文字使用树节点字体。

```cpp
ryn::ThemeConfig config;
config.tree_select.tokens.selector.popup_padding = ryn::dp(6);
config.tree_select.tokens.tree.node_selected_background = ryn::Color::rgba8(21, 76, 123);
config.tree_select.tokens.tree.indent_size = ryn::dp(20);
```

inherit、局部 seed/algorithm、override、序列化/hash、dirty 和后代预发布校验均属于 Theme 合同。未启用的局部 seed 不改变继承。最终节点字号／图标字号量化必须在可渲染范围内；失败更新保留原 snapshot，不能靠先发布再在 Text 中抛错。

原生首版不包含多选／checkbox、搜索、异步、动态拓扑、富节点／popupRender、虚拟化、动画、RTL/Web/AT。首版上限是主动约束，超限配置会拒绝，不静默截断。交互 Gallery 由 098 独立接入与验收。

平台验收与准确完成状态见 `openspec/changes/097-20261028-add-native-tree-select/tasks.md`。MSVC HEADLESS／Recording 证据不代表实际 GPU／原生窗口、Linux 或系统 IME／AT 通过。

## 交互 Gallery

Gallery 目录中的 TreeSelect 跳到“层级目标”。这里可选择父／叶节点、更新指南标题、启用归档、清空目标及编辑保存备注；接受／拒绝选择和关闭的两个开关独立控制父级回写。禁用／只读／显隐及恢复按钮位于选择器外部，恢复只恢复可操作性，不擅自重新打开被抑制的弹层。

下方“交付目标”使用真实 Form String 字段与必填反馈。提交反馈读取接受值，“聚焦后重置目标”先通过真实 ref 聚焦 selector 再 reset，可重复执行同值重置。该 selector 是只读显示，不启动编辑／IME session。098 已通过新构建 Recording、三主题双宽、精确 inventory、八项 contracts 与独立 Astra R1，见 [最终记录](../openspec/changes/098-20261028-add-tree-select-gallery/evidence/03-final-review-and-closure.md)。完整 Gallery、原生窗口／GPU、Linux、系统 IME／AT 未在本轮验收。
