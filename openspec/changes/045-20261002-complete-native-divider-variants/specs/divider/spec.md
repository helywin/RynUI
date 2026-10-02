## Purpose

RynUI Divider 为原生桌面提供水平或垂直内容分隔，在保留文字子树和既有主题、比例间距合同的基础上支持完整线条变体、尺寸以及逻辑标题方位；应用无需自行管理绘制资源或平台接口。

## ADDED Requirements

### Requirement: Native divider line variants

Divider SHALL 提供 typed/reactive Solid、Dashed、Dotted。Dotted SHALL 呈现圆点与透明间隙；`dashed(true)` SHALL 保持既有虚线行为，Dotted SHALL 优先于 legacy dashed。变体更新 SHALL 保留组件、文字子树与 scene 身份，不重新测量无关内容。

#### Scenario: Switch all variants in place
- **WHEN** 挂载后的水平或垂直分割线切换三种变体，并设置 legacy dashed
- **THEN** 圆点、虚线、连续线按优先级呈现，文字和几何身份保留，变体切换不触发文字重排

#### Scenario: Clip translated dots and limit resource count
- **WHEN** 分割线移动、窗口裁剪，或极小正线宽导致过多圆点/虚线段
- **THEN** 圆点保持圆形且按当前 translation/clip 裁剪；超过每个组件 4096 个装饰 primitive 的请求明确失败，不静默丢失视觉，也不进入无界循环

### Requirement: Native divider size and title position

Divider SHALL 提供 reactive Small/Middle/Large 尺寸和 Start/End 标题方位；默认 LTR，显式 RTL 只影响逻辑 Start/End，Left/Right 保持物理方位。尺寸 SHALL 使用 Theme 中的边距；未声明尺寸 SHALL 保留 034 默认几何，垂直线尺寸不改变行高合同。

#### Scenario: Sizes and inherited theme metrics
- **WHEN** 带文字和无文字 Divider 切换三个尺寸及 Default/Dark/Compact 或嵌套 token override
- **THEN** Small 使用小边距、Middle 使用中边距、Large 使用既有默认边距；主题颜色更新只改变材质，尺寸/度量更新按需测量和布局

#### Scenario: Logical and physical title positions
- **WHEN** LTR/RTL 下声明 Left/Right/Center/Start/End 并动态切换方向
- **THEN** Left/Right 的物理位置不变，Start/End 在 RTL 镜像，Center 保持居中；更新不重新执行保留文字 slot

### Requirement: Native divider length margins

Divider SHALL 在既有 Theme/None/Ratio 间距之外支持非负有限逻辑长度；显式长度在非居中标题下抑制近侧 rail，并作为标题到对应边缘的间距。过窄父约束 SHALL 收缩间距和轨道，避免负宽度或标题溢出。非法枚举、长度或比例 SHALL 明确拒绝。

#### Scenario: Length and ratio remain distinct
- **WHEN** 应用切换 Theme/None/Ratio/Length，并使用长度零或非零值
- **THEN** 长度使用 logical units，比例保持旧规则，零长度不等同于未声明；宽度不足时标题和轨道都保持在约束内

### Requirement: Divider native completion evidence

公开说明和 Gallery SHALL 反映已实现的原生合同与 Web 边界，支持状态 SHALL 由已通过测试及实际原生证据支撑。销毁 SHALL 释放圆点、线段、订阅和布局资源。Windows 与 Linux 原生验收 SHALL 分别记录，不互相代替。

#### Scenario: Inspect catalog and destroy a dotted divider
- **WHEN** 用户查看 Gallery Divider 样例后销毁组件
- **THEN** 可验证 variant/size/方向/间距示例，文字只挂载一次，全部绘制资源与订阅清理，平台证据可独立追溯
