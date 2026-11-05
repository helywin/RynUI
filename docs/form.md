# Form、FormItem、FormList 与 FormProvider

设计合同固定为 [Ant Design 6.6.5 Form](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/form/index.en-US.md)。RynUI 使用 typed Props、slots、reactive Prop 与原生输入/布局/焦点服务；不引入 React、DOM、CSS 或 HTML form 属性。

旧 String / optional<double> / bool / SelectSelection 的 field、value/onChange/control(ref)、同步结果形状及同值 reset 继续兼容。新增 SelectValues 与 optional<Decimal>；精确数字从编辑、规则到结果均不转换为 double。

## 兼容旧调用

下面的显式绑定仍可编译。onChange 必须在对应 FormItem content 中取得，回调捕获当前注册代际。

```cpp
#include <ryn/rynui.hpp>
#include <functional>

void DeviceForm(std::function<void(ryn::FormResult)> save) {
    using namespace ryn;
    FormRef form;
    FormField<String> name;
    FormField<SelectSelection> role;
    FormField<InputNumberValue> count{1.0};
    FormField<bool> consent;
    name.required();
    role.required();
    count.required().validator([](InputNumberValue value) {
        return value && *value < 1 ? String{u8"数量必须大于零"} : String{};
    });
    consent.required(String{u8"请先确认"});
    InputRef name_ref;
    SelectRef role_ref;
    InputRef count_ref;
    CheckboxRef consent_ref;

    Form(FormProps{}.ref(form).onFinish(std::move(save)), FormContent{[=] {
        FormItem(FormItemProps{}.field(String{u8"name"}, name)
                     .label(String{u8"名称"}).help(String{u8"设备显示名称"}).control(name_ref),
                 FormItemContent{[=] {
            Input(InputProps{}.value(name.value()).onChange(name.onChange())
                      .status(name.status()).disabled(name.disabled())
                      .readOnly(name.readOnly()).ref(name_ref));
        }});
        FormItem(FormItemProps{}.field(String{u8"role"}, role)
                     .label(String{u8"角色"}).control(role_ref), FormItemContent{[=] {
            Select(SelectProps{}.options({{SelectValue{u8"operator"}, String{u8"操作员"}}})
                       .value(role.value()).onChange(role.onChange()).status(role.status())
                       .disabled(role.uneditable()).ref(role_ref));
        }});
        FormItem(FormItemProps{}.field(String{u8"count"}, count)
                     .label(String{u8"数量"}).control(count_ref), FormItemContent{[=] {
            InputNumber(InputNumberProps{}.value(count.value()).onChange(count.onChange())
                            .status(count.status()).disabled(count.disabled())
                            .readOnly(count.readOnly()).ref(count_ref));
        }});
        FormItem(FormItemProps{}.field(String{u8"consent"}, consent)
                     .label(String{u8"确认"}).control(consent_ref), FormItemContent{[=] {
            Checkbox(CheckboxProps{}.checked(consent.value()).onChange(consent.onChange())
                         .disabled(consent.uneditable()).ref(consent_ref),
                     CheckboxLabel{[] { Text(u8"同意保存"); }});
        }});
        Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submit()); }),
               ButtonContent{[] { Text(u8"保存"); }});
        Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }),
               ButtonContent{[] { Text(u8"重置"); }});
    }});
}
```

## 原生自动绑定与自定义 adapter

命名 FormItem 的 control slot 中，首个兼容的 Input、TextArea、InputNumber、Select、Checkbox 或 Switch 自动接入 typed value/change、disabled/readOnly、status、ref 与表单上下文。InputNumber 根据 field 类型选择 double 或 exact decimal；SelectValues 对应 Multiple/Tags。已有显式 value/onChange 接线保持其权威，并占用首控件位置；后续兄弟控件不自动接线。Mentions、AutoComplete、日期时间、Cascader、Transfer、Upload 与 ColorPicker 的私有输入/选择控件不占用外层 Item 的自动位置或 Enter；popup 内建立的独立 FormItem 保留自己的绑定作用域。嵌套 FormItem 建立自己的控件位置；嵌套 Form 建立新的表单作用域。

```cpp
void AutomaticForm() {
    using namespace ryn;
    FormRef form;
    FormField<String> name;
    FormField<InputNumberStringValue> amount{Decimal{"9007199254740993.0001"}};
    name.required();
    Form(FormProps{}.ref(form).layout(FormLayout::Horizontal), FormContent{[=] {
        FormItem(FormItemProps{}.field(String{u8"name"}, name).label(String{u8"Name"}),
                 FormItemContent{[] { Input(InputProps{}); }});
        FormItem(FormItemProps{}.field(String{u8"amount"}, amount).label(String{u8"Amount"}),
                 FormItemContent{[] { InputNumber(InputNumberProps{}); }});
    }});
}
```

`FormControl(Props)` 明确声明该 slot 的控件所有权。自定义复合控件使用 `FormControl(FormControlAdapter<T>{mount})`，mount 接收 `FormControlBinding<T>`，从其 value/onChange/status/disabled/readOnly/presentation/onBlur 取得 typed 接线，挂载后用 `binding.control(ref)` 或 `FormControlTarget` 注册真实焦点、reset 和 geometry。adapter 仅能接入本 FormItem。没有适配器的任意自定义控件不会通过运行时反射或属性注入自动工作。

## 布局、呈现和上下文

Form 保持兼容的 Vertical 默认值；`layout(FormLayout::Horizontal/Inline/Vertical)` 控制真实布局，`layout(LayoutStyle)` 继续仅控制外部尺寸、margin 和 flex。Horizontal 使用 24 列 label/wrapper span/offset，Inline 使用原生换行 Flex。`labelCol`、`wrapperCol` 接受 `Responsive<FormColumn>`，断点按窗口宽度解析。`labelAlign`、`labelWrap`、`colon`、requiredMark、direction 控制实际文字、对齐、换行、标记及 RTL 几何。

`FormPresentation` 汇集布局、ControlSize、InputVariant、TextDirection 和 FormLocale。`presentation(Prop<FormPresentation>)` 支持运行中更新，传播到 retained Item 和自动控件，普通更新不重新执行 slot。Item 可以覆盖布局/列/标签配置，或提供自己的 presentation。嵌套 Form 的字段、布局和命令独立；Provider 只协调其作用域内具名表单，不合并字段。

FormItem 支持无字段的布局包装、noStyle、hidden、extra、help、validateStatus、hasFeedback、tooltip 与视觉 required。labelContent/helpContent/extraContent/feedbackIcon 提供独立 typed 富内容 slots，保留子控件草稿与身份。hidden 字段保留采值/校验，但不参与布局或输入；noStyle 不绘制标签/反馈间距。错误优先于兼容 help；extra 独立呈现。Text、Tooltip、错误和额外信息都使用保留组件。ThemeConfig.form 控制表单视觉；控件继续使用自身 Theme token。

## 深路径、结构化值和稳定列表

`FormNamePath{u8"people", 0, u8"name"}` 显式表示字符串/数组下标路径。旧 `field(String{u8"people.0.name"}, field)` 永远是字面平面键。重复、父子冲突、非法下标与不匹配的类型在发布前拒绝。`FormResult.values/errors` 保留旧成员并附带 path，warnings 与 structure 是附加信息；`structuredValues()` 返回拥有数据的对象/数组快照，包括空列表。

`FormRef` 提供 getFieldValue/getFieldsValue、setFieldValue/setFieldsValue、resetFields、getFields/setFields、getFieldsError 和 watch。getFieldInstance 返回可选的 typed FormControlTarget（focus/reset/geometry），捕获注册代际，不暴露 DOM 或无类型对象。未挂载的 typed 路径也保存在 store，getFieldValue/getFieldsValue(true) 可读取，watch 可订阅，后续挂载采用已存值；默认 getFieldsValue 只返回已注册字段和 List。批量结构化更新先检查已注册叶类型；数组更新通过列表操作同步形状并保留未变行。应用回调可能产生外部副作用；回调或自定义行 renderer 抛异常时，无法安全继续的完整 Form UI/List 会退役，避免发布部分事务，其他组件继续可用。

```cpp
void PeopleForm() {
    using namespace ryn;
    FormRef form;
    FormListRef people;
    Form(FormProps{}.ref(form), FormContent{[=] {
        FormList(FormListProps{}.name(FormNamePath{u8"people"}).ref(people)
                     .initialValue({FormData{}, FormData{}}),
                 [](FormListField row) {
            FormField<String> name;
            FormItem(FormItemProps{}.field(FormNamePath{u8"name"}, name)
                         .label(bind([row] {
                             return String::from_utf8("Person " + std::to_string(row.index() + 1)).value();
                         })),
                     FormItemContent{[] { Input(InputProps{}); }});
        });
        Button(ButtonProps{}.onClick([=] { static_cast<void>(people.add()); }),
               ButtonContent{[] { Text(u8"Add person"); }});
    }});
}
```

列表内显式路径相对当前行；嵌套列表自动组合，旧 String overload 仍为根级平面身份。row.key 稳定，index/position 随移动更新。add/remove/move 只挂载新增行、销毁删除行；保留其他行的 Input、草稿、preedit、焦点和 callback 身份。删除后旧注册回调及 ref/editor/session 失效；重复 remove 下标和越界操作整体拒绝。FormList 支持同步 rules 和可取消 asyncValidator；`list.meta()` 提供 errors/warnings/validating，`FormErrorList` 用于原生反馈呈现。

初值优先级为 Form.initialValues、Item.initialValue、field 构造初值；列表自身 initialValue 提供行默认值。reset 恢复初始列表形状和字段，清除错误、touched 和 pending，不重建保留的普通控件。Form/Item.preserve(false) 在字段移除时放弃内部值；Form.clearOnDestroy(true) 清除销毁时保留的内部值。受控 FormField 的 source 是最终 authority；用户编辑和 reset 发请求，拒绝回写不会伪造值。`control(ref)` 的同值 reset 继续取消 preedit、清理未完成数字/Select/Date/Time 草稿。

## 规则、异步和状态

FormRule 支持 required、Unicode whitespace/长度、String/Number/Boolean/Array 类型、枚举、pattern、transform、精确 Decimal 范围、自定义跨字段 validator 与 warning-only。规则可以汇总，也可 validate-first。`FormValidationContext.values/get(path)` 是事务快照，跨字段读取不会混用新旧值。

`field.validation(trigger, debounce_ms, dependencies)` 支持 Change/Blur/ChangeAndBlur；旧字段默认 Manual，保持同步调用时机。onBlur 同 onChange 一样捕获注册代际。依赖路径是当前 Form 内的显式完整路径。Debounce 使用宿主单调时钟与下一截止时间，空闲时不持续请求帧。

`asyncValidator(FormAsyncValidator<T>, OwnerThreadPost)` 接受应用服务及 owner-thread dispatcher；Core 不进行网络调用。请求提供 snapshot/generation/cancelled；worker 只调用 completion，由 dispatcher 投递到创建线程。旧、重复及错误线程结果不能写 Signal。应用 dispatcher 应无异常地接受任务并唤醒 owner event loop；不返回/错误投递的服务有 30 秒有界取消，避免永久 validating。

`validateAsync(callback, options)` 支持名称、recursive、dirty 和 validate-only；`submitAsync()` 提交当前快照。只有同步规则时，旧 validate/submit 的结果和异常传播继续有效；存在 async rule 时 submit 非阻塞发起事务，旧 validate 返回空 optional，应使用 async 命令。cancelValidation、改值、移除、reset、隐藏/输入边界变化和销毁撤销旧发布权。存活的过期提交进入 onFinishFailed 且 out_of_date=true，销毁后不回调。

meta/getFields/setFields 暴露 touched/validating/errors/warnings/out_of_date；getFieldError/getFieldWarning、isField(s)Touched/isField(s)Validating 查询状态。useFormInstance 取得当前 FormRef，useWatch(path, form)/watch(path) 返回可读取的 Binding<optional<FormValue>>，精确追踪对应字段，并跟随 FormRef 挂载/解绑；useFormStatus 返回 Binding<FormItemStatus>，包含当前 Item status/errors/warnings。两者均可直接传给 Prop，或在 reactive effect 中调用 get()。无关字段更新不重新执行观察者或组件 slot。

## Enter、滚动和 Provider

首个单行原生编辑器的普通 Enter 提交其所属 Form；TextArea 保留换行，IME composition 先消费 Enter。嵌套表单提交最接近的作用域；隐藏、disabled、zero-bounds、阻挡及非活动窗口不获得原生提交资格。显式控件 onSubmit 可保留自己的提交行为。

`scrollTarget(FormScrollTarget)` 注入原生滚动服务。scrollToField/scrollToFirstError 传递捕获 ref 对应控件的真实 bounds/clip，服务 reveal 后 acknowledge；Form 在下次同步几何后检查存活/代际/事务、输入资格和实际可见性，再聚焦。部分裁剪可聚焦，完全裁剪、隐藏、阻挡和零尺寸不聚焦。reset/移动/删除后旧 acknowledge 不能作用到其他字段。scrollToFirstError(true) 可用于失败提交。

`FormProvider` 提供具名 onFormChange/onFormFinish 与同作用域 forms/ref 快照；change 事件的 changed_fields 包含 typed 值与 meta，result.structure 保留完整值快照；重复具名注册拒绝。嵌套 Provider 隔离注册和事件。用户编辑触发 onValuesChange/onFieldsChange，程序 set/reset 保持显式命令行为。

## 上游 API 的原生映射与验证范围

| 上游概念 | RynUI 原生 API |
| --- | --- |
| Form/useForm/useFormInstance | FormProps、可复制 FormRef、useFormInstance |
| Item/NamePath/FieldData | FormItemProps、FormNamePath、FormFieldData |
| List 操作与 meta | FormListProps/FormListRef、FormListField 稳定 key/index、FormFieldMeta |
| rules/validateFields/Promise | FormRule、typed asyncValidator、validateAsync + FormValidateOptions |
| watch/useStatus | watch/useWatch、useFormStatus、细粒度 Signal |
| 自定义 valuePropName/getValueFromEvent/normalize | typed FormControlAdapter<T>；显式 C++ 值/事件转换 |
| shouldUpdate/render props | typed slots、Prop/Signal；不重跑无关组件 |
| label/wrapper Col | Responsive<FormColumn>，真实原生布局 |
| scrollToField | 显式 FormScrollTarget 与捕获控件几何 |
| Provider/ErrorList | FormProviderProps/事件、FormErrorListProps |
| DOM/CSS/HTML/React 专用接口 | 不提供同名无效别名；由原生 typed API 表达相关语义 |

Gallery 保留旧流程，新增布局/自动绑定/精确数字/滚动、深嵌套列表、合成异步/依赖三个独立实际示例。提交文字来自实际 FormResult。合成异步交付不是服务器或网络证据。

HEADLESS/Recording 合同与 Gallery 测试不代表实际 GPU、系统 IME candidate UI、辅助技术或 Windows/MSVC 验收。Linux 原生分类验收和 Windows 平台任务由父线程独立收口；13.* 只有完整受影响自动检查通过后才勾选。证据见 change107 的 form-linux.md。

主线组合与 Linux 私有 Wayland 的实际结果见 [Form 集成验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/form-integration.md)。
