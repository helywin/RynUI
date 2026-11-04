# InputNumber

Token Gallery 的 LiveSamples 末尾提供受控小数、禁用切换、只读与 InputRef 聚焦，并在 Table 单元格内直接编辑业务数值。排序、翻页和同 key 数据更新保留编辑器身份。

`<ryn/input_number.hpp>` 提供允许空值的 `InputNumber`。`InputNumberValue` 为 `optional<double>`；`.value()` 固定受控模式，`.defaultValue()` 仅初始化非受控模式，两者互斥。typed `onChange(InputNumberValue)` 接收候选值；受控调用方需要回写 Signal 才接受。

```cpp
auto amount = ryn::state(ryn::InputNumberValue{1.2});
ryn::InputNumber(ryn::InputNumberProps{}
                    .value(amount)
                    .min(0)
                    .max(100)
                    .step(0.1)
                    .precision(2)
                    .onChange([=](ryn::InputNumberValue next) { amount.set(next); }));
```

数值必须有限，绝对值不大于 `9007199254740991`，最短十进制表示最多 12 位小数。precision 可省略，或为 0..12；舍入使用半值远离零，范围向内量化。范围/step/precision 先完整校验再应用，非法值抛出 `invalid_argument`。步进内部以整数和小数两段做十进制运算，再在 API 边界转为 double。

编辑保留空白、符号及尾点草稿。完整且范围内的数值发布 change，空文本发布 `nullopt`；非法或未完成文本暂不发布。Enter/blur 规范化尾点、舍入并夹取边界；无法解析则恢复权威值。受控拒绝候选后，草稿可保留到 Enter/blur，随后恢复外部值。外部回写相同候选保留编辑中的文本形式；动态 min/max 不自动修改数值或发 change。

精确模式使用 `Decimal` 与 `InputNumberStringValue = optional<Decimal>`。`.value(Prop<InputNumberStringValue>)` / `.defaultValue(Decimal)` 自动选择 stringMode；可显式 `.stringMode()` 初始化空值。`.min/.max/.step(Prop<Decimal>)` 全程按十进制运算；typed `onStringChange/onStringStep/onStringSubmit` 与旧 double 回调分离，避免泛型回调发生歧义。旧 double API 和签名保持兼容；不得把 double value 与 exact value 混合使用。

```cpp
auto amount = ryn::state(ryn::InputNumberStringValue{ryn::Decimal{"9007199254740993.00000000000000001"}});
ryn::InputNumber(ryn::InputNumberProps{}.value(amount).step(ryn::Decimal{"1e-17"})
    .onStringChange([amount](ryn::InputNumberStringValue next) { amount.set(next); }));
```

Decimal 支持符号、小数、指数与任意精度十进制加减/比较/量化，不经过 double。显式上限为 16384 位十进制数字或小数位；超过上限拒绝，不截断。尾零规范化为最短精确表示，draft 在编辑时保留尾零；precision 为最大位数，半值远离零，负边界向内量化。exact 模式可用指数输入；旧 double 模式维持原来的语法和范围。

`formatter(function<String(String,InputNumberFormatInfo)>)` 接收精确数值文本和 `user_typing/input` 上下文；input 是实际编辑草稿（包括 user_typing=false 的提交/程序更新格式化）；`parser(function<String(String)>)` 返回标准 ASCII 十进制文本。调用方可保留 typing draft，或处理分组符/货币。`.decimalSeparator()` 提供默认 parser 的小数点替换；custom parser 自行负责转换。格式化不会经过 double，异常会沿现有输入事务传播。`onInput` 接收实际 draft；`changeOnBlur(false)` 保留 blur 草稿，Enter 仍提交。

`.changeOnWheel(true)` 在编辑器已聚焦、keyboard 开启且可编辑时允许 wheel。纵向正/负（包括小数 delta）各步进一次；横向、禁用、只读、IME、未聚焦、keyboard=false 及范围无变化不消费，继续共同 Gallery/input-first 路由。按钮按下立即步进，600ms 后每 100ms 单次连发，不补发停顿期间的积压事件；抬起/取消/隐藏/禁用/失焦/卸载清理捕获和 deadline。

`.mode(InputNumberMode::Spinner)` 使用左右独立减/加按钮，默认 Input 模式保留上下按钮；mode 可响应更新。`controls(InputNumberControls)` 支持 typed up_icon/down_icon，与既有 InputPrefix/InputSuffix 一同保留。`.direction()` 传递文本方向。

文本语法为 ASCII 数字、可选正负号和 `.` 小数点；不解析指数、分组符或本地化小数点。此类草稿暂存并在提交时恢复权威值。格式化数值不补齐尾零；precision 表达最大小数位数。

步进先计算十进制候选，再按 precision 最近舍入；如果舍入会跨过当前权威值并反向，则沿请求方向量化。边界夹取也不得让非空值反向；该方向没有合法值时保持原值，不发 change/step。这同样适用于外部 value、precision 或范围动态变更后尚未位于精度网格的值；空值仍按 0 起步再夹取到范围。

preedit 的进入和取消按编辑器 composition 状态刷新按钮资格，包括 ref selection、focus cursor、pointer selection、Escape、commit 及资格变更；取消不发布预编辑文本。真实 disabled/readOnly 与共同逻辑输入门禁继续独立生效。

嵌入 `Spin` 时，loading 阻止数值按钮、键盘和 ref focus/select，并结束当前编辑 session、取消 preedit；迟到的旧 session 事件被拒绝。loading 结束后仍遵守控件自身的 disabled/readOnly 和新 composition 状态，保留数值、编辑器与按钮 identity。

控件直接挂载现有 Input：复用唯一 TextEditorState、TextInputSessionHost、selection/clipboard/history、`InputRef`（focus/blur/select）、placeholder、status、disabled/readOnly，以及 `InputPrefix`/`InputSuffix` typed slots。三种 `ControlSize` 和 Outlined/Filled/Borderless/Underlined 继承 Input 表面 Theme。Up/Down 步进，`.keyboard(false)` 可关闭；`.controls(false)` 关闭按钮但保留键盘。按钮保持编辑器焦点，preedit 期间不解析或步进，只有 TextCommitted 进入数值处理。默认滚轮只经过共同滚动路径。

`onStep(double, InputNumberStepInfo)` 提供候选值、正数 step magnitude、Up/Down direction 和 Button/Keyboard/Wheel source；边界或舍入后无变化不通知。`onSubmit(InputNumberValue)` 在 Enter 规范化后提供权威值。控件移除时清理 ref、焦点、session、捕获及两个按钮，销毁期间不提交草稿。

`ThemeConfig::input_number.tokens` 提供 `control_width`（90）、`handle_width`（默认 controlHeightSM - 2*lineWidth = 22）、`handle_font_size`（默认 fontSize/2 = 7），以及 handle 背景、active/filled 背景、hover/普通/disabled 字色和边框色。长度为正且不超过 10000 logical px；颜色与 metrics 分组失效，支持嵌套继承、`inherit=false`、Dark/Compact 和独立 component algorithm。表面视觉继续由 `ThemeConfig::input` 控制；`LayoutStyle` 仅控制外部布局。Input 模式按钮常显并在 Input affix 内上下排列；不实现上游 hover reveal 动画。

设计参考固定到 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 input-number 组件、文档、style 与 token 源码。此功能只增加 logical CPU scene 中的既有 Quad/Icon，不改变 scene v3、GPU ABI v2 或 backend。

068/072 的历史 MSVC headless 编辑/焦点/指针/键盘路由及注入 composition/commit 证据保留；107 的平台通用合同在 Linux GCC HEADLESS 执行，后续 Linux 原生有限覆盖独立记录，不扩大系统 IME 或历史 Windows 证据。

change 107 的 Linux GCC HEADLESS 结果见 [InputNumber 验证](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/input-number-linux.md)。平台通用合同不等同真实 GPU/系统 IME 或 Windows 验收。

Linux 私有 Wayland/Vulkan 的精确小数与 formatter/parser、长按/wheel、input-area/cursor 和离页清理已通过 [主分支组合验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/color-number-integration.md)；系统 IME 候选 UI、跨实际显示器/DPI、AT 与新增 Windows 行为仍未验证。
