# Util 与 C++ 类型工具

锁定的 Ant Design 6.6.5 `components/_util/index.en-US.md` 公开介绍 `GetRef`、`GetProps`、`GetProp` 三种 TypeScript 类型工具。它们在编译期提取类型，没有运行时控件、视觉状态或挂载生命周期。Gallery 的 Util 独立说明页保留这个来源，标记为 `web-only`，不注册虚构的可运行示例。

RynUI 使用已经公开的 C++ 类型表达这些需求：

| 上游工具 | RynUI / C++ 对应方式 |
| --- | --- |
| `GetRef<typeof Select>` | 直接使用公开的 `ryn::SelectRef`、`ryn::InputRef` 等 ref 类型。 |
| `GetProps<typeof Select>` | 直接使用公开的 `ryn::SelectProps`、`ryn::InputProps` 和相应 typed slots。 |
| `GetProp<SelectProps, 'options'>` | 使用对应的公开值类型，例如 `ryn::SelectOption`、`ryn::SelectValue`；局部表达式可用 `decltype` 推导类型。 |
| `GetProp<Props, 'callback', 'Return'>` | 对已有可调用对象使用 `std::invoke_result_t`；可选值处理由 `std::optional` 明确表达。 |

例如，应用可以直接声明 `ryn::SelectRef reference;` 和 `std::vector<ryn::SelectOption> options;`，再通过 `SelectProps{}.ref(reference).options(options)` 组合它们。无需从组件函数或 Props 的私有存储中反射字段。

React Context 的类型提取、TypeScript 的 `NonNullable` 和函数联合类型规则属于上游语言合同。RynUI 不提供同名兼容别名，也不将它们解释为尚缺的原生视觉组件。上下文的运行时传播由实际 Provider 合同承担；本页不据此声明 ConfigProvider 或 i18n 已实现。
