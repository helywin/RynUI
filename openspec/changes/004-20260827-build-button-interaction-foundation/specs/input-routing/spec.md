## Purpose

定义平台输入进入 RynUI 后的稳定、可测试路由语义，使命中测试、传播、hover 和 pointer capture 不依赖 SDL3 类型，并在组件销毁或回调重入时保持 identity 与生命周期安全。

## ADDED Requirements

### Requirement: 平台输入归一化
系统 SHALL 把首批鼠标主按键、主触点、键盘按下/抬起、窗口焦点变化和取消事件归一化为 RynUI 自有事件值；位置 SHALL 使用窗口 logical coordinates，且事件类型 MUST 不包含 SDL3 类型、scan code、window handle 或 GPU 类型。输入只 SHALL 在 Window 的 UI owner thread 上进入 Runtime。

#### Scenario: Pointer 坐标不随 framebuffer scale 改变语义
- **WHEN** 同一窗口在非 1.0 display scale 下收到位于组件 logical bounds 内的鼠标或主触点事件
- **THEN** 系统使用 logical coordinates 命中该组件，不要求上层按 framebuffer pixels 手工换算

#### Scenario: 平台类型不泄漏
- **WHEN** 编译只依赖公开 RynUI headers 的 consumer，并扫描 Reactive、Layout、Component 与公开 API
- **THEN** 这些边界中不存在 SDL event、scan code、window handle 或 GPU backend 类型

#### Scenario: 非 owner thread 输入被拒绝
- **WHEN** 非 UI owner thread 尝试派发已归一化输入
- **THEN** 系统以确定错误拒绝该操作，且不改变 hover、capture、focus 或组件状态

### Requirement: HitTest 选择与绘制一致的目标
系统 SHALL 根据已完成布局的有效 bounds、translation、有效 clip、可交互状态和实际绘制顺序执行 HitTest；重叠目标中最深且最后绘制的 eligible 节点 SHALL 成为 target。纯视觉子节点 MAY 将命中归属委托给最近的可交互祖先。

#### Scenario: 重叠组件选择最上层目标
- **WHEN** 两个可交互组件的有效 bounds 重叠且后声明组件后绘制
- **THEN** 重叠区域只命中后绘制组件，并生成从 root 到该 target 的稳定 route

#### Scenario: clip 外不命中
- **WHEN** pointer 位于节点自身 bounds 内但位于祖先有效 clip 或窗口内容区域之外
- **THEN** 该节点及其交互祖先不因该位置成为 target

#### Scenario: HitTest dirty 最小更新
- **WHEN** 一个节点只改变 Material 或 Text content 且其有效交互 bounds 与顺序未改变
- **THEN** 系统不得重建无关节点的 HitTest 数据；bounds、translation、clip、交互资格或顺序变化时才更新对应范围

### Requirement: Pointer 三阶段传播
系统 SHALL 以 route snapshot 按 Capture、Target、Bubble 顺序派发 pointer 事件，并支持 handler 停止后续传播。每次进入 handler 前 MUST 重新校验 generation 和交互生命周期，避免回调期间销毁或复用节点后访问 stale target。

#### Scenario: 完整传播顺序
- **WHEN** root、父节点和 target 均注册对应阶段 handler，且没有 handler 停止传播
- **THEN** 调用顺序为 root 到父节点的 Capture、target 的 Target、父节点到 root 的 Bubble

#### Scenario: Capture 阶段停止传播
- **WHEN** 父节点 Capture handler 停止传播
- **THEN** target 与 Bubble handler 均不再执行，已经执行的 handler 不会重复调用

#### Scenario: 回调销毁 target
- **WHEN** 任一 handler 销毁 target、祖先或整个 Component Scope
- **THEN** 系统重新校验剩余 route，跳过 stale identity，自动清理相关 hover/capture，且不访问复用 slot

### Requirement: Hover 与 pointer capture 生命周期
系统 SHALL 为每个受支持 pointer identity 维护 hover path 和可选 capture。pointer down 后 target MAY capture 该 pointer；capture 存续期间 move/up/cancel SHALL 路由给 capture target，同时 hover path 仍按实际位置确定。up、cancel、窗口失焦、target 销毁或失去交互资格 MUST 释放 capture。

#### Scenario: 拖出后释放不产生 click
- **WHEN** Button 在 pointer down 时取得 capture，pointer 移到其 bounds 外再抬起
- **THEN** Button 收到 captured up 并清除 pressed，但不触发 click，hover 状态按实际位置更新

#### Scenario: 窗口失焦取消按压
- **WHEN** pointer 被组件 capture 且窗口失去焦点
- **THEN** 系统派发确定的 cancel、释放 capture 并清除 pressed，不触发 click

#### Scenario: 多 pointer identity 相互隔离
- **WHEN** 不同 pointer identity 依次产生事件
- **THEN** 每个 identity 的 capture 与 pressed 生命周期独立，不把一个 pointer 的 up 解释为另一个 pointer 的激活

### Requirement: 稳态输入路径可观测且有界
系统 SHALL 提供 HitTest、route、capture、cancel 和 stale-skip 诊断计数；在交互树和 handler 容量稳定后，单次 pointer move/press/release 的 steady-state 路径 MUST 不产生 heap allocation，也不得隐式请求连续帧。

#### Scenario: 稳态 pointer move
- **WHEN** 在不改变树结构和注册数量的情况下重复派发 pointer move
- **THEN** 诊断只增加必要的 HitTest/hover/route 计数，steady-state 分配计数为零，且没有持续 frame submit

