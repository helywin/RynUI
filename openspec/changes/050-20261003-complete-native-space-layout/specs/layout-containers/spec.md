# Spec Delta

## Purpose

定义原生 Space 的内容间距、默认与真实文字基线对齐、方向及 separator composition，保持 typed reactive API 和 retained 身份，使已有内容在尺寸、方向及主题变化后仍具有确定布局和完整资源清理。

## ADDED Requirements

### Requirement: Space 原生对齐和方向
Space SHALL 提供 Start/Center/End/Baseline 与自动默认对齐；省略 align 时 horizontal MUST 使用 Center，vertical 使用 Stretch。typed orientation SHALL 兼容旧 vertical，并提供 LTR/RTL；最后一次 orientation/vertical 配置决定订阅，声明和 focus 顺序保持。

#### Scenario: 默认对齐迁移
- **WHEN** 旧调用显式 Start 或新调用省略 align 并切换 orientation
- **THEN** 旧顶部/左侧意图保持，新调用按对应 H/V 默认对齐；自动 cross 尺寸拉伸且显式尺寸保留

#### Scenario: 基线与 RTL
- **WHEN** 不同字号 Text 和控件在 horizontal Baseline 与 RTL 中布局
- **THEN** 真实文字基线对齐、坐标按方向镜像，普通更新不重新执行 content

### Requirement: Space typed separator composition
Space SHALL 在相邻初始 item 边界挂载 typed separator，split 为兼容别名；无 item 或单 item MUST 不执行 separator。separator SHALL 作为独立 flow item 参与方向、gap 和 wrap，保留自己的 Theme、content、scene 和 lifecycle。

#### Scenario: 分隔内容保持
- **WHEN** N 个 item 的 Space 使用富 separator 且方向、gap、wrap 或 Theme 改变
- **THEN** separator 执行 N-1 次，布局顺序为 item/separator/item，后续更新不重挂载，销毁后没有订阅或 scene 资源

### Requirement: Space 验证和局部失效
Space SHALL 在发布模型前拒绝非法枚举/长度，并保留已有合法布局；基线变化 SHALL 测量，纯方向与非基线对齐 SHALL 复用测量，销毁 MUST 取消所有订阅和帧请求。

#### Scenario: 非法更新回滚
- **WHEN** reactive orientation 或 align 收到非法值
- **THEN** 报告错误且旧布局与身份保持，合法更新可以继续
