## Purpose

为 RynUI 提供可公开消费、确定性派生并支持响应式主题切换的 typed Theme Runtime，把完整 Token catalog 转换成 Default、Dark、Compact 与 Component Token snapshot。

## ADDED Requirements

### Requirement: Theme 必须遵守分层派生合同
Theme Runtime SHALL 按 Seed Token → Theme Algorithm → Map Token → Alias Token → Component Token 的顺序生成 immutable snapshot；下游 layer MUST 能追溯其输入 identity，Component Token 默认继承 global Alias Token，稳定组件不得绕过该链路读取孤立视觉常量。

#### Scenario: 生成默认主题
- **WHEN** 应用未提供 Theme override
- **THEN** Runtime 从锁定 Seed 生成完整 Default Map/Alias/Component snapshot，重复生成得到逐字段相等的结果和稳定 snapshot identity

#### Scenario: Component Token 继承 global token
- **WHEN** global `colorPrimary`、`borderRadius` 或 control density 被覆盖且 Button 未提供对应 component override
- **THEN** Button Component Token 从新的 global snapshot 派生，不保留旧的硬编码颜色或尺寸

### Requirement: Token value 必须使用强类型
公开 Theme API MUST 使用受验证的 `Color`、logical length、font family/weight/line-height、duration、cubic-bezier、border、radius、opacity、z-index、breakpoint、`ShadowLayer`/`ShadowList`、enum 与 typed optional value；API MUST 拒绝 CSS shorthand、任意字符串样式、NaN、无穷值、非法负尺寸、越界 opacity 和不合法层级关系。

#### Scenario: 构造合法 typed override
- **WHEN** consumer 使用 typed builder 覆盖品牌色、字体、radius、motion 或 shadow
- **THEN** Runtime 接受 override 并生成新 snapshot，公开 API 不暴露 React、DOM、CSS-in-JS、SDL3、GPU 或内部 Node 类型

#### Scenario: 非法 override 原子失败
- **WHEN** consumer 提供非有限长度、负 blur、越界 alpha、无效 easing 或不完整 Token identity
- **THEN** Theme 更新在发布新 snapshot 前失败并返回稳定诊断，现有组件继续使用先前完整 snapshot

### Requirement: Default Dark Compact Algorithm 必须可组合且确定
系统 SHALL 提供 Default、Dark 与 Compact Algorithm；Dark 只改变规定的 palette/neutral/surface/semantic 结果，Compact 在所选 color algorithm 之后调整 font、size 与 control height，Algorithm sequence MUST 有固定顺序且同一输入得到同一输出。

#### Scenario: 组合 Dark 与 Compact
- **WHEN** Theme 依次应用 Default base、Dark color 与 Compact density
- **THEN** 输出使用 Dark 语义颜色和 Compact size/control height，同时保留未受影响 Seed、Map 与 Component Token 的确定值

#### Scenario: 关闭 motion
- **WHEN** Seed `motion=false`
- **THEN** fast、mid、slow duration 全部解析为零时长，但状态终值、focus 可见性和 disabled/loading 行为不被跳过

### Requirement: Override 与继承必须有明确优先级
Theme override SHALL 按 global Seed/Alias override、Algorithm、Component Token override 的受控顺序解析；nested Theme 默认继承父 snapshot，显式关闭继承时从锁定 Default Seed 重新开始。Component-specific algorithm 默认关闭，只有 typed configuration 明确启用时才运行。

#### Scenario: Nested Theme 继承
- **WHEN** 子 Theme 只覆盖 Button `borderRadius` 并保持 inherit=true
- **THEN** 子树继承父 Theme 的颜色、字体、motion 与其他组件 Token，只有 Button 的目标字段变化

#### Scenario: 关闭继承
- **WHEN** 子 Theme 设置 inherit=false 并选择 Dark Algorithm
- **THEN** 子树从锁定 Seed 生成独立 Dark snapshot，不意外继承父 Theme 的品牌或 Component override

### Requirement: Theme 更新必须最小化失效
Token metadata MUST 声明其最小影响域；纯颜色、opacity、shadow color 或 z-order 材质变化只触发 Paint/GPU material 更新，font/line-height/control size/spacing/radius geometry 按实际依赖触发 Text、Measure、Layout 或 Geometry，Theme 更新不得默认触发 Structure rebuild 或重新执行无关 Component content。

#### Scenario: 切换只改变颜色的主题
- **WHEN** 应用从 Default 切换到只改变 semantic color 的 Theme
- **THEN** 读取相关 Color Token 的 primitive 更新，content closure 不重跑、Node/interaction identity 不改变、无关 Layout 不执行且 scene topology 不重建

#### Scenario: Compact 改变 control density
- **WHEN** 应用启用 Compact Algorithm
- **THEN** 读取 control height、spacing、font size 的组件执行必要的局部 Measure/Layout，未读取这些 Token 的子树不被失效

### Requirement: 现有基础组件必须统一消费 Theme snapshot
Text、Button、Flex 与 Space SHALL 从同一个 Theme snapshot 读取 typography、semantic color、control size、radius、state、focus、shadow 与 gap Token；迁移后 `DefaultThemeSnapshot` 中不得继续存在另一套会与新 Theme 分叉的视觉常量。

#### Scenario: 基础组件 Default parity
- **WHEN** 使用未覆盖的 Default Theme 运行现有 Text、Button、Flex 与 Space 示例
- **THEN** 除本 change 明确修正的 focus offset/shadow 行为外，组件的 logical bounds、默认颜色、字体、8/16/24 gap 与交互状态保持兼容

### Requirement: Theme snapshot 必须可诊断与验收
Runtime SHALL 能输出 snapshot version、algorithm chain、override scope、source manifest identity、resolved token count 与各 invalidation domain 的 changed count；输出不得包含地址或平台内部句柄。

#### Scenario: 主题示例输出诊断
- **WHEN** 示例切换 Default、Dark、Compact 或 component override
- **THEN** telemetry 能证明对应 snapshot identity 和 changed Token domain，且 idle 后不持续提交 frame
