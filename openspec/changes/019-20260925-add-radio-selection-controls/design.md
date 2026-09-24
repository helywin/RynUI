# Design

## Context

详见 `proposal.md`。`SelectionComponentHost` 已为 Switch/Checkbox 借用 `WindowComponentServices`、`PressableBehavior`、焦点与 retained surface；现有状态模型以 `checkbox` 标志区分外观和激活，需要为 Radio 明确第三种语义。参考源锁定在仓库的 Ant Design 6.6.5 快照，Radio 使用圆环与圆点，Group 使用值比较抑制重复变更。

## Goals / Non-Goals

**Goals:** Radio 和 RadioGroup 的状态与视觉变化不复制新的窗口服务；Group 只保留一个选中值；颜色修改保持局部材质失效；标签可参与整个 Radio 的命中。

**Non-Goals:** `Radio.Button`、动态 options 增删、任意值类型、平台原生 HTML radio 行为、自定义颜色入口；这些需要独立设计。首版值类型采用经验证的 UTF-8 `ryn::String`。

## Decisions

1. **在 Selection 宿主中增加 kind，而非创建 Radio 私有宿主。** 这沿用已经验证的生命周期、指针取消和 surface 所有权。Switch/Checkbox 现有 API 与行为不变；Radio 的激活策略单独判断，只允许 false→true。备选单独宿主会复制交互与场景清理路径。
2. **Group 拥有选中值，选项仍是普通 Radio。** Group 通过 typed options 构造 Radio 子组件，记录其组件 ID，并在值更新时只修改 checked 材质。受控 Group 发出请求后等待 `Prop<std::optional<String>>` 回写；未受控 Group 同步修改当前值。重复选中被忽略。备选让每个 Radio 独立持有 checked Signal 容易短暂出现多个选中项。
3. **外观映射本地 6.6.5 Token。** 外径取 `fontSizeLG`，圆点取 `fontSizeLG - 2*(4 + lineWidth)`；颜色从 Map/Alias 的 primary、hover、border、container、disabled 与 focus 派生。圆环、内底、圆点保留固定三层 visual topology，颜色更新只写 material；尺寸变化再更新 layout/geometry。标签沿用 Checkbox 的语义文字与 spacer 机制。
4. **输入策略是 Space/Tab 与 PressableBehavior。** Enter 被消费；指针捕获/离开/取消沿用既有机制。当前 FocusManager 不提供从输入 handler 安全请求焦点的延迟导航接口，故 Group 首版不声明方向键切换合同；后续增加箭头导航时先设计焦点事务，而不在组件 handler 中重入 FocusManager。

公开状态矩阵：

| 控件 | 状态源 | 有效激活 | 已选中再激活 | disabled | 更新范围 |
| --- | --- | --- | --- | --- | --- |
| 独立 Radio | `checked` 或 `defaultChecked` | 请求/设置 `true` | 无变更 | 不可激活/聚焦 | 单个 Radio |
| 未受控 Group | `defaultValue` | 更新一个选中值 | 无变更 | 全组选项不可激活 | 旧值与新值选项 |
| 受控 Group | `value` | 回调请求新值，等待回写 | 无变更 | 全组选项不可激活 | 外部回写后更新选项 |

## Risks / Trade-offs

- Group 回调可销毁当前组件 → 先完成状态与子项同步，复制回调后调用，并在每次访问时按 ID 重新取状态。
- 选项值重复会使互斥关系含糊 → 挂载前检查并抛 `invalid_argument`。
- 静态 options 需要改变列表时重挂载 Group → 当前只要求值与禁用状态响应式，后续动态列表单独处理。
- 本轮 Windows 真实 Win32、D3D12/DXIL、系统字体可验；Linux Wayland/Vulkan 专属证据依用户安排暂缓，不使用 Windows 结果代替。

## Migration Plan

新增 API 不需要迁移。按规划、实现、Windows 验收分阶段提交；失败时可独立回退对应阶段提交。正式构建使用 `windows-msvc-debug`/`windows-msvc-release` 的 Ninja Multi-Config 和 MSVC；Linux 后续用正式 GCC/Clang preset。
