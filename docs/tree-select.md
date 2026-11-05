# TreeSelect

`<ryn/tree_select.hpp>`（也包含于 `<ryn/rynui.hpp>`）组合真实 retained Input、Popover 和 Tree。旧 `TreeSelect(props, std::vector<TreeSelectItem>)`、String 单值与 `TreeSelectRef = InputRef` 兼容；空 String 表示清空，可继续接入 `FormField<String>`。拓扑、异步、窗口行及节点交互依赖 [Tree](tree.md)。

```cpp
void destination() {
    ryn::TreeSelect(ryn::TreeSelectProps{}.placeholder(u8"选择项目").allowClear()
                        .treeDefaultExpandedKeys({u8"project"})
                        .layout(ryn::LayoutStyle{}.width(ryn::dp(280))),
                    {ryn::TreeSelectItem{u8"project", u8"项目",
                        {ryn::TreeSelectItem{u8"src", u8"源码"},
                         ryn::TreeSelectItem{u8"docs", u8"文档"}}}});
}
```

单值再次选择当前 key 请求关闭，不清空或重复 onChange。未知接受值保留并显示 key；已接受节点的标题更新刷新显示，隐藏/禁用不擅自改写应用值。`.items(Prop<std::vector<TreeSelectItem>>)` 加 `TreeSelect(props)` 支持稳定 key 增删、重排和跨父移动，最多 100,000 节点、64 层；更新先验证。节点 richTitle/icon、contentRevision、selectable/checkable/disableCheckbox/isLeaf 与 Tree 相同。相同 key/版本保留富节点草稿，工厂替换或移除取消旧后代输入。

## 多值、checkbox 与标签

`.multiple()` 开启多选；`.treeCheckable()` 开启 checkbox 并隐含多值显示。`.value(Prop<TreeKeys>)`、`.defaultValue(TreeKeys)` 与 `.onValuesChange` 提供 typed 多值；旧 String value/onChange 保留。不同 value 权威互斥。

```cpp
void targets(ryn::Signal<ryn::TreeKeys> values) {
    ryn::TreeSelect(ryn::TreeSelectProps{}.multiple().value(values).showSearch().allowClear()
                        .onValuesChange([values](ryn::TreeKeys next) mutable { values.set(next); })
                        .layout(ryn::LayoutStyle{}.width(ryn::dp(320))),
                    {ryn::TreeSelectItem{u8"a", u8"Alpha"}, ryn::TreeSelectItem{u8"b", u8"Beta"}});
}
```

`treeCheckStrictly` 关闭父子级联；`showCheckedStrategy(ShowAll|ShowParent|ShowChild)` 控制 checkbox 输出集合，默认 ShowChild。checked/halfChecked 由实际 Tree 处理，禁用隔离不跨边界。`TreeSelectLabelValues` 的每项包含 value、label、half_checked；`.value(Prop<TreeSelectLabelValues>)` 与 `.onLabelChange` 提供原生 labelInValue 合同，strict 事件也交付显式半选。已接受标签文本按 key 保留，选中 key 暂时不在数据中时仍可显示；最新已知节点标题更新保留标签。

标签是真实可交互 Tag，支持移除；受控拒绝保留接受值和标签。`tagRender(TreeSelectTagInfo)` 接收 value、响应 label/disabled 与 onClose，可组合任意富原生内容；自定义工厂使用 Tag 时传入 `.visible(true)`，由接受值决定移除，保留受控拒绝时的标签。`maxTagCount(std::size_t)` 固定数量，`TreeSelectTagCount::Responsive` 依据实际字体、标签、Input padding 和 affix 测量分配空间；溢出显示 +N。`maxTagTextLength` 按 UTF-8 code point 截断，不截坏字节。响应式隐藏的富标签保持身份但封锁输入；删除值释放。标签缓存随接受值集合增长。

## 搜索、popup 与加载

`showSearch` 使 selector 使用 Input 的真实 Unicode 编辑和 IME session；`searchValue` 为受控查询，onSearch 交付请求。默认标题包含过滤，`filterTreeNode(key,title)` 自定义匹配，结果及祖先由真实 Tree 显露。编辑 composition 期间 TreeSelect 不截获候选确认键；IME 平台候选 UI 验收独立。

`prefix(TreeSelectAffix)`、`suffix(TreeSelectAffix)`、`tagRender` 和 `popupRender(std::function<void()> original)` 提供真实 typed slots。popupRender 必须调用传入的 `original()`，以保留 Input/Popover/Tree 合同；只执行一次并保留富内容，失败工厂干净退役本组件。普通属性变化不重新执行应用组合，也不借用其他 Tree 或窗口级 capture。

```cpp
#include <ryn/flex.hpp>
#include <ryn/button.hpp>
#include <ryn/text.hpp>

void custom_popup() {
    ryn::TreeSelect(ryn::TreeSelectProps{}.popupRender([](std::function<void()> original) {
        return ryn::TreeSelectPopup{[original] {
            ryn::Flex(ryn::FlexProps{}.vertical(true), ryn::FlexContent{[original] {
                original();
                ryn::Button(ryn::ButtonProps{}.onClick([] {}), [] { ryn::Text(u8"Popup action"); });
            }});
        }};
    }), {ryn::TreeSelectItem{u8"a", u8"Alpha"}});
}
```

`loadData(TreeLoader)`、`treeLoadedKeys` 和 onTreeLoad 使用 Tree 的每节点独立取消/generation/owner-thread dispatcher 合同，应用拥有 transport；无 Core 网络访问。加载 children 后更新节点标签与可选 key，加载中/失败/重试使用实际 Tree 状态。隐藏、禁用、只读、失活、零面积、reset 和卸载拒绝旧结果；展开另一个节点不取消合法请求。`virtualized`、`rowHeight`、`treeRef(TreeRef)` 复用实际有界 Tree 行与 scrollTo，不通过挂载全部行伪装虚拟化。

## 受控状态、输入与 Form

value/defaultValue、open/defaultOpen、treeExpandedKeys/treeDefaultExpandedKeys 互斥。受控值、查询、展开和开合只交付请求，应用回写才接受；拒绝后可重试。多选保持 popup 打开，单选选择合法 key 请求关闭。allowClear、readOnly、disabled、visible、size/status/variant、placement、rtl 和有限主题 motion 遵守原生门禁。

指针打开；Down/Up/Enter/Space 把焦点交给实际 Tree；方向键、Home/End、Enter/Space 遵守 Tree。富子弹层先处理 Escape，再关闭 TreeSelect 并恢复 selector。Tab/Shift+Tab 按 selector 所在顺序离开；onFocus/onBlur 是真实 Input 焦点事件。关闭、失活和隐藏终止待转移焦点，恢复不复活旧 popup。

String Form 继续使用字段 value()/onChange()、FormItem `.control(ref)` 与同一 TreeSelectRef。新增 Input form-reset hook 在相同接受值 reset 时也取消节点请求、清空查询草稿、待聚焦意图和实际 popup；保留 selector/editor/Tree 身份，不产生选择事件。多值可用显式 `.onValuesChange`/`.value` 接应用字段 adapter，本组件不改写 Form core 的受支持值类型。

TreeSelectRef 保留 InputRef 的 bound/focus/blur 与文本选择合同；Tree 行滚动使用独立 `.treeRef(ref)`，失效 generation 不操作替代宿主。`listHeight` 正且有限，默认 256 logical px；popup 受窗口限制并匹配 selector 宽度。LayoutStyle 只控制外部布局。

## Theme 与验收

`ThemeConfig.tree_select` 独立于 Select/Tree；selector tokens 控制背景、边框、outline 和 popup padding/radius/gap/shadow/order，tree tokens 控制节点视觉与字体，Input/Tag 继续使用共同原生 Token。完整五份 Theme golden 保留所有既有领域。

Gallery 独立页保留原有目标选择、受控拒绝、备注和 Form reset，并追加多选/搜索/标签、自定义 popup、strict checkbox、RTL、10,000 个动态虚拟选项、明确标注的合成异步源及真实 callbacks。结果见 [107 TreeSelect 证据](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/tree-select-linux.md)和[主线 Form 组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/tree-treeselect-integration.md)。HEADLESS 证明 logical scene/Input 合同；Linux 私有 Wayland/GPU、Windows/MSVC、系统 IME 候选 UI、辅助技术与跨显示器证据分别记录。Web DOM/CSS/React/portal、DataTransfer 和字段映射别名不移植。
