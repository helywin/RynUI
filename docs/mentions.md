# Mentions

真实保留的原生多行提及编辑器，基于 TextArea 与逻辑窗口层；参考 [Ant Design 6.6.5 的锁定源码](https://github.com/ant-design/ant-design/tree/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/mentions)。组件不直接访问网络、SDL 或 renderer。

```cpp
#include <ryn/mentions.hpp>

ryn::Mentions(ryn::MentionsProps{}
    .defaultValue(u8"消息：\n")
    .prefix({u8"@", u8"#", u8"提及"})
    .options({{u8"ada", u8"Ada", u8"Ada"}, {u8"ming", u8"阿明", u8"阿明"}})
    .autoSize(ryn::TextAreaAutoSize{true, 2, 6})
    .maxLength(std::size_t{500})
    .layout(ryn::LayoutStyle{}.width(ryn::dp(320))));
```

`value(Prop<String>)` 控制完整文档，`defaultValue(String)` 仅设置初值，两者不可并用。`MentionsRef` 与 TextAreaRef 相同，提供 focus、blur、select；选区使用 UTF-8 byte offset，端点必须是完整 grapheme 边界。保留 TextArea 的 Unicode/bidi 编辑、换行、selection、撤销/重做、剪贴板、组合输入、滚动、resize、rows、autoSize、maxLength、disabled/readOnly、size/status/variant/direction 和 ref 行为。`visible(false)` 停用整个分支并释放输入；`layout` 只控制外部布局。

`prefix` 接受单个 String、响应式 `Prop<String>`、String 列表或响应式列表，默认 `@`。在实际折叠 caret 之前找最后结束的完整前缀，同一结束位置选最长前缀，支持 `@`/`@@` 及 Unicode 重叠。默认 search 验证拒绝 split、换行和 tab；`split` 默认空格，空 split 不插入分隔符。自定义 `MentionsSearchValidator` 接收 query 与只含 prefix/split 的 `MentionsConfig`，返回 bool；predicate 应保持纯函数。

上下键只在有效 query 的建议中导航，并跳过 disabled/group；Enter 选择、Escape 关闭、Tab/ShiftTab 从主编辑器的相邻位置离开整个 compound。组合输入活动期间，这些命令优先交给原生编辑器，preedit 不发布文本或搜索。pointer 可选择建议，也能操作富行及自定义 popup 内的输入；子窗口层优先处理自己的 Escape/wheel。建议 popup 锚定实际 caret，跟随换行、TextArea 滚动、resize、祖先变换和窗口可用边界，支持 Top/Bottom placement 及溢出翻转。滚出 caret 可见区域时隐藏建议。

选择通过 TextArea 的真实 replace transaction 完成：只替换活动 prefix/query 区域，复用 caret 后与选值相同的完整 grapheme 后缀，插入选值和 split，避免重复 split。嵌入普通文本的前缀前会补 split；换行后直接保留换行。maxLength 由相同的原生编辑事务裁剪，selection 位于实际插入末尾；不会切开 emoji/组合字符。

编辑的事件顺序为 `onChange(String)`、有效 query 的 `onSearch(String query, String prefix)`。选择顺序为变化值的 onChange、`onSelect(MentionsOption, String prefix)`，不伪造搜索事件。程序 value 更新不发 onChange，受控拒绝后恢复应用接受的完整文档；onSearch 仅用于更新建议。clear 走原生编辑器并发 onClear。回调可重入或销毁组件；失败 owner 清理或原子回滚后，其他控件继续工作。

每个 option 具有稳定唯一非空 key、value、label、disabled，可带 `MentionsRichLabel{MentionsOptionLabel{...}}`。普通行在长列表中按可见范围窗口化；富行一旦挂载，只要 key 与 rich label 身份存活，就保留局部草稿。富行使用 Theme 的固定 option_height，内容超出时裁剪；离屏或过滤掉的行不能持有焦点、preedit 或 capture。普通 Prop 更新不重挂编辑器或存活富行。`groups` 是显式原生扩展，组标题不可选择；`filterOption(false)` 关闭本地过滤，默认对 value 做 Unicode normalization/case folding。自定义 `MentionsFilter` 可返回 bool。

`notFoundContent` 接受 String 或 typed `MentionsRichLabel` 内容。loading 与异步 pending 使用有限静态加载状态，不产生 idle 动画。`popupRender` 包裹保留的 origin，必须同步调用一次。可选 `editor(MentionsEditor)` 用于包装原生 TextArea，必须保留给定的 value/ref/event 绑定并且只挂载一个 MultiLine 编辑器。更换 renderer 只重建其局部拥有区域；富内容的普通数据更新应使用 reactive Props。

```cpp
const auto entities = ryn::getMentions(u8"@Ada #native", {{u8"@", u8"#"}, u8" "});
// entities: {"@", "Ada"}, {"#", "native"}
```

`getMentions` 是纯 typed helper，按显式 split 切 token，token 起点匹配 prefix，忽略空值。默认 split 不隐式解释换行。原生扩展的重叠前缀选择最长匹配；空 split 按完整 grapheme 分段，有限执行。

`source(Prop<MentionsSource>)` 复用 `AsyncOptionsSource<MentionsOption>`。provider 收到 query、generation 和线程安全取消标志，返回可选取消函数，通过 Complete 返回 options/error。`OwnerThreadPost` 必须把任务投递到组件创建线程并唤醒应用事件循环；工作线程不得执行 Core 修改。`onSearch` 同时提供 prefix，应用可据此调整当前 source 或 options。旧 query、close、reset、disabled/readOnly/hidden、窗口失活、source 更换、unmount/window destroy 取消旧票据；过期和重复完成不生效，错误发 onLoadError。取消回调重入或抛出不覆盖新票据。

Form 使用显式 `FormField<String>` value/onChange 与 `FormItemProps::control(ref)`；同值 reset 也结束 preedit、清除 query/popup、取消 pending，不发假的 onChange。可直接编译的完整示例见 [mentions_example.hpp](../examples/mentions_example.hpp)。不改写 Form 的绑定或验证核心。

`ThemeConfig::mentions` 独立配置：`tokens.input` 使用 native InputTokenOverride，`tokens.suggestions` 配置 popup/option 的颜色、指标、字体、shadow 和层级。组件 seed 只有 `algorithm=true` 时生效，支持嵌套 inherit 和全局 Default/Dark/Compact 算法。snapshot、hash、订阅和 dirty phase 包含这些字段；不改变 Input、Select、AutoComplete 或其他组件的配置。`motion(true)` 只启用有限开启动画，reduced motion 立即收敛。

Gallery 的独立 Mentions 页提供真实多行、前缀搜索、富建议草稿、受控拒绝、Form reset、禁用/只读/显隐/方向/placement、10,000 项建议和显式交付的合成异步源。平台结果见 [107 Mentions 验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/mentions-linux.md)。Core composition 测试不代表物理系统 IME 候选 UI；Windows、物理显示器和系统辅助技术的结果单独记录。Web DOM/CSS/React/HTML、浏览器 portal 及网络 transport 不属于原生公开组件合同。

主线组合与完整 Theme 基线结果见 [Mentions 集成验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/mentions-integration.md)。
