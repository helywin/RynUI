# AutoComplete

原生自由 String 输入与建议选择。参考锁定 Ant Design 6.6.5 AutoComplete（HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`）；不依赖 React 或 DOM。

`AutoCompleteProps` 提供 `value(Prop<String>)` / `defaultValue(String)`、`options(Prop<vector<AutoCompleteOption>>)`、`placeholder`、`filterOption`、`defaultActiveFirstOption`、`backfill`、`allowClear`、`disabled`、`readOnly`、`size`、`variant`、`status`、`open` / `defaultOpen`、`listHeight`、Input `ref` 和外部 `layout`。受控与对应 default 不得同时设置。`listHeight` 必须为有限正数。

每个 `AutoCompleteOption` 有唯一非空 `key`、输入接受的 `value`、显示的 `label`、`disabled` 与可选 `AutoCompleteRichLabel{AutoCompleteOptionLabel{...}}`。相同 key 的 row 保留；普通 String label 更新不重建子树。rich label 复制保持身份，只有显式换用另一 rich label 对象才替换该局部子树。rich 内容可绑定 reactive 数据。列表更新不重建真实 Input/editor。

默认不过滤（锁定上游 tests 的 combobox 行为）；`AutoCompleteFilter{true}` 开启大小写敏感 UTF-8 value 子串过滤，也可使用 `AutoCompleteFilter{[](StringView query, const AutoCompleteOption& option) -> bool {...}}`。predicate 应为无副作用纯函数，不在计算中修改组件树。

输入提交产生 `onChange(String)` 和 `onSearch(String)`。选择先对变化值发 `onChange`，再发 `onSelect(String, AutoCompleteOption)`，不发 `onSearch`。受控值由外部接受；`onSearch` 只用于建议查询，不能代替 onChange 控制值。程序 value 更新不发用户事件；preedit 本身不发搜索。清空发变化的 onChange、onSearch 和 onClear。

上下键在建议中移动并跳过 disabled 项；Enter 选择，Escape 取消临时 backfill 并请求关闭，Tab 请求关闭并按正常焦点顺序离开。backfill 不产生值事件。组合输入活动时这些建议快捷键交还编辑器。prefix/suffix 是 typed 装饰 slots，不增加 Tab 入口。rich label 中的子 popup 按最近窗口层拥有 wheel/Escape。

`open` 是外部权威，`onOpenChange` 只是意图；空过滤结果不显示 popup。输入边界隐藏、Modal/Spin 阻断和窗口失活会 suppress popup 并清理临时状态，恢复后需要显式打开。首版有限高度滚动，无虚拟化。

Theme 通过 `ThemeConfig::auto_complete` 独立设置。`AutoCompleteThemeToken` 与 `AutoCompleteTokenOverride` 复用 Select 的视觉字段：popup/option/selector/clear 颜色、option_height/padding/radius/gap、typography、shadow 和 z_index_popup。支持继承、独立 component algorithm 和全局 dark/compact；Input 自有尺寸/状态/variant 继续使用其 Theme。视觉只来自 Theme，LayoutStyle 仅控制外部布局。最终 popup font 按实际 round 后检查 FontRuntime 1..INT_MAX/64 范围，非法后代 preflight 不发布部分 Theme。

本阶段先交付模型与完整 Theme 合同；原生组件和可编译 Form 示例在后续实施阶段验证后补齐。首版排除网络异步管理、选项分组、虚拟化、动画、RTL、Web、自定义编辑器替换与任意 popupRender。headless 合成事件不能替代 native GPU、Linux、系统 IME 或 AT 验收。
