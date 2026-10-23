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
