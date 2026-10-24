# DatePicker

首版为保留的原生单日期控件。`CivilDate` 表示 0001-01-01 到 9999-12-31 的有效 Gregorian 公历日期，构造非法日期抛出 `std::invalid_argument`。日期没有时区、时间或 epoch 含义。`DatePickerValue` 是 `std::optional<CivilDate>`；空值表示未选择。

`parse_iso_date(std::string_view)` 只接受十字节 `YYYY-MM-DD`，非法或空字符串返回 `nullopt`。`format_iso_date(CivilDate)` 总是返回同一固定格式。编辑中的空字符串可提交为清空，其他非法或禁用草稿不发布，失焦恢复当前选择。IME preedit 不作为日期发布。

```cpp
#include <ryn/rynui.hpp>

void date_field(ryn::Signal<ryn::DatePickerValue>& selected) {
    using namespace ryn;
    DatePicker(DatePickerProps{}
        .value(selected)
        .defaultPanelDate(CivilDate{2026, 10, 1})
        .minDate(CivilDate{2020, 1, 1})
        .maxDate(CivilDate{2030, 12, 31})
        .onChange([&selected](DatePickerValue date) { selected.set(date); }),
        DatePickerFooter{[] { Text(u8"Choose a date"); }});
}
```

`value` 与 `defaultValue`、`open` 与 `defaultOpen` 分别互斥。受控回调是请求，调用方不回写则保留原值。范围改变不会改写受控选择。`minDate`、`maxDate` 限制浏览及提交，`disabledDate` 只控制提交资格；禁用所有日期也不扫描寻找下一可用日期。`minDate > maxDate` 抛出异常。禁用规则应为无副作用的轻量谓词。

面板初始日期依次来自选择值、`defaultPanelDate`、运行时 **UTC 当前日期**。这里不提供本地时区“今天”服务；需要业务本地月份时由调用方提供 `defaultPanelDate`。面板保留浏览状态，不随每次打开重置。

支持 `allowClear`（默认 true）、`disabled`、`inputReadOnly`、`placeholder`、三种 `ControlSize`、四种 `InputVariant` 和三种 `InputStatus`、`DatePickerRef`（InputRef）、`autoFocus`、`onFocus/onBlur`、受控开合及 retained typed `DatePickerFooter`。`inputReadOnly` 只禁用手工编辑，仍可选择和清空。

接入 Form 时使用 `FormField<String>` 显式转换 ISO 字符串，并通过 `FormItemProps::control(DatePickerRef)` 连接真实控件。即使字段仍为空或与初值相同而没有值通知，聚焦期间直接 `FormRef.reset()` 也会取消 preedit、丢弃未提交日期草稿并恢复已接受值；不需要先失焦，不重建 Input/月历，也不发日期或开合请求。受控字段的 reset 是初值请求，外部拒绝回写时显示当前外部 authority。普通 validate、help、status、Theme 更新继续保留草稿与 preedit；没有 `control(ref)` 的自定义 slot 只重置字段值。

```cpp
void form_date_field(const ryn::FormRef& form, ryn::FormField<ryn::String> field,
                     const ryn::DatePickerRef& picker) {
    using namespace ryn;
    Form(FormProps{}.ref(form), FormContent{[field, picker] {
        FormItem(FormItemProps{}.field(String{u8"date"}, field).control(picker),
                 FormItemContent{[field, picker] {
            DatePicker(DatePickerProps{}
                .value(bind([field] { return parse_iso_date(field.get().bytes()); }))
                .defaultPanelDate(CivilDate{2026, 10, 1})
                .status(field.status()).ref(picker)
                .onChange([change = field.onChange()](DatePickerValue date) {
                    change(date ? format_iso_date(*date) : String{});
                }));
        }});
    }});
}
```

`DisabledDate` 是按共享谓词身份比较的轻量值，可直接传 lambda，也可使用 `Signal<DisabledDate>` 或 `bind(...)` 动态替换规则。更新范围和规则保留选择值；即使该选择现在不允许再次提交，也不会隐式回写调用方。禁用或隐藏控件会丢弃草稿，不把失活过程当作用户提交。

Theme 的 `date_picker` 提供 typed `colors/metrics/shadow/z_index_popup` 派生结果；使用 `ThemeConfig::date_picker.tokens` 覆盖颜色、面板/单元格尺寸、字体、阴影和层级。`date_picker.algorithm = true` 才应用该组件的局部 `seed`，默认继承父 Theme。颜色更新只失效材质；字体/度量更新重新布局，Input 与 footer 的真实节点及编辑会话保留。选择框沿用 Input 的变体/状态合同；DatePicker 的 selector 背景与 hover/active border 通过局部 Input Theme 接入。

未启用 `date_picker.algorithm` 时，局部 seed 完全忽略，也不会阻断父级 token 的继承。所有最终度量在派生及 override 后逐项校验；面板宽度、header/row/cell 高度及字体/行高至少为 1，padding/radius/gap 非负，全部有限。非法派生值会在 Theme snapshot 发布前拒绝；可以显式提供合法的最终 token override 修复局部 seed 派生出的度量。

```cpp
ryn::ThemeConfig calendar_theme() {
    ryn::ThemeConfig config;
    config.date_picker.tokens.selected = ryn::Color::rgba8(32, 90, 130);
    config.date_picker.tokens.cell_height = ryn::dp(28);
    return config;
}
```

Footer 内容只挂载一次，允许普通 Input、Button、TextArea 和拥有子 popup 的控件。日历 Escape 会恢复选择框焦点，子 popup 或当前编辑器的 IME 先处理自己的 Escape。外部点击、选择和 Escape 发出关闭请求；受控调用方拒绝普通关闭时保持打开。窗口失活、Modal/Spin 或隐藏祖先会独立 suppress 实际弹层，即使受控 `open` 仍为 true，恢复祖先也不会自动重开；显式打开动作或受控 false→true 才解除 suppression。

Footer 中的 Table 可使用 `scrollY` 提供有限滚动视口；滚轮先交给实际命中的内容控件及较高子 popup。内容到达滚动边界或命中日历 padding 时，日历会保留该滚轮，避免滚动被覆盖的 sibling 或弹层外祖先。日历外未被覆盖的内容维持原有滚动行为。

弹层遵循窗口层规则，脱离触发器外部布局及其内容裁剪，在窗口内定位；月历自身、footer 后代、实际 glyph/表面/命中使用面板内有效裁剪。窗口过短时只显示可用区域；没有滚动月历或缩放日期网格以容纳全部内容的隐式行为。缺少空间而得到零可用高度时不留下可见或可命中的日历内容。

月历固定六周、每周从周日开始。方向键按日/周移动，PageUp/PageDown 按月，Shift+PageUp/PageDown 按年，Home/End 到本月首末日，Enter 选择；关闭时 Down 打开。输入草稿 Enter 提交。Escape 关闭，Tab 正常离开，不创建 42 个 Tab stop。月历标题及工作日为固定数字/英文，首版没有 locale 配置。

首版不提供 RangePicker、多选、时间、周/月/季/年 picker、自由格式、locale/RTL、时区转换、预设、自定义 cell/panel、动画或辅助技术桥接；不声称完整覆盖 Ant Design DatePicker。Gallery、GPU、原生窗口/DPI、Linux 和系统 IME 的实际验收独立于本 change 的 Windows MSVC headless 合同测试。

## Gallery 与 Form String 适配

Gallery 的 DatePicker 目录进入 Form 之后的真实日期样例。受控选择初值固定为 `2026-10-12`，`defaultPanelDate` 固定为 `2026-10-01`，范围为 `2026-10-05` 至 `2026-11-25`，每月 13 日禁用。可通过月历、键盘和严格 ISO 输入选择，使用原生清空操作；footer 的备注 Input 在月历、Theme 和宽度更新后保留。

另一个独立 Form 使用已有 `FormField<String>` 保存日期，空值为 `String{}`。适配依赖公开解析/格式化函数，不向 FormValue 添加 CivilDate 类型：

```cpp
// date 为已配置 required/ISO/范围 validator 的 FormField<String>。
// 在 FormItem content 中执行，确保 change 捕获当前注册代际。
const auto change = date.onChange();
DatePicker(DatePickerProps{}
    .value(bind([date] { return parse_iso_date(date.get().bytes()); }))
    .onChange([change](DatePickerValue value) {
        change(value ? format_iso_date(*value) : String{});
    })
    .defaultPanelDate(CivilDate{2026, 10, 1})
    .status(date.status())
    .disabled(date.uneditable())
    .inputReadOnly(date.readOnly())
    .ref(date_ref));
```

外层 `FormItemProps{}.field(String{u8"date"}, date).control(date_ref)` 连接同一个真实 DatePickerRef。`inputReadOnly` 仅限制键入；Form 的只读语义通过 `date.uneditable()` 禁用整个选择器，保证月历和清空也不能改值。程序写入的非空 String 必须通过有效十字节 ISO、范围和禁用日校验；未提交或非法编辑草稿不进入成功结果。成功文字取实际 FormResult 的 String 快照，重置恢复空初值，禁用/只读恢复按钮位于 Form 外。

聚焦 Recording 入口为 `rynui_token_gallery_frame_tests --date-picker-only`，也接入完整 frame。阶段验收和平台边界由 [079](../openspec/changes/079-20261025-add-form-and-picker-gallery/tasks.md) 独立记录；本页不将组件 headless 或 Gallery Recording 结果视为原生 GPU、人工视觉、系统 IME、Linux 或辅助技术验收。
