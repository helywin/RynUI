# Select

058 提供原生单选 Select，107 在兼容旧单选 API 的基础上补齐原生多选与自定义内容合同。Select 复用真实 Input、IME、窗口层与 retained scene。平台通用、Gallery 和 Windows/Linux 原生验收分别记录在 change 的 tasks/evidence；未通过的平台项保持待验收。

```cpp
ryn::Signal<ryn::SelectSelection> role{ryn::SelectSelection{1}};
ryn::SelectRef role_ref;
// 在已有窗口的 Content 中挂载，窗口需要绑定 InputComponentHost。
ryn::Select(ryn::SelectProps{}
    .options({{1, u8"管理员"}, {2, u8"成员"}, {3, u8"访客", true}})
    .value(role).showSearch().allowClear().placeholder(u8"请选择角色")
    .notFoundContent(u8"没有匹配项").ref(role_ref)
    .onChange([role](ryn::SelectSelection value) mutable { role.set(std::move(value)); })
    .layout(ryn::LayoutStyle{}.width(ryn::dp(240))));
```

原有单选、搜索、清空、动态 options、loading/disabled、键盘导航、三尺寸、四变体和 status 均保留。新增 Multiple/Tags、分组、保留渲染、窗口列表、可取消应用数据源、交互式前后缀、RTL 与可选有限动画。

`SelectValue` 保留 `String` 或有符号64位整数的类型，字符串 `"1"` 与整数 `1` 是不同值。`SelectOption` 包含 value、label、disabled，重复 typed value 会在发布新 options 前整体拒绝。

`value` 绑定 `Prop<SelectSelection>`，其中 `SelectSelection = std::optional<SelectValue>`；空值表示没有选择。受控模式通过 onChange 请求值，父级不回写时保留原显示。未知值保留原值文本，后续 options 到达后显示对应 label。defaultValue 用于非受控模式。

搜索仅过滤 label，不提交选择。使用 Unicode 规范化和完整大小写折叠，支持中文、变长折叠（如 Straße/STRASSE）及组合字符；不移除重音，也不转换简繁。沿用仓库锁定的 utf8proc 2.11.3，由现有内部 text_boundary 隔离第三方 API。

Select Component Token 通过 `ThemeConfig::select` 配置，覆盖菜单颜色、行高/间距/圆角、字体/选中字重、shadow 和 z-index，以及 selector/clear 背景、hover/active border 与 active outline。配方依据 Ant Design 6.6.5；尺寸、变体、status 的选择器仍复用 Input 的配方。视觉 Token 不进入 LayoutStyle。

`size` 使用 ControlSize 的 Small/Middle/Large，`variant` 使用 InputVariant 的 Outlined/Filled/Borderless/Underlined，`status` 使用 InputStatus 的 Default/Warning/Error。disabled 关闭菜单并清理焦点/输入会话；loading 保留搜索能力但阻止候选提交和清空，默认 suffix 显示 loading 图标。自定义 suffix 会替代默认图标。

鼠标点击选择器打开菜单，点击候选提交选择；ArrowUp/Down、Home/End 跳过禁用项并把活动项滚入视口，Enter 确认，Escape/Tab/外部点击请求关闭。IME 组合期间这些键交给输入法；第一次 Escape 取消组合，后续 Escape 再关闭菜单。列表支持滚轮，边缘空间不足时向上翻转并限制在窗口范围。

`open`/`onOpenChange` 使用受控请求语义，父级不回写时保留外部打开值；失焦、禁用和窗口失活会另外隐藏菜单以完成清理。defaultOpen 用于非受控模式。打开搜索时显示 query，选中 label 作为搜索 placeholder；关闭会丢弃 query 并恢复选中 label。`SelectRef` 复用 InputRef 的 focus/blur/搜索选区。

prefix/suffix 为 typed slots，可放原生按钮和编辑器；主 Input 在 affix 子控件之前独立捕获，不会因额外 Input 误认 editor。装饰性默认内容保留一个 Tab 入口，应用的交互式 affix 使用原生焦点顺序。普通 query、活动项、值、主题和尺寸变化保留选择器/候选 identity；options 添加/删除/重排按稳定 key 局部协调。销毁或回调内卸载后，旧输入会话、捕获和 ref 均失效。

Theme 支持继承、inherit=false、独立 component seed/algorithm 和显式 override；colors、metrics、typography、effects、order 分开订阅失效域。新增 Select 的 snapshot identity 会变化，既有组件的 resolved Token 值必须保持。

Gallery 保留既有 Select 示例，并新增多选、Tags、窗口长列表和合成异步源。058 的平台通用验证及 Windows MSVC Debug/Release、D3D12/DXIL 的20次主题/缩放窗口验收已通过；证据见 [Windows 验收](../openspec/changes/058-20261023-add-native-select/evidence/windows.md)。自动输入事件不替代人工系统IME；Linux 原生验收仍独立待完成。

## 多选和标签

```cpp
ryn::Signal<ryn::SelectValues> values{ryn::SelectValues{ryn::SelectValue{1}}};
ryn::Select(ryn::SelectProps{}.mode(ryn::SelectMode::Tags)
    .values(values).onValuesChange([values](ryn::SelectValues request) mutable {
        values.set(std::move(request));
    })
    .options({{1, u8"管理员"}, {2, u8"成员"}})
    .tokenSeparators({u8",", u8"；"}).maxTagCount(std::size_t{2})
    .allowClear().layout(ryn::LayoutStyle{}.width(ryn::dp(320))));
```

`SelectValues = vector<SelectValue>` 保持字符串/整数的类型；`.values` 与 `.defaultValues` 对应受控/非受控多值模型，不能与单选 value/defaultValue 混用。非受控 mode 切换在单值和多值间保留首值；受控 mode 更新必须保持所绑定的 typed value 模型，否则整体拒绝，改变模型类型时应挂载匹配的控件。`onChange(SelectSelection)` 原签名不变，多选用明确的 `onValuesChange`，避免泛型 lambda overload 歧义。`labeledValue(Prop<SelectLabeledSelection>)` / `labeledValues(Prop<SelectLabeledValues>)` 保存应用提供的值及显示标签，`onLabeledChange(vector<SelectLabeledValue>)` 提供 typed label-in-value 请求；raw 与 labeled 只能选择一种权威来源。

Multiple 选择既有选项，Tags 允许通过 Enter 创建字符串值。分隔符可为 Unicode String，`tokenizer(function<vector<String>(StringView)>)` 可提供自定义完整 token 拆分；原样的单个 token 保留为正在编辑的文本，Enter 才提交，多 token 或解析后发生变化的 token 才自动提交。普通分隔符输入保留最后未结束的 token。相同 typed value 去重，禁用项不提交；`maxCount` 限制新增数量，不截断外部权威值。多选默认允许搜索，也可显式 `showSearch(false)`；选项点击/Enter 切换值并保持 popup 打开，空搜索 Backspace 移除最后可移除项。`autoClearSearchValue(false)` 保留选择后的搜索。

`maxTagCount(size_t)` 限制展示，`responsiveMaxTagCount` 通过实际保留标签、字体和 affix 测量适配容器，保留编辑空间并在 resize 时重新分配；`maxTagTextLength` 按 Unicode grapheme 截断，`maxTagPlaceholder` 接收 omitted typed values。隐藏标签不消费输入。默认标签有原生移除动作；自定义 `tagRender(SelectTagRenderer)` 的 `SelectRenderInfo` 提供 value、reactive label/selected/disabled 和 remove 回调，应用决定呈现。`labelRender` 提供单选保留标签；renderer 身份不变时保留其子树，变化时仅替换对应内容 owner。

## 建议、分组与渲染

`groups(Prop<vector<SelectOptionGroup>>)` 表达稳定 key 的组和选项，组头不能选择，过滤后空组不展示。普通 option 的 `key` 可显式指定；省略时由 typed value 生成，字符串与整数不会冲突。重复值/键在发布前拒绝。`rich_label` 和 `optionRender(SelectOptionRenderer)` 保留原生富内容，可含 Input/Button。相同 key 的富控件在重排或 label 更新时保留草稿；替换 rich/renderer 只重建该标签。

默认 Unicode label 过滤继续兼容旧 API；`filterOption(SelectFilter{false})` 关闭过滤，predicate 接收 query 和 typed option。`optionFilterProp`/`optionLabelProp` 使用 `SelectOptionField::Label|Value`，`filterSort(SelectSort)` 在组内稳定排序并接收 search query；`searchValue(Prop<String>)` 是显式搜索权威，`hideSelected` 排除已选项。过滤和排序应为纯函数。`defaultActiveFirstOption(false)` 关闭自动首项；键盘仍可主动导航。

`popupRender(SelectPopupRenderer)` 接收本次同步构建中必须调用一次的 origin 函数，可用 Flex 加入额外原生内容；额外内容参与测量，子 popup 保持最近窗口层的 wheel/Escape 归属。更换 popup renderer 释放其 body，保留主编辑器。`placement` 支持 Top/Bottom Left/Right，`popupWidth`、`popupMatchSelectWidth`、`direction` 提供宽度/非虚拟和 RTL 合同。自定义 suffix slot 替代默认图标；typed `clearIcon`、`suffixIcon`、`searchIcon`、`loadingIcon`、`removeIcon`、`menuItemSelectedIcon` 使用现有 IconSource。

`virtualList` 默认开启；超过 32 项时只创建窗口与 overscan 普通行，Home/End/上下键滚入实际可见区域。离窗普通行释放，已访问的富内容作为不活动 keyed 子树暂存以保留草稿，删除 key 后释放；富内容缓存随访问 key 增长，普通行数量有界。过滤/离窗/禁用清除隐藏输入的 focus、capture 和 IME。`motion(true)` 提供 Theme fast duration 的有限淡入，默认保持旧版即时行为；hidden/reduced motion/Theme motion=false 停止，idle 不持续请求帧。

## 异步、事件和 Form

`SelectSource = AsyncOptionsSource<SelectOption>` 与 AutoComplete 使用同一应用 provider、线程安全取消票据和 `OwnerThreadPost` 合同；应用负责 transport 与 owner-thread event loop，Core 不直接联网。新查询替代旧请求，关闭/reset/隐藏/禁用/窗口失活/source 更换/销毁后旧结果失效；仅当前 generation 的首次 owner-thread 完成可发布。错误交给 `onLoadError`，不替换 options。完整适配说明见 [AutoComplete](auto-complete.md)。Gallery 的异步样例是明确标注的可交付合成数据。

值变化先发对应 change 请求，再发新增项 `onSelect` 或移除项 `onDeselect`；单选重复选择仍发 onSelect，批量 clear 发 onClear。`onActive` 区分导航与提交；`onPopupScroll(float)` 给出逻辑偏移；`onInputKeyDown(SelectKeyEvent)` 可返回 true 消费按键，event 包含组合输入和 modifier 状态。程序 Prop 更新不伪造用户事件。回调自毁或抛出不能让旧请求/资源作用到新 generation。

Form 继续显式 `.value(field.value()).onChange(field.onChange()).ref(ref)` 或对应多值绑定，FormItem `.control(ref)`；并显式连接 disabled/readOnly/status。reset 即使同值也结束 preedit、清理搜索/浮层和异步票据，不自动绑定任意子控件。

107 的平台通用结果见 [Linux HEADLESS 验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/select-linux.md)。Linux 私有后台 Wayland/Vulkan 的有限输入与离页清理见 [组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/select-autocomplete-integration.md)；系统 IME、跨实际显示器、辅助技术和 Windows MSVC 原生窗口仍未验证。DOM/CSS/React/浏览器 portal、fieldNames 的动态对象字段映射及废弃 Web 别名不进入 typed Core。

## 默认空态的语言来源

未设置 `notFoundContent` 时使用最近 ConfigProvider 的有效语言包 `common.empty`；无 Provider 保持 `No data`。
显式英文和显式空字符串均保持原文，已有 option/popup renderer 优先级不变。默认 placeholder 继续为空。
语言更新只投影保留空行的文案，不重配 options、重启异步搜索或设置 value/open，也不重跑富内容 renderer。
108 选择族新增源码回归入口为 `rynui.i18n_components_selection`；本子阶段交接时尚未编译、运行或取得 GPU/系统 IME 证据。
