# Form 与 FormItem

原生首版提供纵向表单：typed 字段、显式控件绑定、同步校验、错误反馈，以及 FormRef 提交/重置。视觉参考固定为 Ant Design 6.6.5；不依赖 Web 表单或 React。

下面的函数在正常 RynUI 窗口 content 中调用。窗口需要已配置既有 Input 与 Selection host；不需要额外创建 Form host。

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

`field(name, handle)` 负责注册；`.value()` / `.onChange()` 负责真实采值。Checkbox 用 `.checked()`。FormItem 不改写任意 slot 的 props；未显式绑定的控件不会加入结果，也不会自动继承 disabled/readOnly/status。`onChange()` 必须在 FormItem 的 content 中取得，它绑定当前注册代际；卸载后旧回调失效。

`control(ref)` 捕获本次挂载的真实 ref，用于失败焦点及 Input reset；控件重新绑定同一个 ref 后，旧 FormItem 不会操作新控件。失败聚焦检查捕获代际对应控件在同一窗口中的实际 bounds、祖先 clip 与输入资格；全裁剪或零尺寸控件不会因 FormItem 的全宽 wrapper 可见而获得焦点，部分可见仍可聚焦。替换控件时应同时重新挂载 FormItem。也可用 `.focus(function<bool()>)` 接入自定义控件；返回 false 会继续尝试后续无效字段。自定义 focus 回调须自行检查真实目标的资格及可见性，Form 只能检查其 control region；未提供 control 时 reset 只恢复 typed 值。

## 值、验证与命令

- `FormField<T>` 支持 `String`、`std::optional<double>`（即 InputNumberValue）、`bool` 和 `SelectSelection`。复制 handle 保留字段身份；字符串 name 为唯一平面标识，不解析点号或深路径。数字必须有限。
- 初值在创建 field 时固定。`get()` 读当前 authority，`set(value)` 程序写入；`.required(message)` 和 `.validator(function<String(T)>)` 在注册前配置。空错误字符串代表通过。
- required 检查空串、空 optional，bool 要求 true；空 SelectSelection 无效。不自动 trim，不增加 whitespace、range、异步或跨字段依赖规则。
- `validate()` 返回 `optional<FormResult>`，更新错误但不触发提交回调或改变焦点。值改变只清除自身错误；不自动重新验证。custom validator 异常向调用者传播，不发布部分错误。
- `submit()` 同步校验后进入 `onFinish(FormResult)` 或 `onFinishFailed(FormResult)`。返回 true 表示命令被接受，包含验证失败；拒绝或执行中销毁返回 false。结果拥有 values/errors 快照，按当前注册顺序排列；`valid()` 同时检查 errors 和 `out_of_date`。
- 验证期间改值、增删字段、切换输入边界会使事务过期；不发布旧成功结果。错误发布或聚焦回调中的改值也会清除本事务已发布的错误。仍存活的过期事务进入失败回调。validate/submit/reset 的递归调用被拒绝。
- `reset()` 清除错误并恢复初值，不重建控件。InputNumber 即使数值未变，也会通过 `control(ref)` 丢弃未完成草稿并格式化为当前 authority；Input/TextArea 的 preedit 会取消。普通 validate、help、error、Theme 更新保留草稿与 preedit。真实失焦仍遵守既有控件的格式化行为。
- 移除 FormItem 后结果不含该字段、错误清除；handle 保留值，再注册取得新代际。Form 销毁后 held FormRef 的命令无操作，`bound()` 为 false。

受控 field 使用 `FormField<T>{reset_initial, Prop<T>, onChange}`。例如 `FormField<String>{String{}, external_signal, [=](String value) { external_signal.set(value); }}`。用户编辑和 reset 都发请求，应用是否回写决定最终值；拒绝回写时不伪造新 authority，reset 的数字显示也服从外部值。外部值变化不再次调用 onChange。

## 交互与布局

Form/FormItem 的 `disabled(Prop<bool>)` 和 `readOnly(Prop<bool>)` 通过 field 适配属性显式连接。Input/InputNumber 使用 disabled 与 readOnly；Select/Checkbox 没有 readOnly，使用 `uneditable()` 连接 disabled。disabled 的 Form 拒绝 validate/submit；readOnly 允许提交。程序 set/reset 不受编辑 gate 限制。

Form 被 Modal 排除、Spin 阻止或祖先隐藏时，validate/submit 不执行。已注册但隐藏/禁用的字段保留值并参与校验，提交失败只尝试 branch/input boundary 允许且控件区域实际可见的无效字段。首版不自动滚动；可用 `.focusFirstInvalid(false)` 关闭失败聚焦。没有窗口 Enter filter，不将 TextArea 换行或 IME Enter 解释为提交；程序和原生 Button 都调用同一 FormRef。

标签、必填标记、帮助及错误使用 retained Text，错误优先于 help。空 label/help/error 不占布局流；宽度与祖先 clip 使用实际节点约束。Form token 只影响表单标签/反馈及间距，既有控件继续使用自己的 Theme token。外部尺寸、margin、flex 等使用 LayoutStyle。

`ThemeConfig::form` 支持 colors（label、required、help、error）及 metrics（item_margin_bottom、label_gap、required_gap、font_size、line_height、font_family）typed override；支持继承、inherit=false 与独立 component algorithm/seed。最终派生的 font_size 必须有限且至少为 0.5 logical dp，保证 Text 在 DPI 换算前取整后仍可表示；非法值在 Theme 发布前拒绝。纯颜色更新只失效 material，metrics 更新相应文字/测量/布局，均不重执行控件 slot。

`form.algorithm=false` 时局部 `form.seed` 不生效，也不阻断父级 Form token 覆盖；子级显式 token override 仍叠加在继承值上。开启组件算法才使用局部 seed，关闭后恢复正常继承。

首版不含嵌套 Form、横向/inline/Grid label 布局、Form.List/数组、JSON schema、深路径、自动任意控件采值、async/debounce/dependencies、自动 Enter/滚动、Web DOM/CSS/HTML required、完整上游 API 或平台无障碍 adapter。本次 headless 验证不代表 Windows/Linux 原生 GPU、系统 IME、系统字体或人工视觉已验收。实现证据见 [075](../openspec/changes/075-20261024-add-native-form/evidence/platform-generic.md)；Gallery/support overlay 由父线程单独整合。
