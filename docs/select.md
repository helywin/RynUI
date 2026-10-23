# Select

058 正在实现原生单选 Select。当前阶段完成 typed API 声明、选择/搜索模型和 Theme Token；窗口组件、Gallery 与原生验收仍按 tasks 推进，暂不把 API 声明视为可挂载功能。

首版范围为单选、搜索、清空、动态 options、loading/disabled、键盘导航、三尺寸、四变体和 status。multiple/tags、分组、自定义 option render、虚拟列表与内置异步数据源后续扩展。

`SelectValue` 保留 `String` 或有符号64位整数的类型，字符串 `"1"` 与整数 `1` 是不同值。`SelectOption` 包含 value、label、disabled，重复 typed value 会在发布新 options 前整体拒绝。

`value` 绑定 `Prop<SelectSelection>`，其中 `SelectSelection = std::optional<SelectValue>`；空值表示没有选择。受控模式通过 onChange 请求值，父级不回写时保留原显示。未知值保留原值文本，后续 options 到达后显示对应 label。defaultValue 用于非受控模式。

搜索仅过滤 label，不提交选择。使用 Unicode 规范化和完整大小写折叠，支持中文、变长折叠（如 Straße/STRASSE）及组合字符；不移除重音，也不转换简繁。沿用仓库锁定的 utf8proc 2.11.3，由现有内部 text_boundary 隔离第三方 API。

Select Component Token 通过 `ThemeConfig::select` 配置，覆盖菜单颜色、行高/间距/圆角、字体/选中字重、shadow 和 z-index，以及 selector/clear 背景、hover/active border 与 active outline。配方依据 Ant Design 6.6.5；尺寸、变体、status 的选择器仍复用 Input 的配方。视觉 Token 不进入 LayoutStyle。

Theme 支持继承、inherit=false、独立 component seed/algorithm 和显式 override；colors、metrics、typography、effects、order 分开订阅失效域。新增 Select 的 snapshot identity 会变化，既有组件的 resolved Token 值必须保持。
