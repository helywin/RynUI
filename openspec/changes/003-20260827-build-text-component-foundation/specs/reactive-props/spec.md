## Purpose

定义 RynUI 公开 typed Props 的响应式值边界，使组件字段可以统一接受静态值、Signal 或 Binding，同时保持确定的所有权、Scope 清理和最小失效行为。

## ADDED Requirements

### Requirement: Prop 统一表达静态值与响应式值
系统 SHALL 提供公开 `Prop<T>`，使 typed Props 字段可以由静态 `T`、`Signal<T>` 或返回 `T` 的 `Binding<T>` 构造；三种来源 MUST 保持同一字段语义，不得要求组件为不同来源维护多套 API。

#### Scenario: 静态值不建立响应订阅
- **WHEN** 调用方用静态 `T` 构造组件字段并挂载组件
- **THEN** 组件读取该值完成初始状态，且不会为该字段建立响应 Observer

#### Scenario: Signal 与 Binding 驱动同一字段
- **WHEN** 两个组件分别使用 `Signal<T>` 与 `Binding<T>` 提供同一类 Props 字段
- **THEN** 两者都通过该字段的统一更新路径改变目标属性，并保持相同验证与失效规则

### Requirement: Prop 拥有安全的来源生命周期
`Prop<T>` MUST 按值保存静态值或响应式来源所需的安全句柄，使从临时 `Binding<T>` 或可复制 `Signal<T>` 构造的 Props 在组件挂载后仍可读取；不得保存指向调用栈临时对象的裸引用。

#### Scenario: 临时 Binding 在挂载后仍有效
- **WHEN** 调用方以内联创建的临时 `Binding<T>` 构造 Props 并完成组件挂载
- **THEN** 后续依赖变化仍能安全计算字段值，不访问已经销毁的临时对象

#### Scenario: Props 临时对象不限制组件寿命
- **WHEN** 调用方用临时 typed Props 挂载组件且该 Props 表达响应式来源
- **THEN** 组件在其 Scope 存活期间继续安全接收更新，不依赖原 Props 对象地址

### Requirement: Prop 订阅服从组件 Scope
响应式 `Prop<T>` MUST 在拥有组件的 `Scope` 中建立订阅；Scope dispose 或组件销毁后 MUST 停止字段更新并释放 Observer，已排队的旧组件更新不得作用于复用后的对象。

#### Scenario: 组件销毁后不再更新
- **WHEN** 响应式 Props 所属组件被销毁后其 Signal 再次写入
- **THEN** 该组件的字段回调不再执行，Node、Text state、Scene range 和 frame request 均保持不变

#### Scenario: 复用 slot 不接收旧更新
- **WHEN** 旧组件销毁后内部 slot 被新组件复用，并发生源自旧 Props 的延迟更新
- **THEN** 更新因 generation 或等价生命周期校验被丢弃，不修改新组件

### Requirement: 相同值不得扩大失效
当响应式来源重新计算为与当前字段值相等的 `T` 时，系统 MUST 保持该组件的下游属性、布局、Scene 和 frame request 不变；不同字段的更新不得重新执行无关组件函数。

#### Scenario: 相同内容重复发射
- **WHEN** `Binding<T>` 的依赖变化但计算结果与组件当前字段值相等
- **THEN** 目标属性不产生新的 Dirty 标记，不请求额外帧

#### Scenario: 一个组件字段变化
- **WHEN** 一个已挂载组件的响应式 Props 字段产生不同值
- **THEN** 只执行该字段的更新适配器和其声明的最小失效阶段，不重新挂载兄弟组件

### Requirement: Prop 公共边界保持平台无关
`Prop<T>` 及其公开适配接口 MUST 只依赖 RynUI 公开类型和 C++20 标准库，不得暴露内部 Observer、Node、Layout、FreeType、HarfBuzz 或 SDL3 类型。

#### Scenario: 独立包含 Prop 头文件
- **WHEN** consumer target 只包含公开 Prop header 并构造静态、Signal 与 Binding 三类值
- **THEN** target 无需包含内部目录或第三方 header 即可编译和链接
