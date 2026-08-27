## Purpose

定义 RynUI 公开组件从 typed content 闭包挂载到 retained Node 树的组合与生命周期合同，并限制 LayoutStyle 只表达组件外部布局关系。

## ADDED Requirements

### Requirement: 组件在应用 Host 中按声明顺序挂载
系统 SHALL 在应用 runtime 的组件 Host 中以 UI owner thread 执行公开 typed content 闭包，并按声明顺序把组件建立为持久化树；普通 Props 更新 MUST 复用已挂载对象，不得重新执行整个 content 闭包。本 change 不要求发布稳定的 `Application`、`Window` 或 Host 构造 API。

#### Scenario: 首次挂载执行一次 content
- **WHEN** 调用方在组件 Host 中声明两个相邻 Text 组件
- **THEN** content 闭包执行一次，两个组件按声明顺序获得稳定身份并进入 retained 树

#### Scenario: 普通字段更新不重新组合
- **WHEN** 第一个组件的响应式字段变化且组件结构未变化
- **THEN** Host 不重新执行 content 闭包，第二个组件的身份和状态保持不变

### Requirement: Typed slot 只组合声明允许的内容
组件 API MUST 以具名 typed slot 或明确的 typed content closure 表达子内容；slot 接受的内容种类由组件类型约束，不得提供任意 DOM children、字符串键或通用 `Modifier` 链。

#### Scenario: 合法 content slot
- **WHEN** 组件调用其公开 content slot 并在其中声明受支持的子组件
- **THEN** 子组件挂载到正确父级且保持调用顺序

#### Scenario: 不支持的 slot 不存在
- **WHEN** consumer 尝试给只支持 content 的组件设置未声明的 prefix 或 footer slot
- **THEN** 公开类型系统不提供该入口，错误在编译期暴露

### Requirement: 组件销毁释放完整子树与 Scope
Host、父组件或组件实例销毁时 MUST 先停止其响应订阅，再释放关联的 Text/Scene 资源和 retained Node 子树；重复 dispose MUST 安全且不得重复调用清理。

#### Scenario: Host 销毁完整清理
- **WHEN** 持有多个子组件的 Host 被销毁
- **THEN** 所有子组件 Scope、Node、Text state 和 Scene range 均被释放，后续源 Signal 写入不请求帧

#### Scenario: 重复 dispose
- **WHEN** 调用方或生命周期路径对同一 Host 请求多次 dispose
- **THEN** 首次请求完成清理，后续请求不重复释放也不访问失效对象

### Requirement: LayoutStyle 只控制外部布局
公开 `LayoutStyle` SHALL 在本 change 支持可选 width、height、min/max width、min/max height 与四边 margin，并只影响组件在父布局中的外部约束和放置；它 MUST NOT 改变 Text 的颜色、字体、line height、glyph、背景或其他稳定视觉。

#### Scenario: 固定外部宽度约束 Text
- **WHEN** Text 的 `LayoutStyle` 设置有限 width
- **THEN** 父布局以该外部宽度约束 Text measurement，文字在合法 cluster 边界换行，视觉 token 不变

#### Scenario: Margin 只改变放置
- **WHEN** 已挂载 Text 的 margin 改变而内容和可用 content width 不变
- **THEN** Text 在父布局中的位置更新，不重新 shaping 或改变 Material

#### Scenario: 非法尺寸被拒绝
- **WHEN** `LayoutStyle` 包含负数、NaN 或 min 大于 max 的尺寸约束
- **THEN** 挂载或更新以明确错误 fail-fast，既有组件树和布局状态保持有效

### Requirement: 公开组件边界不泄漏内部类型
公开 content closure、typed slots 与 `LayoutStyle` API MUST 使用 `ryn` 命名空间和 RynUI 自有值类型，不得暴露内部 Host、`NodeId`、layout model、Scene range、GPU handle 或 SDL3 事件与对象。

#### Scenario: 公共组件头隔离编译
- **WHEN** consumer 只包含公开组件和 layout style headers 并声明最小组件树
- **THEN** consumer 无需引用 `src/`、FreeType、HarfBuzz 或 SDL3 header，且公开符号中不存在这些类型
