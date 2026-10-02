# Spec Delta

## Purpose

为 RynUI 提供可用于原生桌面表单与选项切换的单选组件，统一独立控件、动态组合组与按钮外观的值、焦点、主题和生命周期行为，使普通状态变化保持已有内容与资源身份。

## ADDED Requirements

### Requirement: 类型值与动态 Group

Radio SHALL 支持 String/double/bool 值，保留既有字符串受控入口。Group SHALL 支持 reactive options 或 typed 内容，最近组统一管理成员，拒绝重复、非有限值、互斥配置和超过 1024 项；受控状态等待回写，外部值更新无回调。

#### Scenario: 更新选项并保留身份
- **WHEN** options 重排、改名、禁用、删除、清空或恢复
- **THEN** 同值项保留组件和表面身份；顺序与 Tab 合同更新，删除项资源与 capture/focus 清理，非受控已删除值清空且不触发回调

#### Scenario: 手工组合与候选
- **WHEN** Radio 位于被动布局内的 Group 内容，并发生选择
- **THEN** 仅最近 Group 改变；成员 onChange、Group 候选回调和 onClick 按顺序使用相同候选，回调内销毁安全；点击已选项仅发 onClick

### Requirement: 原生键盘与 ref

Radio SHALL 提供 owner-thread RadioRef focus/blur、autoFocus 和 disabled 生命周期。Group SHALL 具有单一 Tab 入口；方向键循环跳过 disabled 项并选择，RTL 交换横向逻辑方向；Enter 不激活。

#### Scenario: 导航与受控值
- **WHEN** 使用 Tab、Space、横纵方向键选择受控组，或删除/禁用已聚焦成员
- **THEN** Space 不重复激活，方向键移动焦点并发候选而不越过受控权威，删除/禁用清理按压与焦点，ref 销毁后可复用且跨线程拒绝

### Requirement: 按钮外观与本地主题

RadioButton 与 Group optionType SHALL 支持 outline/solid、Small/Middle/Large、block、横纵方向与 RTL；相邻按钮连接单边界，仅外侧角圆角。视觉 SHALL 由独立 Radio 主题控制，普通颜色变化不重挂内容、不重排无关组件。

#### Scenario: 组合按钮与主题切换
- **WHEN** 组合按钮切换尺寸、方向、block、选中态与主题算法或 Radio token
- **THEN** 呈现正确相邻边界、圆角、文字与 disabled/hover/press/focus；标签和选择身份保留，颜色更新不重塑文字

### Requirement: 有限反馈与平台证据

Radio SHALL 提供可关闭有限 wave，指示器或按钮区域匹配当前形状，重启复用资源；结束、禁用、motion=false、reduced motion、失活和销毁 SHALL 取消后续 deadline。Windows 与 Linux 原生证据 SHALL 独立报告。

#### Scenario: 动画结束与窗口验收
- **WHEN** 反馈结束并进入空闲，或在真实窗口执行输入、缩放、resize、主题与动态组更新
- **THEN** 空闲无动画 deadline、无持续提交；实际 OS/renderer/font/scale/readback 记录可复现，未运行平台保持待验
