## Purpose

让原生应用以可访问的分隔条调整相邻面板，提供稳定的尺寸约束、折叠和恢复行为，适配嵌套布局以及持续或延迟更新模式，并在输入取消和销毁时释放捕获。

## ADDED Requirements

### Requirement: 交互式分割面板

Splitter SHALL 支持水平/垂直、多面板、逻辑/百分比尺寸、受控/defaultSize、min/max、resizable、双侧折叠与恢复、lazy、双击回调、resize start/change/end 及 collapse 回调。支持键盘箭头/Home/End，禁用或销毁时安全取消捕获。

#### Scenario: 受约束拖动和取消
- **WHEN** 用户拖动分隔条越过 min/max，随后取消或销毁组件
- **THEN** 尺寸保持有效，相邻面板总可用尺寸守恒，无残留捕获或延迟回调。

### Requirement: 隐藏面板生命周期

面板 SHALL 支持 destroyOnHidden，默认保留折叠内容；隐藏分支不接收输入，恢复后按策略保留或重建内容。

#### Scenario: lazy 与折叠
- **WHEN** lazy 拖动完成或面板折叠后恢复
- **THEN** 延迟尺寸在完成时提交，折叠回调与内容生命周期符合显式配置。
