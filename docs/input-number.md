# InputNumber

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

首版不提供 stringMode、任意精度、formatter/parser 服务、wheel stepping、spinner 模式或长按连发。

文本语法为 ASCII 数字、可选正负号和 `.` 小数点；不解析指数、分组符或本地化小数点。此类草稿暂存并在提交时恢复权威值。格式化数值不补齐尾零；precision 表达最大小数位数。

步进先计算十进制候选，再按 precision 最近舍入；如果舍入会跨过当前权威值并反向，则沿请求方向量化。边界夹取也不得让非空值反向；该方向没有合法值时保持原值，不发 change/step。这同样适用于外部 value、precision 或范围动态变更后尚未位于精度网格的值；空值仍按 0 起步再夹取到范围。

preedit 的进入和取消按编辑器 composition 状态刷新按钮资格，包括 ref selection、focus cursor、pointer selection、Escape、commit 及资格变更；取消不发布预编辑文本。真实 disabled/readOnly 与共同逻辑输入门禁继续独立生效。

嵌入 `Spin` 时，loading 阻止数值按钮、键盘和 ref focus/select，并结束当前编辑 session、取消 preedit；迟到的旧 session 事件被拒绝。loading 结束后仍遵守控件自身的 disabled/readOnly 和新 composition 状态，保留数值、编辑器与按钮 identity。

控件直接挂载现有 Input：复用唯一 TextEditorState、TextInputSessionHost、selection/clipboard/history、`InputRef`（focus/blur/select）、placeholder、status、disabled/readOnly，以及 `InputPrefix`/`InputSuffix` typed slots。三种 `ControlSize` 和 Outlined/Filled/Borderless/Underlined 继承 Input 表面 Theme。Up/Down 步进，`.keyboard(false)` 可关闭；`.controls(false)` 关闭按钮但保留键盘。按钮保持编辑器焦点，preedit 期间不解析或步进，只有 TextCommitted 进入数值处理。默认滚轮只经过共同滚动路径。

`onStep(double, InputNumberStepInfo)` 提供候选值、正数 step magnitude、Up/Down direction 和 Button/Keyboard source；边界或舍入后无变化不通知。`onSubmit(InputNumberValue)` 在 Enter 规范化后提供权威值。控件移除时清理 ref、焦点、session、捕获及两个按钮，销毁期间不提交草稿。

`ThemeConfig::input_number.tokens` 提供 `control_width`（90）、`handle_width`（默认 controlHeightSM - 2*lineWidth = 22）、`handle_font_size`（默认 fontSize/2 = 7），以及 handle 背景、active/filled 背景、hover/普通/disabled 字色和边框色。长度为正且不超过 10000 logical px；颜色与 metrics 分组失效，支持嵌套继承、`inherit=false`、Dark/Compact 和独立 component algorithm。表面视觉继续由 `ThemeConfig::input` 控制；`LayoutStyle` 仅控制外部布局。首版按钮常显并在 Input affix 内上下排列；不实现上游 hover reveal 动画。

设计参考固定到 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 input-number 组件、文档、style 与 token 源码。此功能只增加 logical CPU scene 中的既有 Quad/Icon，不改变 scene v3、GPU ABI v2 或 backend。

验证范围为 MSVC headless 的真实编辑/焦点/指针/键盘路由和注入的 composition/commit 合同；这不构成真实 GPU 窗口、系统 IME、Linux 或人工视觉验收。
