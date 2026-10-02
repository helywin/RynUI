# Spec Delta

## Purpose

扩展原生 Tooltip 的标题内容、指针触发与箭头表现，使已有组件获得可复用的完整提示体验，并保持受控属性、子组件交互、窗口浮层与共用渲染资源的稳定合同。

## ADDED Requirements

### Requirement: Retained typed rich title

Tooltip SHALL 接受与 String title 互斥的 typed title slot，标题仅挂载一次并继承提示文字语义。reactive titleAvailable=false、disabled 或窗口失活 SHALL 隐藏标题。富内容更新 SHALL 重新测量自身浮层，不重跑 trigger 或外层 Content；关闭保留内容状态，销毁和挂载异常 MUST 完整释放。

#### Scenario: Rich content changes
- **WHEN** 已显示的富标题内 Text 属性或字体发生变化
- **THEN** 同帧更新自身尺寸与位置，标题、trigger 与 sibling identity 不变

#### Scenario: Empty rich title declaration
- **WHEN** 调用者将 titleAvailable 置为 false 或同时指定 String 和 slot
- **THEN** 前者取消显示和 deadline，后者在获得资源前拒绝

### Requirement: Pointer trigger composition

Tooltip SHALL 支持 Click、ContextMenu 与 typed hover/focus/click/contextMenu 组合；空组合为手动模式。primary 在同一后代按下并释放切换 click open，secondary release 在 trigger 内打开并锚定该指针位置。动作 SHALL 保留子控件激活、capture 与 focus，disabled 阻止提示请求，受控值只通过回调请求，不强制回写。

#### Scenario: Button click remains active
- **WHEN** 包裹 Button 的 Click Tooltip 收到完整 primary click
- **THEN** Button 激活一次，Tooltip 切换一次；drag 到外部释放或 cancel 不切换

#### Scenario: Repeated unacknowledged controlled click
- **WHEN** 受控 open=false 的提示连续点击但调用者没有回写
- **THEN** 请求依次 true/false，显示值保持 false

### Requirement: Outside dismissal and lifetime

Click/ContextMenu 提示 SHALL 在 trigger 和 popup 之外的 primary/secondary down 请求关闭，包括窗口空白。观察不得消耗路由事件或阻断外部控件激活。Escape、window loss、配置变化及销毁 SHALL 清除动作状态；回调可以销毁自身或其他提示，失效目标 MUST 安全跳过，路由重入拒绝并恢复 pointer 状态。

#### Scenario: Blank window dismissal
- **WHEN** 打开的 Click 提示外侧窗口空白处按下 primary
- **THEN** 提示请求关闭，pointer 仍正常结束，focus 不被提示夺取

#### Scenario: Destructive callback
- **WHEN** Button 或提示回调销毁同一路由中的 Tooltip
- **THEN** 本次派发安全完成，无 stale 访问、悬空 capture 或资源残留

### Requirement: Arrow center and vector scene

Tooltip SHALL 提供 reactive pointAtCenter：中心 placement 指向锚点中心；角 placement 默认使用固定边缘箭头位置，启用后移动 popup 使箭头对准锚点中心。翻转、移位和裁剪 SHALL 保持有限有效坐标。箭头 SHALL 通过共用 logical glyph/atlas 呈现连续三角形，支持四方向、颜色/尺寸更新、隐藏与销毁，无私有 GPU 上传。

#### Scenario: Corner alignment
- **WHEN** 宽 trigger 使用 topLeft 并切换 pointAtCenter
- **THEN** 箭头在 popup 上保留固定角内距，popup 横向移动使箭头指向 trigger 中心，trigger 不重新测量

#### Scenario: Real GPU coverage
- **WHEN** 四方向箭头在 Windows D3D12 的多 DPI 与 Dark/Compact 环境呈现
- **THEN** GPU 回读显示完整连续颜色、方向正确且无细条近似，关闭不留下 glyph

### Requirement: Native support evidence

支持文档 SHALL 区分原生实现、平台证据和排除的 Web API。平台通用合同只要求一次 headless Debug/Release；OS/GPU/font/input/DPI 验收 MUST 按实际 Windows/Linux 分开记录。

#### Scenario: Windows-only evidence
- **WHEN** 本机完成 Windows 原生矩阵但没有 Linux native 运行
- **THEN** Windows 项可完成，Linux 项保持 pending，不回退已经完成的平台通用任务
