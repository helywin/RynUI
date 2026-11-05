# Tree 与 DirectoryTree

`<ryn/tree.hpp>` 提供 retained 原生树。旧 `Tree(TreeProps, std::vector<TreeItem>)` 调用兼容；响应数据使用 `.items(Prop<std::vector<TreeItem>>)` 和 `Tree(props)`。key 为唯一非空 String；整个更新先验证，再发布。上限为 100,000 节点、64 层，拒绝重复 key、非法高度和超限数据，不截断。

```cpp
#include <ryn/tree.hpp>

void project_tree() {
    ryn::Tree(ryn::TreeProps{}.checkable().defaultExpandedKeys({u8"project"})
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(320)).height(ryn::dp(200))),
              {ryn::TreeItem{u8"project", u8"Project",
                             {ryn::TreeItem{u8"src", u8"Sources"}, ryn::TreeItem{u8"docs", u8"Docs"}}}});
}
```

普通 Prop 更新不重新执行应用 Component。稳定 key 支持新增、删除、重排和跨父移动；存活行、richTitle(TreeTitle) 和 icon(TreeIcon) 保留身份。相同 key/`contentRevision` 保留已挂载工厂及编辑草稿；递增版本或切换 rich/plain、custom/default icon 才替换工厂。替换前取消旧输入、捕获、IME 与后代浮层。失败工厂使本组件干净退役，其他组件继续工作。不要依赖为相同 key/版本传入另一个闭包来替换已挂载内容。

节点标题、visible、disabled、selectable、checkable、disableCheckbox、isLeaf 可响应更新。disabled 节点隔离 checkbox 传播，但其独立可用子节点仍可操作；disableCheckbox 和 checkable=false 同样构成 checkbox 隔离边界。隐藏祖先隐藏整条分支。全树 disabled/visible=false、零面积、窗口失活或输入边界封锁取消输入和捕获。

expandedKeys、selectedKeys、checkedKeys 与相应 default 属性互斥。受控状态只发送候选，应用回写才接受；无关 key 被规范化。非受控数据删除会移除失效选择、勾选和展开，受控输入仍由应用拥有。`autoExpandParent` 展开已展开 key 的祖先；`defaultExpandAll` 只影响初始数据。初始没有自动展开祖先的旧调用保持旧行为。

`multiple` 提供指针 Ctrl/Meta 增减、Shift 可见范围，以及 Shift+方向键范围选择；默认单选。Enter 选择，Space 在 checkable 时勾选。`checkStrictly` 关闭级联；`halfCheckedKeys` 为显式半选权威。onSelect/onCheck/onExpand 返回动作 key、候选集合及动作方向，onCheck 同时交付 checked/halfChecked。行有单一 Tab 入口；Up/Down、Home/End、左右键导航、展开/折叠；富 child 保留自己的编辑、Tab 和子浮层合同。

`searchValue` 过滤并显露匹配节点的祖先，默认按标题包含匹配；`filterTreeNode(key,title)` 自定义匹配，并使用 Theme 的节点强调色。搜索不改写接受的 expandedKeys。`showLine` 使用 logical scene 的真实线段，switcher、checkbox、DirectoryTree 文件/目录均使用向量 Icon；`showIcon`、`blockNode` 和 `rtl` 控制相应布局。视觉复用独立 `ThemeConfig.tree.tokens`；LayoutStyle 仅控制外部布局。

## 虚拟行与 refs

```cpp
void virtual_tree(std::vector<ryn::TreeItem> data, ryn::TreeRef ref) {
    ryn::Tree(ryn::TreeProps{}.items(std::move(data)).virtualized().rowHeight(24).ref(ref)
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(320)).height(ryn::dp(240))));
}
```

`virtualized` 默认关闭，保留旧自然尺寸行为。启用后真实销毁视口及 overscan 以外的普通行；不是挂载全部行后只裁剪画面。`rowHeight` 默认 24；`itemHeight(key)` 可指定每行高度，均须有限且在 [1,4096]。扩展 flatten 与高度前缀由数据状态生成；滚动定位使用高度前缀。键盘导航和 `TreeRef::scrollTo({key,offset})` 显露目标。折叠隐藏的 key 不可 scrollTo；应用先展开其祖先。

富标题/自定义图标的草稿缓存按已访问 key 增长，不宣称恒定空间；普通 DirectoryTree 内置图标不进入富缓存。离窗富行先封锁并结束焦点、捕获、IME 和子浮层，保留草稿；删除 key 或替换版本释放缓存。`TreeRef::bound()` 与 generation 清理可检测宿主有效性；`reset()` 取消所有加载，不重挂载存活编辑器。

## 异步与拖放

`loadData(TreeLoader)` 使用 `AsyncOptionsSource<TreeItem>`。request.query 是节点 key，generation 是本次节点 ticket，cancelled 为共享取消标记；结果 options 是该节点的新 children，error 非空代表失败。应用提供 transport 和 `OwnerThreadPost`，Core 不访问网络。post 必须在挂载线程执行；错误线程的结果被拒绝。每节点独立请求，展开 B 不取消 A；同节点加载中不重复请求。重复完成、取消、重置、删除和卸载的旧结果无效。折叠保留该节点正在执行的请求，隐藏/禁用取消；失败 switcher 使用 retry 图标，重试重新发起请求。

`loadedKeys` 可受控，`defaultLoadedKeys` 初始化非受控状态；onLoad 返回 key、候选 loaded_keys 和 error。加载结果也经过全树验证，不能引入重复 key。应用下一次主动替换 items 为新的权威数据时，以应用数据为准。

`draggable` 提供窗口内真实 PointerRouter capture 拖放。`allowDrag(key)`、`allowDrop(TreeDragInfo)` 可拒绝动作；内置拒绝自身/后代循环和 disabled 目标。上下四分之一区域分别为 Before/After，中间为 Inside。onDragStart/Enter/Over/Leave/End 和 onDrop 交付源 key、目标 key、子树 keys、position、allowed；drop indicator 使用 logical scene。边缘停留使用单调时钟按 16 ms tick 自动推进滚动；拖放由可见 Tree 根持有 capture，虚拟源行离窗可正常释放。取消/隐藏/数据更新释放 capture 和滚动 deadline。组件不自动改变业务拓扑，应用在 onDrop 后回写 items；不依赖 DataTransfer 或 OS 外部 DND。

`DirectoryTree(props, items)` 组合 Tree 并使用文件/文件夹图标；`props.directory(DirectoryExpandAction::Click|DoubleClick|None)` 可选择展开动作。普通点击保持目录项选中，multiple 配合 Ctrl/Meta/Shift。展开和选择使用有限主题 motion；reduced motion、隐藏、禁用、失活、零面积和销毁停止动画，idle 不请求连续帧。

Gallery 的 Tree 独立页保留项目目录/富备注的原有行为，并展示动态反转、真实拖放回写、多选/勾选反馈、过滤、RTL DirectoryTree、10,000 行 scrollTo 和明确标注的合成异步源。平台结果与边界见 [107 Tree 证据](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/tree-linux.md)和[主线 Form 组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/tree-treeselect-integration.md)。DOM/CSS/React、浏览器 DND、系统 IME 候选 UI 和辅助技术平台桥不由 HEADLESS 结果证明。
