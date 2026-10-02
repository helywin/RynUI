# Spec Delta

## Purpose

使 RynUI 原生桌面 Button 具备可组合的颜色、变体、图标与加载呈现，同时保持受控属性、既有槽节点、键盘焦点和主题失效阶段稳定，并为点击反馈与窗口生命周期提供可验证的有限时长合同。

## ADDED Requirements

### Requirement: Typed color and variant precedence

Button SHALL 提供 Default/Primary/Danger 与锁定参考的预设色、Outlined/Dashed/Solid/Filled/Text/Link 变体，以及 ghost/danger；未配置新增选择器时旧类型和 tokens 保持兼容。显式 color/variant 各自覆盖 type 推导，danger 只覆盖非显式 color，ghost 将 Solid 转为透明 Outlined；Text/Link 的 ghost 无额外作用。非法 enum MUST 在资源获取或属性更新前拒绝。

#### Scenario: Reactive combination
- **WHEN** 既有 Button 从 Primary 切换到 preset color + Dashed，再切换到 ghost
- **THEN** 同一个组件显示主题派生的虚线圆角/透明背景，Content 不重跑，颜色更新不重新测量

#### Scenario: Disabled precedence
- **WHEN** 任意颜色或变体的 Button disabled 或 loading
- **THEN** disabled 高于 hover/pressed/loading，Text/Link 保持透明背景；loading 阻止 activation 且保留焦点

### Requirement: Retained icon and loading composition

Button SHALL 提供 typed 常规图标和自定义加载图标 slots、start/end 位置和 icon-only 入口。图标、内容与加载 slot 一次挂载，loading 时替换图标，布局只计入当前可见项及必要 gap；reactive 文本/图标/位置/加载变化不得重跑 slots。无自定义加载图标时使用既有 spinner。

#### Scenario: End icon with loading
- **WHEN** 带内容的 Button 配置 End 图标并切换 loading，再更新内容
- **THEN** 加载图标位于内容末尾，图标替换不多留 gap，结束加载恢复同一个图标节点

### Requirement: Shape and external sizing

Button SHALL 提供 Default/Circle/Round/Square 与三档尺寸，icon-only 默认正方尺寸；Circle 保持至少 control height 的宽度与圆角，Round 使用半高圆角，Square 使用零圆角。block 占据有限父约束宽度，LayoutStyle 的显式外部宽度优先，不将视觉值写入 LayoutStyle。

#### Scenario: Resize block and round
- **WHEN** block Round 按钮的父宽度变化
- **THEN** 按钮同帧更新宽度、边框和命中范围，文本/图标保持挂载

### Requirement: Native focus reference and delayed loading

Button SHALL 提供 owner-thread、代际安全的 focus/blur ref 与 mount-only autoFocus；重复绑定失败、销毁后 ref 返回 false、重用不能驱动旧组件。loading delay 为非负 Duration，可被 false、重新配置与销毁取消；等待期间保留既有可操作状态，达到截止时间才进入 loading。

#### Scenario: Cancel before deadline
- **WHEN** loading=true 配置延迟后在 deadline 前恢复 false
- **THEN** 不出现加载图标、不保留下一帧 deadline，Content 不重跑

### Requirement: Finite wave lifecycle

Button SHALL 对可操作的有边框变体 activation 显示主题控制的有限时长扩散/淡出 wave，并支持关闭。wave 不改变布局/命中/焦点；disabled/loading、无边框变体、motion off/reduced、window inactive 或销毁 MUST 清除 wave 和未来请求，回调销毁组件不得留下资源；重复 activation 重启当前 wave。

#### Scenario: Window loss during wave
- **WHEN** wave 播放中窗口失活或组件被 activation 回调销毁
- **THEN** 无可见 wave、残留 effect 或未来 wave deadline，旧 ref 不驱动新组件

### Requirement: Shared rendering and platform evidence

Button SHALL 通过共同 logical scene、glyph atlas 与 rounded effects 呈现，不添加平台/backend 私有上传。平台通用测试与 Windows、Linux 的实际 GPU/字体/DPI/输入验收分开记录，未运行平台不得标记通过。

#### Scenario: Native DPI matrix
- **WHEN** Windows Debug/Release 在系统与 1/1.25/1.5/2 比例运行按钮窗口
- **THEN** 每次记录 driver/shader、退出码、图像与哈希，覆盖主题、透明 ghost、虚线、图标位置、spinner/wave、焦点与 resize
