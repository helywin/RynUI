# Spec Delta

## Purpose

为原生桌面组件提供由窗口拥有的文字提示浮层，使鼠标和键盘用户在不改变布局、焦点与组件生命周期的情况下获得说明，并为 Slider、Typography 等现有组件补齐可复用的提示能力。

## ADDED Requirements

### Requirement: Typed reactive visibility

Tooltip SHALL 提供 typed trigger slot 与 reactive title、open、disabled、placement、arrow 和延迟属性，支持受控与非受控模式。空 title、disabled 或失活窗口不显示；显式 open 与 defaultOpen 冲突 MUST 拒绝。用户触发只请求受控值变化，不改写调用者属性。

#### Scenario: Controlled trigger
- **WHEN** 用户悬停受控且 open=false 的 Tooltip
- **THEN** 回调请求 open=true，但提示保持隐藏，直到调用者回写；等值请求不重复回调

#### Scenario: Empty or disabled
- **WHEN** 已显示提示的 title 变空或 disabled=true
- **THEN** 提示与未到期计时器立即清除，child 的原有交互保留

### Requirement: Pointer and keyboard lifecycle

Tooltip SHALL 在 hover 或后代键盘 focus 时显示，支持进入与退出延迟、Escape 关闭、窗口失活关闭、销毁后无计时器或残留场景。提示不抢夺焦点，不阻断 trigger 的事件，禁用 child 仍可由外层 trigger 接收 hover。

#### Scenario: Delay and cancellation
- **WHEN** 鼠标进入后在进入延迟到期前离开
- **THEN** 不显示提示，并撤销过期 deadline；idle 不持续请求帧

#### Scenario: Keyboard dismissal
- **WHEN** trigger 后代获得键盘 focus，然后按 Escape
- **THEN** 提示关闭并保持原焦点，直到 focus/hover 离开再重新进入才自动显示

### Requirement: Window overlay positioning

Tooltip SHALL 支持十二种方位、箭头与 autoAdjustOverflow；越界时优先主轴翻转，再在窗口内移位，锚点移动、scroll translation 与 resize 后同帧重新定位。浮层绘制高于普通内容，不影响父布局测量，显示范围由窗口限制；尺寸与坐标必须有限，极小窗口无负尺寸。

#### Scenario: Edge placement
- **WHEN** top 提示靠近窗口上边且 bottom 有更多空间
- **THEN** 自动翻到 bottom，并把侧向坐标限制在窗口内；箭头指向锚点

#### Scenario: Retained content
- **WHEN** 后续 sibling 绘制或 trigger 位于裁剪内容中
- **THEN** 提示仍在窗口浮层绘制，普通 child 顺序保持；关闭或销毁不留下 quads/glyphs

### Requirement: Theme and scene contracts

Tooltip SHALL 使用 Theme 与 typed Component Token，支持 Default/Dark/Compact、nested inheritance、override 和 component algorithm。纯颜色变化 MUST 不测量或重建 trigger；稳定提示使用共同 logical scene 与文本资源，无私有 backend 分支。公开支持状态 MUST 区分已实现原生功能、缺失原生功能和 Web 专用接口。

#### Scenario: Material update
- **WHEN** 只覆盖 Tooltip 的背景或文字颜色
- **THEN** retained identity 不变，无无关 sibling 测量、remount 或持续 upload
