# ConfigProvider 的原生默认属性

`ConfigProviderProps::size`、`disabled`、`direction` 为后代已有原生属性提供默认来源。locale、formatLocale、messages、catalog、Theme 和捕获上下文见 [国际化基础](i18n.md)。本页以 Ant Design **6.6.5**、commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的本地源码为基线；不增加 Web 兼容属性。

```cpp
ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Small};
ryn::Signal<bool> disabled{true};
ryn::ConfigProvider(
    ryn::ConfigProviderProps{}.size(size).disabled(disabled)
        .direction(ryn::TextDirection::RightToLeft),
    ryn::ConfigProviderContent{[] {
        ryn::Input(ryn::InputProps{}); // 继承
        ryn::Input(ryn::InputProps{}.size(ryn::ControlSize::Middle)
            .disabled(false).direction(ryn::TextDirection::Auto)); // 三项均为显式来源
    }});
```

## 消费者矩阵

以下列出已接入源码的真实消费者。各组件通过自己的 Prop 连接更新保留布局、编辑器或交互，不通过 Provider 根节点统一阻断输入。

| 默认字段 | 当前原生消费者 |
| --- | --- |
| size，输入族 | Button；Input/TextArea/Password/Search/InputNumber/OTP；AutoComplete/Mentions/Select/TreeSelect/Cascader/ColorPicker；DatePicker/TimePicker 及各 Range；RadioGroup/RadioButton、Switch、Rate、Segmented |
| size，展示与布局族 | Pagination/Table/Tabs/Steps；Card/Collapse/Descriptions；Avatar/AvatarGroup；Spin、SkeletonAvatar/Button/Input；SpaceCompact/SpaceAddon |
| size，已有原生扩展 | Transfer、Upload 的现有 ControlSize 属性；保持该原生属性，不声称上游 Upload/Transfer 自身调用 useSize |
| disabled，输入族 | Button；Input/TextArea/Password/Search/InputNumber/OTP；AutoComplete/Mentions/Select/TreeSelect/Cascader/ColorPicker；DatePicker/TimePicker 及各 Range；Checkbox/Radio/Switch、Slider/RangeSlider/MultiSlider、Rate、Tree、Transfer、Upload |
| disabled，其他现有属性 | Tag/CheckableTag/TagGroup、FloatButtonGroup/FloatButton；原生 SpaceAddon 与 Listy 的现有禁用属性 |
| direction，文本与输入 | Text、Typography/Title；Input 派生族、OTP、AutoComplete/Mentions/Select/TreeSelect/Cascader/ColorPicker、日期/时间/Range、Checkbox/Radio/Group/Switch/Rate/Transfer；Button 的逻辑 Start/End 图标位置；Slider 水平轨道的 RTL 与 reverse 组合 |
| direction，其他已有方向属性 | Breadcrumb/Menu/Pagination/Tabs/Steps/Segmented/Table/Tree；Calendar/Carousel、Collapse/Descriptions、Popover/Tooltip/Tour、Tag 族、Badge/Ribbon、AvatarGroup、Image/PreviewGroup、Upload、Listy/Timeline；Drawer 与 Modal 默认 footer；Flex/Row/Space/SpaceCompact/Masonry、Divider |

上游证据主要为 `components/config-provider/{DisabledContext,SizeContext}.tsx`、`button/Button.tsx`、`input/{Input,TextArea,Search,OTP}`、各选择器/Picker 的 `useSize` 与 `DisabledContext` 调用，以及 Form/Radio/Checkbox 的上下文组合。上游没有从 DisabledContext 读取的 Carousel、Calendar、Menu、Segmented、Popover、Tooltip、Tour、Image 等，保持其原生局部 disabled 含义，不将这些不同用途的开关统一改为 Provider 输入默认。Progress 的尺寸、水印/BorderBeam 的启停、Space 的间距等也不被误当作 ControlSize 或输入禁用。

没有 Provider 或没有对应字段来源时，保留组件原缺省：例如 Text/Input 的 Auto、Table 的 Large、Avatar 的 Medium、Descriptions 的 Default。Provider 显式 Middle 与组件未设置可产生不同结果，不能用值比较判断来源。

## 优先级和输入权威

size 依次采用显式控件、适用控件的 Compact、已有局部 Form/RadioGroup、最近 Provider、原组件缺省。Button/Input/RadioGroup 的既有 explicit_size_ 保留；复合选择器和 Picker 在转交内部 Input 前保留来源，不能把缺省 Middle 变成显式值。Form 的完整 presentation 与显式 size/direction 是局部来源；显式控件 Middle/Auto 保留。Card/Steps 只区分 Small 与 Default，Switch 只区分 Small 与 Middle；对应 Provider Large 按该组件现有原生枚举映射。

disabled 首先采用显式控件布尔值，否则读取最近 Provider。内嵌 Provider false 可覆盖外层 true。CheckboxGroup/RadioGroup/Form 未设置的 false 不会被替换为 inherited true 写入原生 OR gate；其成员直接读取自己捕获的配置。显式本地 Group/Form true、FormItem gate、loading/busy、只读限制及 Modal/Spin 输入域继续按既有合同组合。显式成员 false 不解除这些本地安全限制。CheckboxGroup/RadioGroup/Form 的 false 只清除对应本地 gate；Provider 默认仍由实际成员及其显式属性决定。

Modal 内置默认 footer 按固定上游 `modal/shared.tsx` 使用显式 disabled(false)，保持关闭/确认动作可用；内部 loading/busy 仍控制提交。应用提供的 footer 和其中显式 Button 属性不被覆盖。Provider 不操作 WindowOverlayStack，不改变 Modal/Spin 输入租约。

direction 与 locale 独立。支持 Auto 的组件继续保留显式 Auto 的原语义；二元 RTL/布局字段从 Provider Auto 映射为 LTR。Table/Tour 等原本只接受 LTR/RTL 的显式方向校验保持。Slider 的 reverse 仍是业务反转，RTL 只与水平轨道组合，不改变垂直轨道。

## CheckboxOption / RadioOption 的 disabled 来源

这两个 aggregate 的字段名称、前缀和顺序保留；`disabled` 的精确类型由 bool 改为独立小值类型 `SelectionOptionDisabled`。默认构造为未设置；隐式 bool 构造和 bool 赋值均记录显式来源。隐式 bool 读取仍返回旧值，未设置读取为 false；`if (option.disabled)`、`!option.disabled`、`option.disabled == false`、赋值和第三个 aggregate bool 初始化继续使用原有布尔含义。它不使用 optional 的 has_value 作为真假值。

```cpp
ryn::CheckboxOption inherited{ryn::String{u8"a"}, u8"Inherited"};
ryn::CheckboxOption enabled{ryn::String{u8"b"}, u8"Enabled", false};
inherited.disabled = false; // 显式 false
inherited.disabled = ryn::SelectionOptionDisabled{}; // 恢复未设置
const bool legacy_value = inherited.disabled;
const bool effective = inherited.disabled.valueOr(true); // 继承值为 true
const auto source = inherited.disabled.explicitValue(); // optional<bool>，只用于来源查询
```

两个 wrapper 间的 equality 包含来源，所以未设置和显式 false 能使 Reactive 继续传播；wrapper 与 bool 的 comparison 按旧布尔值。生成控件用自己的捕获 scope 解析缺省，动态 options 切换来源保留原 value/key、选择和组件身份。显式 Group true 的原生 OR gate 仍有效。

精确类型相关用法需要迁移：`decltype(option.disabled)` 现在为 `SelectionOptionDisabled`；不能继续绑定 bool& 或取 bool*。需要普通 bool 时复制或 `static_cast<bool>`，向 `Prop<bool>` 等另一个转换类型传参也应显式转换，避免依赖两次用户定义转换。`selection_option_disabled.hpp` 可独立包含，未扩展 Prop 或其他 option 类型。

## 更新与生命周期

私有来源标记区分未设置和显式 false/Middle/Auto。组件连接投影后的有效值；locale-only 更新不重新应用相同的 size/disabled/direction，不重跑 Table 数据投影、row factory 或 Form validator。配置变化不重建 slot、保留 editor 或同 key 子组件。打开 popup、延迟 append 和 App Modal/Drawer/通知服务继续使用现有捕获的 Theme/config/window owner。

禁用使用实际组件的资格更新路径，清除 pointer capture、焦点/IME 会话和失效输入；恢复时保留业务值和 editor 身份。退出活动 IME 可取消 preedit，不能把取消会话误报为保留活动 composition。size/direction 等展示变化保留 draft、反向 selection、composition 和 undo。用户 getter、observer 或 blur 回调返回后，必须重新检查 owner/ComponentId 代际，不能继续使用已退役的裸 state。

## 自动入口

`rynui_config_provider_consumers_tests` 注册 `rynui.config_provider_sources`、`compact_group`、`input_gates`、`reentry`、`form_sources`、`compound`、`popup_service`、`collections_idle`、`getter_retirement`、`boundary_rtl`、`observer_retirement`、`generated_options`。独立公开头 TU 包含旧布尔调用/aggregate 编译断言。12 项真实 Host/Provider 回归已在正式 MSVC HEADLESS Debug/Release 通过，涵盖来源、嵌套/动态更新、双窗口、Compact/Form/Group、编辑/选区/preedit、弹层/服务晚挂、同 key 与等值 idle。实际平台边界见 [验收记录](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/config-gallery-windows.md)；CPU 输入事件不作为系统 IME 实测。

Linux 私有 Wayland/Vulkan 的实际默认尺寸、方向、禁用与显式/嵌套覆盖、语言更新保留草稿，以及真实 capture/Core preedit 离页退役见[其他分类验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/application-context-linux.md)。目录中的原生功能状态与 Windows、物理系统 IME、OS 辅助技术及跨实际显示器的覆盖分别记录。
