## Purpose

为 RynUI 提供与 Ant Design Token 对齐的 typed 多层阴影和精确 focus outline 渲染，使 elevation、popup、方向阴影、inset 边缘与键盘焦点在 GPU、DPI 和裁剪条件下可重复验收。

## ADDED Requirements

### Requirement: Shadow 必须表达完整 typed layer 语义
一个 shadow layer SHALL 明确表达 logical offset x/y、非负 blur radius、signed spread radius、Color/opacity、outer 或 inset kind 和 rounded shape radius；`ShadowList` MUST 保持声明顺序并支持 Ant Design 6.5.0 catalog 中所有多层、负 spread、四向与 inset shadow 组合，不使用 runtime CSS string parser。

#### Scenario: 三层 elevation shadow
- **WHEN** Token 解析包含三个 outer layer 的 `boxShadow`
- **THEN** Runtime 保留三个 layer 的独立 offset、blur、spread、alpha 与顺序，不把它们预混成单色或单层近似值

#### Scenario: 非法 shadow 原子失败
- **WHEN** layer 包含 NaN、无穷 offset、负 blur、越界 alpha 或超过受控 layer 上限
- **THEN** 整个 `ShadowList` 构造失败且不发布部分 shadow state

### Requirement: 默认 elevation 必须与锁定快照一致
Default Alias Token `boxShadow` 与 `boxShadowSecondary` SHALL 规范化为 `(0,6,16,0,8%)`、`(0,3,6,-4,12%)`、`(0,9,28,8,5%)` 三层；`boxShadowTertiary` SHALL 规范化为 `(0,1,2,0,5%)`、`(0,1,6,-1,3%)`、`(0,2,4,0,3%)` 三层，alpha 乘以 `colorShadow` 的基础 alpha。Card、Drawer 四向、Popover、Tabs overflow inset 和 Button default/primary/danger shadow MUST 进入 typed catalog 与对应视觉矩阵。

#### Scenario: Default shadow parity
- **WHEN** 从 Ant Design 6.5.0 Default Alias/Component Token 生成 shadow snapshot
- **THEN** 每个 layer 的 offset、blur、spread、方向、kind 与最终 premultiplied color 在容差内等于锁定 golden value

### Requirement: Outer 与 inset shadow 必须保持正确几何
outer shadow SHALL 从 rounded shape 外侧扩散且不污染 shape fill；inset shadow SHALL 限制在 shape 内部并按方向形成边缘衰减。signed spread MUST 先改变 shadow shape，再应用 blur；corner radius MUST 与 spread 后形状连续，透明像素不得形成矩形硬边。

#### Scenario: 圆角多层 outer shadow
- **WHEN** 带圆角的 Surface 使用正 spread、负 spread 和不同 blur 的多层 outer shadow
- **THEN** 每层围绕同一圆角轮廓连续衰减，四角无方形漏色、断层或可见采样边界

#### Scenario: Tabs overflow inset shadow
- **WHEN** 水平或垂直内容溢出使用相应 inset direction Token
- **THEN** shadow 只出现在内容内侧目标边缘，方向与 Token 符号一致且不覆盖外部相邻组件

### Requirement: Scene clip 与 z-order 必须可预测
outer shadow 的 effect bounds SHALL 包含 offset、spread、blur 安全区并参与脏区与 clip 计算；shadow 默认位于所属 surface fill 之前但保持组件间 scene order，ancestor clip 可裁剪 shadow，元素自身 content bounds 不得错误裁掉合法 outer blur。

#### Scenario: 相邻带阴影组件
- **WHEN** 两个 elevation Surface 在 scene 中重叠
- **THEN** 后绘制组件的 shadow 与 fill 共同位于前一组件之上，同时每个 surface 的 shadow 位于自己的 fill 之下

#### Scenario: Clip 容器裁剪 shadow
- **WHEN** 带 outer shadow 的 child 超出启用 clip 的 ancestor
- **THEN** 超出 ancestor clip 的 shadow 被裁剪，而未启用 clip 时 shadow 可超出 child logical bounds 完整显示

### Requirement: DPI 下必须保持 logical visual contract
shadow offset、blur、spread、outline width 和 outline offset SHALL 使用 logical pixel；GPU 在当前 drawable/display scale 下提供足够物理采样精度，但 Theme、Layout 与 HitTest 数值不得乘以 display scale。Shadow 与 focus effect MUST 不改变组件布局尺寸或 pointer bounds。

#### Scenario: 100% 与 150% shadow
- **WHEN** 同一 Theme snapshot 分别在 display scale 1.0 与 1.5 启动
- **THEN** shadow 和 outline 的 logical envelope 相同，1.5 输出使用更高物理 coverage，组件 measure、line break 与 HitTest bounds 保持一致

### Requirement: Button focus-visible 必须正确渲染空心 outline
Default Button keyboard focus SHALL 使用 Ant Design 6.5.0 `lineWidthFocus=3` logical px、`outlineOffset=1` logical px 与 `colorPrimaryBorder`；offset 区域 MUST 保持透明，outline MUST 是独立空心 rounded ring，不得把 width 与 offset 合并成连续 4px 实心蓝带。Pointer focus 不显示 ring，disabled 隐藏 ring，loading 保留已存在的 keyboard focus identity，hover/active/keyboard-pressed 不得改变 ring 几何。

#### Scenario: Keyboard focus ring
- **WHEN** 用户通过 Tab 把焦点移动到 enabled Button
- **THEN** Button 外侧先出现 1 logical px 透明间隔，再出现 3 logical px `colorPrimaryBorder` 空心 ring，内外圆角连续且 Button border 不增厚

#### Scenario: Pointer 与 disabled 状态
- **WHEN** Button 由 pointer 获得焦点或 focused Button 变为 disabled
- **THEN** pointer focus 不显示 ring，disabled transition 清除或隐藏 ring，HitTest 与 declaration-order focus contract 不改变

### Requirement: Shadow 与 outline 更新必须保留 retained 性能
纯 shadow/outline color、opacity 或 visibility 更新 SHALL 只修改 effect material/visibility；offset、blur、spread、radius 或 bounds 变化最多触发 Geometry/Paint 和局部 GPU range upload，不得触发 Component content 重跑、Structure rebuild 或无关 Text measure。稳态 idle MUST 停止 frame submission，常见三层 shadow 不得产生每帧 heap allocation。

#### Scenario: Theme 切换只改变 shadow color
- **WHEN** Dark Theme 只改变 `colorShadow` 与 outline color
- **THEN** effect instance identity 与 scene topology 保持不变，只上传受影响 material range，idle 后不再 submit

### Requirement: GPU shader 必须跨后端保持合同
阴影与 outline shader SHALL 从同一锁定 HLSL source 离线生成 DXIL 与 SPIR-V，reflection、instance layout、blend、premultiplied alpha、layer order 与数值容差由合同锁定；D3D12 和 Vulkan 的平台特有真实 GPU 证据 MUST 分别完成，平台通用数学、Scene 与 headless tests 只需在一个受支持平台验收一次。

#### Scenario: Shader artifact parity
- **WHEN** 正式 preset 构建 shadow shader
- **THEN** DXIL/SPIR-V artifact、reflection 与 deployed shader 均来自锁定 source，缺失、过期或 instance layout 不一致时构建或 contract test 失败

#### Scenario: 真实窗口视觉矩阵
- **WHEN** 在受验收 GPU backend 打开 Theme/Shadow 示例并切换 Default、Dark、Compact、三档 elevation、多层、四向、inset 与 Button focus 状态
- **THEN** 截图、driver/shader format、display scale、effect count、upload/draw/submit counters 与正常退出码写入该平台独立 evidence，且不存在硬边、矩形裁切、错误叠层或连续 4px focus 蓝带
