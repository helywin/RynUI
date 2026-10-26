# AutoComplete

原生自由 String 输入与建议选择。参考锁定 Ant Design 6.6.5 AutoComplete（HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`）；不依赖 React 或 DOM。

`AutoCompleteProps` 提供 `value(Prop<String>)` / `defaultValue(String)`、`options(Prop<vector<AutoCompleteOption>>)`、`placeholder`、`filterOption`、`defaultActiveFirstOption`、`backfill`、`allowClear`、`disabled`、`readOnly`、`size`、`variant`、`status`、`open` / `defaultOpen`、`listHeight`、Input `ref` 和外部 `layout`。受控与对应 default 不得同时设置。`listHeight` 必须为有限正数。

每个 `AutoCompleteOption` 有唯一非空 `key`、输入接受的 `value`、显示的 `label`、`disabled` 与可选 `AutoCompleteRichLabel{AutoCompleteOptionLabel{...}}`。相同 key 的 row 保留；普通 String label 更新不重建子树。rich label 复制保持身份，只有显式换用另一 rich label 对象才替换该局部子树。rich 内容可绑定 reactive 数据。列表更新不重建真实 Input/editor。

首次构建或新增/替换富标签时若 slot 抛出异常，失败的 AutoComplete 会退役，并把原异常继续抛给调用者；调用者可局部 catch 后挂载 fallback，不会留下可布局的半成品行。

异常退役清除旧 focus/press/capture，不发用户 blur/cancel/leave。正常焦点转移仍交付原生回调；Escape 关闭子 popup 并返回内部 trigger 时，父建议保持打开。

默认不过滤（锁定上游 tests 的 combobox 行为）；`AutoCompleteFilter{true}` 开启大小写敏感 UTF-8 value 子串过滤，也可使用 `AutoCompleteFilter{[](StringView query, const AutoCompleteOption& option) -> bool {...}}`。predicate 应为无副作用纯函数，不在计算中修改组件树。

输入提交产生 `onChange(String)` 和 `onSearch(String)`。选择先对变化值发 `onChange`，再发 `onSelect(String, AutoCompleteOption)`，不发 `onSearch`。受控值由外部接受；`onSearch` 只用于建议查询，不能代替 onChange 控制值。程序 value 更新不发用户事件；preedit 本身不发搜索。清空发变化的 onChange、onSearch 和 onClear。

上下键在建议中移动并跳过 disabled 项；Enter 选择，Escape 取消临时 backfill 并请求关闭。Tab/ShiftTab 请求关闭，并以主 Input 为锚点按正常相邻顺序离开；这也适用于 pointer 聚焦的 rich Input 和嵌套子 popup。焦点离开整个逻辑后代时请求关闭建议。backfill 不产生值事件。组合输入活动时这些建议快捷键交还编辑器。prefix/suffix 是 typed 装饰 slots，不增加 Tab 入口。rich label 中的子 popup 按最近窗口层拥有 wheel/Escape。

`open` 是外部权威，`onOpenChange` 只是意图；空过滤结果不显示 popup。输入边界隐藏、Modal/Spin 阻断、实际布局零宽/高和窗口失活会 suppress popup 并清理临时状态，恢复后需要显式打开。实际零尺寸同时清理 focus/session/preedit；首次尚未布局的临时零值不影响 defaultOpen 或初始打开的 Modal。首版有限高度滚动，无虚拟化。

Theme 通过 `ThemeConfig::auto_complete` 独立设置。`AutoCompleteThemeToken` 与 `AutoCompleteTokenOverride` 复用 Select 的视觉字段：popup/option/selector/clear 颜色、option_height/padding/radius/gap、typography、shadow 和 z_index_popup。支持继承、独立 component algorithm 和全局 dark/compact；Input 自有尺寸/状态/variant 继续使用其 Theme。视觉只来自 Theme，LayoutStyle 仅控制外部布局。最终 popup font 按实际 round 后检查 FontRuntime 1..INT_MAX/64 范围，非法后代 preflight 不发布部分 Theme。

完整可编译的 Form String 示例见 [auto_complete_example.hpp](../examples/auto_complete_example.hpp)，由 `rynui.auto_complete_example` 实际挂载、输入和 reset。调用方式：

```cpp
#include "examples/auto_complete_example.hpp"

void Page() {
    ryn::FormRef form;
    ryn::FormField<ryn::String> name{ryn::String{u8"Initial"}};
    ryn::AutoCompleteRef input;
    AutoCompleteForm(form, name, input);
}
```

字段通过 `.value(name.value()).onChange(name.onChange())` 显式采值；FormItem 使用 `.control(input)`，并显式连接字段的 disabled/readOnly/status。reset 取消 preedit、候选和 backfill，即使接受值没有改变；普通同值 value 或建议更新保留 preedit。Form 不自动修改任意子控件 props。

首版排除网络异步管理、选项分组、虚拟化、动画、RTL、Web、自定义编辑器替换与任意 popupRender。headless 合成事件不能替代 native GPU、Linux、系统 IME 或 AT 验收。

实现与 Windows MSVC headless 证据见 [092 实施记录](../openspec/changes/092-20261026-add-native-auto-complete/evidence.md)。该记录是 worker 本树结果，后续由父执行 fresh Astra 审查与 main 集成；没有把它等同于整库发布验收。
