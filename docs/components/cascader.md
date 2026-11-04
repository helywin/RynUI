# Cascader

`<ryn/cascader.hpp>` 提供 `Cascader(CascaderProps)` 和嵌入式 `CascaderPanel(CascaderProps)`。设计合同锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 API、类型、样例及 Cascader/Select 样式配方。

`CascaderValue` 是 `variant<String, double>`；`CascaderPath` 表示从根到目标的值路径，单选和多选统一使用 `CascaderPaths`。单选至多一条路径。`value/defaultValue`、`open/defaultOpen`、`expandedPath/defaultExpandedPath`、`searchValue/defaultSearchValue` 各自互斥；受控更新只发送请求，应用接受后改变显示。不存在于当前选项的合法 value 保留并显示原始路径；空路径、重复路径和非有限 number 会被拒绝。

`options` 接受响应式 `Prop<vector<CascaderOption>>`。key 必须全局唯一且非空，同级 value 唯一，number 有限，嵌套不超过 32 层。选项按 key 保留身份；同 key、同 `content_revision` 的富 slot 工厂只执行一次，即使应用传入了另一份工厂。通过 slot 内 Prop 更新内容；需要替换工厂时递增 `content_revision`。重排、路径和标签更新保留未变内容及 Input 草稿。

`multiple`、`changeOnSelect`、`showCheckedStrategy(ShowParent|ShowChild)`、`expandTrigger(Click|Hover)` 控制选择和展开。disabled 禁止选择与展开；`disable_checkbox` 只阻止选择，仍允许展开。禁用后代不受父项选择请求影响。`readOnly`、`visible`、输入阻塞、零面积和窗口失活会停止输入并取消加载；受控值仍由应用决定。

多选使用真实保留 tags，单项移除不改变面板开合。`tagRender(CascaderTagInfo)` 返回 typed `CascaderTagContent`；info 提供响应式 label/disabled、路径和带生命周期保护的 `onClose`。`maxTagCount` 接受数量或 `CascaderTagCount::Responsive`；溢出按钮打开可操作的遗漏项浮层。`maxTagTextLength` 按完整 Unicode grapheme 截断，`maxTagPlaceholder` 接受遗漏路径列表并返回摘要文字。`displayRender` 为单选显示或每个多选 tag 生成文字；多选时每次接收该 tag 的一条路径。

`showSearch` 使 selector 可编辑，并提供与其共享受控查询的独立搜索 Input；两者各自保留 Core preedit/session。不开启搜索时 selector 保持只读。默认搜索复用 Core 的 Unicode 规范化和完整 case folding；默认路径文字保留原文，并高亮匹配的 grapheme（包括 `ß` 的展开映射）。`search(CascaderSearch)` 提供 filter、稳定 sort、render、limit（0 不限）、`auto_clear_search_value` 和 `match_input_width`。自定义 render 返回的文字直接显示。富选项 slot 在搜索期间隐藏，恢复后保留草稿；搜索结果不触发懒加载。搜索头、底部 popup 内容及每列视口分别布局，行的绘制和命中均受列视口裁剪。

方向键、Home/End、PageUp/PageDown 支持行和列导航，RTL 交换横向方向。焦点进入保留的富子控件时自动显露对应行。精确滚轮保留小数位，在可滚动边缘交回上层；子浮层拥有自己的输入。IME preedit 由 Core Input 会话处理，Escape 先取消 composition，再关闭最内层浮层；Tab/ShiftTab 离开整个 Cascader 时关闭其面板。

`loadData` 是显式应用服务，不访问网络。`is_leaf=false` 且无 children 的项展开时收到 `CascaderLoadRequest`，含路径、线程安全取消标志和 `complete`。应用必须把完成派发回挂载线程；外线程立即返回 false，不读取可变 UI 状态。完成只能使用一次。新加载、搜索、选项替换、关闭、禁用、隐藏、卸载和窗口销毁会使旧 generation 失效。加载及错误显示在与选项视口分离的状态区，使用真实 Retry 按钮或重新激活该项重试。合法完成更新内部选项快照；之后新的 `options` Prop 更新仍有权替换它。

`content`、`popupContent`、`triggerContent`、`emptyContent`、`prefix` 和 `tagRender` 都是保留的 typed slots；图标使用 typed `IconSource`，包含 `showSearch` 的 `searchIcon`；弹层打开时 selector 与搜索头均使用搜索图标。`CascaderRef` 提供 `focus/blur/bound`，适用于默认和自定义触发器。事件包括 `onChange/onOpenChange/onExpand/onSearch/onClear/onFocus/onBlur`；`onChange` 可接收 value，或同时接收每条路径的 typed selected-option ancestry。应用把自定义数据字段映射到 `CascaderOption`，不依赖运行期字段名字符串。`size/status/variant/placement/direction` 为 typed Prop，`layout` 仅控制外部布局。

`ThemeConfig::cascader` 提供独立 Component Token、组件 seed/algorithm 和作用域 inherit；palette、metrics、typography、effects 分别订阅和失效。token 实际驱动 selector、列/选项/选中态、箭头/清除、tags、搜索高亮、空状态、焦点和 popup 阴影；不重建无关工厂。完整 snapshot/identity 和五种算法 golden 保留其他组件全部字段。

非法拓扑和值在发布前验证。新富内容工厂、渲染回调或事件回调抛错时，损坏的 Cascader 会完整退场并释放输入、场景和订阅；其他已挂载组件继续工作。渲染/slot 工厂内同步重入更新并覆盖当前快照时采用完整退场；排队到当前事务结束后的更新按下一次完整事务处理，避免保留半创建内容。普通事件回调中接受受控值、更新选项或销毁宿主受到支持。应用应在工厂外更新响应式状态。

Gallery 独立页展示单选、Panel、多选 tags、responsive 溢出、自定义保留标签、独立 Theme、长列表搜索/滚动、本地 loader、带 Input 的子浮层、自定义触发器和空状态。隔离分支验证见 change107 的 `evidence/cascader-linux.md`，当前 main 的组合验证与平台边界见 `evidence/cascader-integration.md`。DOM/CSS/React、浏览器 URL/portal 不属于原生 API；系统 IME 候选窗口和 Windows/MSVC 实测独立于 Core 会话合同。
