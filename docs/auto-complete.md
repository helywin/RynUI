# AutoComplete

原生自由 String 输入与建议选择。参考锁定 Ant Design 6.6.5 AutoComplete（HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`）；不依赖 React 或 DOM。

`AutoCompleteProps` 提供 `value(Prop<String>)` / `defaultValue(String)`、`options(Prop<vector<AutoCompleteOption>>)`、`placeholder`、`filterOption`、`defaultActiveFirstOption`、`backfill`、`allowClear`、`disabled`、`readOnly`、`size`、`variant`、`status`、`open` / `defaultOpen`、`listHeight`、Input `ref` 和外部 `layout`。受控与对应 default 不得同时设置。`listHeight` 必须为有限正数。

每个 `AutoCompleteOption` 有唯一非空 `key`、输入接受的 `value`、显示的 `label`、`disabled` 与可选 `AutoCompleteRichLabel{AutoCompleteOptionLabel{...}}`。相同 key 的 row 保留；普通 String label 更新不重建子树。rich label 复制保持身份，只有显式换用另一 rich label 对象才替换该局部子树。rich 内容可绑定 reactive 数据。列表更新不重建真实 Input/editor。

首次构建或新增/替换富标签时若 slot 抛出异常，失败的 AutoComplete 会退役，并把原异常继续抛给调用者；调用者可局部 catch 后挂载 fallback，不会留下可布局的半成品行。

异常退役清除旧 focus/press/capture，不发用户 blur/cancel/leave。正常焦点转移仍交付原生回调；Escape 关闭子 popup 并返回内部 trigger 时，父建议保持打开。

默认不过滤（锁定上游 tests 的 combobox 行为）；`AutoCompleteFilter{true}` 开启大小写敏感 UTF-8 value 子串过滤，也可使用 `AutoCompleteFilter{[](StringView query, const AutoCompleteOption& option) -> bool {...}}`。predicate 应为无副作用纯函数，不在计算中修改组件树。

输入提交产生 `onChange(String)` 和 `onSearch(String)`。选择先对变化值发 `onChange`，再发 `onSelect(String, AutoCompleteOption)`，不发 `onSearch`。受控值由外部接受；`onSearch` 只用于建议查询，不能代替 onChange 控制值。程序 value 更新不发用户事件；preedit 本身不发搜索。清空发变化的 onChange、onSearch 和 onClear。

上下键在建议中移动并跳过 disabled 项；Enter 选择，Escape 取消临时 backfill 并请求关闭。Tab/ShiftTab 请求关闭，并以主 Input 为锚点按正常相邻顺序离开；这也适用于 pointer 聚焦的 rich Input 和嵌套子 popup。焦点离开整个逻辑后代时请求关闭建议。backfill 不产生值事件。组合输入活动时这些建议快捷键交还编辑器。prefix/suffix 是 typed 装饰 slots，不增加 Tab 入口。rich label 中的子 popup 按最近窗口层拥有 wheel/Escape。

`open` 是外部权威，`onOpenChange` 只是意图；空过滤结果不显示 popup。输入边界隐藏、Modal/Spin 阻断、实际布局零宽/高和窗口失活会 suppress popup 并清理临时状态，恢复后需要显式打开。实际零尺寸同时清理 focus/session/preedit；首次尚未布局的临时零值不影响 defaultOpen 或初始打开的 Modal。建议列表以 `listHeight` 限定窗口高度。

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

107 补齐以下原生能力，保留上述 String 输入、事件顺序、受控拒绝与显式 Form 绑定合同：

- `groups(Prop<vector<AutoCompleteOptionGroup>>)` 使用稳定的非空 group/option key。组头不接受选择；只保留有匹配子项的组头。也可在平铺 options 中使用 `group=true` 的组头。
- `virtualList` 默认开启；超过 32 项时，仅创建可视窗口与少量 overscan 的普通行，键盘活动项会先滚入窗口。普通行离窗释放；已挂载的富内容按 key 暂存为非活动子树，保留子编辑器草稿，离窗/过滤立即阻断输入、取消捕获和 preedit，删除 key 时释放。富内容缓存随访问的 key 增长，普通行数量有界。
- `direction(TextDirection::RightToLeft)` 同步编辑器、普通标签和 popup 对齐；`popupWidth(float)` 可扩大 popup；`popupMatchSelectWidth(false)` 保留非虚拟列表行为。`clearIcon` 接受 typed IconSource；`notFoundContent` 可在已有选项过滤为空时提供文本提示，原始 options 为空时仍不展示 popup。宽度须为有限非负数。
- `motion(true)` 启用来自 Theme fast duration 的有限 popup 淡入。默认关闭以保留原有即时开合；hidden、禁用、窗口失活、Theme motion=false 和 reduced motion 停止计时，idle 不请求下一帧。
- `popupRender(AutoCompletePopupRenderer)` 接收可直接调用的 `std::function<void()> origin`，必须在本次同步构建中调用一次。可通过 Flex 包装列表并添加原生编辑器或按钮。重复或遗漏 origin 整体拒绝；替换 renderer 仅重建其 popup body，保留主编辑器；普通属性更新不重建 renderer。
- `editor(AutoCompleteEditor)` 接收 `AutoCompleteEditorContext`，可挂载 `Input(context.input, context.prefix, context.suffix)` 或 `TextArea(context.text_area)`。必须挂载一个编辑器并保留给定 value/ref/disabled/readOnly/events。TextArea 自带原生清空。更换 editor renderer 取消旧 IME/会话并替换该编辑器，保留接受值和 popup，按需恢复焦点。普通更新保留同一个编辑器。FormItem 继续绑定外部 ref，reset 对同值也取消请求、preedit 和 backfill。

```cpp
ryn::AutoComplete(ryn::AutoCompleteProps{}
    .groups(std::vector<ryn::AutoCompleteOptionGroup>{
        {u8"people", u8"联系人", {{u8"ada", u8"Ada", u8"Ada"}}}})
    .editor(ryn::AutoCompleteEditor{[](ryn::AutoCompleteEditorContext context) {
        ryn::TextArea(context.text_area.rows(std::size_t{2}));
    }}));
```

`source(Prop<AutoCompleteSource>)` 是应用注入的异步建议服务。`AsyncOptionsRequest` 包含 query、generation 和线程安全取消标志；provider 返回可选取消函数，以 `Complete(AsyncOptionsResult<AutoCompleteOption>)` 返回 options 或 error。第二个构造参数 `OwnerThreadPost` 必须线程安全地将闭包投递到组件创建线程，并唤醒应用事件循环；Core 不直接创建网络连接或轮询线程。无效 source 表示禁用服务。

```cpp
ryn::AutoCompleteSource source{
    [](ryn::AsyncOptionsRequest request, ryn::AutoCompleteSource::Complete complete) {
        // 应用 transport 可在工作线程完成；取消时应停止 transport。
        return transport.search(request.query, std::move(complete), request.cancelled);
    },
    [](std::function<void()> apply) { application.postToUi(std::move(apply)); }};
```

上例中的 transport/application 由应用提供，并非 RynUI 内置对象。每次提交文本取代旧请求；关闭、reset、禁用、只读、窗口失活、隐藏、source 更换和销毁都会取消旧票据。完成先经 dispatcher，再检查创建线程、票据、owner generation 与存活状态；只接受当前请求的首次完成。错误交给 `onLoadError`，不替换已有建议；`onPopupScroll(float)` 返回逻辑滚动偏移。取消回调可以重入启动新请求或销毁组件，外层请求不得覆盖较新的票据。provider/应用回调抛出时退役失败 owner 并清理票据和资源。

Linux 私有后台 Wayland/Vulkan 的有限宿主输入、系统字体与离页清理已通过 [107 组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/select-autocomplete-integration.md)。系统 IME 候选 UI、跨实际显示器、辅助技术与 Windows 仍未验证。Web DOM/CSS/React/portal 与浏览器事件别名不进入 Core。

Core 已通过独立 Astra R4，并与 Tree 在 main 新构建 Debug/Release 各 98/98，见 [组合验收](../openspec/changes/094-20261026-add-native-tree/evidence/main-integration.md)；原 [092 实施记录](../openspec/changes/092-20261026-add-native-auto-complete/evidence.md) 仍保留 worker 本树边界。

Gallery 的 AutoComplete 目录直达“联系人建议”：自由输入/选择、过滤和更新建议、接受或拒绝候选、清空及禁用/只读恢复。富建议中的备注 Input 和保存按钮保持身份；下方 Form String 示例提供保存和同值 reset。096 定向与集成的完成状态见 [任务清单](../openspec/changes/096-20261028-add-tree-and-autocomplete-gallery/tasks.md)，不据此扩大为完整 Gallery 或平台验收。
