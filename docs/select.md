# Select

058 提供原生单选 Select，复用真实 Input、IME、窗口层与 retained scene。平台通用、Gallery 和 Windows/Linux 原生验收分别记录在 change 的 tasks/evidence；未通过的平台项保持待验收。

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

首版范围为单选、搜索、清空、动态 options、loading/disabled、键盘导航、三尺寸、四变体和 status。multiple/tags、分组、自定义 option render、虚拟列表与内置异步数据源后续扩展。

`SelectValue` 保留 `String` 或有符号64位整数的类型，字符串 `"1"` 与整数 `1` 是不同值。`SelectOption` 包含 value、label、disabled，重复 typed value 会在发布新 options 前整体拒绝。

`value` 绑定 `Prop<SelectSelection>`，其中 `SelectSelection = std::optional<SelectValue>`；空值表示没有选择。受控模式通过 onChange 请求值，父级不回写时保留原显示。未知值保留原值文本，后续 options 到达后显示对应 label。defaultValue 用于非受控模式。

搜索仅过滤 label，不提交选择。使用 Unicode 规范化和完整大小写折叠，支持中文、变长折叠（如 Straße/STRASSE）及组合字符；不移除重音，也不转换简繁。沿用仓库锁定的 utf8proc 2.11.3，由现有内部 text_boundary 隔离第三方 API。

Select Component Token 通过 `ThemeConfig::select` 配置，覆盖菜单颜色、行高/间距/圆角、字体/选中字重、shadow 和 z-index，以及 selector/clear 背景、hover/active border 与 active outline。配方依据 Ant Design 6.6.5；尺寸、变体、status 的选择器仍复用 Input 的配方。视觉 Token 不进入 LayoutStyle。

`size` 使用 ControlSize 的 Small/Middle/Large，`variant` 使用 InputVariant 的 Outlined/Filled/Borderless/Underlined，`status` 使用 InputStatus 的 Default/Warning/Error。disabled 关闭菜单并清理焦点/输入会话；loading 保留搜索能力但阻止候选提交和清空，默认 suffix 显示 loading 图标。自定义 suffix 会替代默认图标。

鼠标点击选择器打开菜单，点击候选提交选择；ArrowUp/Down、Home/End 跳过禁用项并把活动项滚入视口，Enter 确认，Escape/Tab/外部点击请求关闭。IME 组合期间这些键交给输入法；第一次 Escape 取消组合，后续 Escape 再关闭菜单。列表支持滚轮，边缘空间不足时向上翻转并限制在窗口范围；首版为非虚拟列表。

`open`/`onOpenChange` 使用受控请求语义，父级不回写时保留外部打开值；失焦、禁用和窗口失活会另外隐藏菜单以完成清理。defaultOpen 用于非受控模式。打开搜索时显示 query，选中 label 作为搜索 placeholder；关闭会丢弃 query 并恢复选中 label。`SelectRef` 复用 InputRef 的 focus/blur/搜索选区。

prefix/suffix 为装饰性 typed slots，可放文字与图标；首版拒绝额外 focusable 控件，保证内置 Select 只有一个 Tab 入口。普通 query、活动项、值、主题和尺寸变化保留选择器/候选 identity；只有显式 options 更新才替换候选子树。销毁或回调内卸载后，旧输入会话、捕获和 ref 均失效。

Theme 支持继承、inherit=false、独立 component seed/algorithm 和显式 override；colors、metrics、typography、effects、order 分开订阅失效域。新增 Select 的 snapshot identity 会变化，既有组件的 resolved Token 值必须保持。
