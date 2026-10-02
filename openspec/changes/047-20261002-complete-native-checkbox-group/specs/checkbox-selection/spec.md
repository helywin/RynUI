# Spec Delta

## Purpose

定义 RynUI 原生 Checkbox 的单项与多选分组合同，使桌面应用能够使用稳定的状态内容、动态选项、焦点与主题反馈；规范受控方不回写、禁用、销毁及输入取消的可观察行为，避免功能目录掩盖未实现能力。

## ADDED Requirements

### Requirement: 原生多选分组状态

CheckboxGroup SHALL 接受受控 value 或一次性 defaultValue、group disabled 和 typed 原生 String/finite number/bool 值。有效激活 SHALL 报告去重、仅包含当前注册选项且按选项顺序排序的候选；受控状态 SHALL 等待调用方回写。半选保持独立的展示状态。

#### Scenario: 非受控多选
- **WHEN** 用户以 pointer 或 Space 选择/取消组中的可用选项
- **THEN** 各项 checked 与候选集合一致，group onChange 每次有效激活执行一次，禁用项不会激活。

#### Scenario: 受控方不回写
- **WHEN** 用户激活受控组但调用方没有修改 value
- **THEN** onChange 收到正确候选，展示继续服从原 value；外部 value 更新不产生用户回调。

### Requirement: 动态选项保留与失败原子性

CheckboxGroup SHALL 接受 reactive options，按 value 保留仍存在的选项身份、更新标签/disabled 并重排布局与 Tab 顺序；删除选项 SHALL 释放焦点、捕获、场景与订阅。重复值、非有限数字、重复选择值、超过 1024 options 或同时提供 options/content SHALL 明确拒绝，保留此前已发布状态。

#### Scenario: 重排和删除
- **WHEN** options 的顺序、标签或禁用发生变化并删除当前焦点选项
- **THEN** 未删除控件与标签保持身份，选项顺序同步到布局/键盘，删除项清理焦点/输入资源；非受控选择移除已删除值，受控 value 保留调用方权威。

#### Scenario: 无效动态输入
- **WHEN** 更新包含重复值或非有限数值
- **THEN** 更新明确失败，原选项身份、标签与选择展示保持可用，后续有效更新可恢复。

### Requirement: 保留的手工内容与组边界

CheckboxGroup SHALL 支持一次挂载的 typed 内容与嵌套布局，Checkbox SHALL 通过 value 注册到最近组；skipGroup SHALL 保持独立单项行为。组与单项 disabled SHALL 合并为逻辑或，手工标签 SHALL 支持被动 Text/Icon/布局。组自身 SHALL 不成为可激活或 Tab 焦点目标。

#### Scenario: 手工分组和跳过组
- **WHEN** 内容中包含带原生 value 的 Checkbox、skipGroup 单项与嵌套 CheckboxGroup
- **THEN** 各项仅参与所属最近组，内容不因 value/disabled 更新重跑，skipGroup 使用自身状态。

#### Scenario: 回调销毁
- **WHEN** 单项 onChange 在激活中销毁自身或组
- **THEN** 已复制的 group/onClick 候选回调可安全执行一次，不访问失效组件，不残留捕获与动画资源。

### Requirement: 原生焦点与方向

Checkbox SHALL 支持 owner-thread CheckboxRef 的 bound/focus/blur、一次性 autoFocus、同一目标值的 onClick，以及 reactive LTR/RTL。disabled SHALL 不可聚焦/激活且取消按压；RTL SHALL 镜像 checkbox 与标签位置。重复 live ref 绑定或跨线程操作 SHALL 明确拒绝。

#### Scenario: ref 生命周期
- **WHEN** 挂载 autoFocus、调用 ref、禁用或销毁后复用 ref
- **THEN** 焦点遵循可用状态，销毁后 ref 失效，复用绑定可正常工作且内容不重挂载。

### Requirement: 主题与有限反馈

Checkbox SHALL 通过 Theme 解析独立组件算法、大小/线宽/圆角、标签、checked/indeterminate/disabled/hover/focus 及 wave token；外部 LayoutStyle SHALL 不控制稳定视觉。有效用户激活 SHALL 产生有限 wave，motion=false、reduced motion、disabled、窗口失活或销毁 SHALL 取消。静止结束后 SHALL 没有动画 deadline。

#### Scenario: 主题隔离与闲置
- **WHEN** 修改 Checkbox 独立组件主色或激活后等待反馈结束
- **THEN** Checkbox 与标签按主题更新而不重跑内容，Radio/Switch 不受局部配置影响，反馈结束无持续帧请求。

### Requirement: 原生证据与覆盖状态

Gallery SHALL 展示已实现的 Group/内容/方向/焦点能力并引用共同合同；Windows 与 Linux 原生 OS/GPU/字体/输入/DPI 验收 SHALL 独立记录。DOM/React/HTML/CSS/浏览器表单 API SHALL 保持范围外。

#### Scenario: 平台证据边界
- **WHEN** Windows 实际窗口完成 readback 与输入缩放验收
- **THEN** 仅勾选 Windows 平台项，Linux 缺少实际机器结果时保持 pending，不影响已通过的共同合同。
