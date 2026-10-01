# Spec Delta

## Purpose

为框架提供与具体 GPU 坐标和字节布局独立的 logical CPU scene，使组件的几何和材质更新能够由共同 renderer 层转换、局部上传并可靠恢复，供后续 backend 扩展复用同一 retained 场景合同。

## ADDED Requirements

### Requirement: Logical CPU scene coordinates

Core scene SHALL 保存 Quad/Glyph 的 logical 几何、平移与裁剪，Quad 圆角 SHALL 使用 logical 长度；CPU 数据 MUST 与 shader packed 字节类型独立。

#### Scenario: 同一场景使用不同 viewport
- **WHEN** 相同 CPU scene 在不同有效 device metrics 下同步
- **THEN** CPU scene 数值与绘制索引保持不变，renderer 产生适合各 viewport 的 packed bytes

#### Scenario: 组件发布几何
- **WHEN** Button、Selection、Divider 或 Text 装饰发布 Quad，文本服务发布 Glyph
- **THEN** 发布数据为 logical 几何，Core 不执行 NDC 转换或 GPU 上传

### Requirement: Packed GPU ABI compatibility

共同 renderer SHALL 将 logical scene 转换为 packed GPU ABI v1，保持 Quad 48 bytes、Glyph 80 bytes、既有属性 offset、NDC y 方向、UV、opacity、圆角及 draw order。有效几何 MUST 保持现有 SDL 视觉结果；无效 metrics SHALL 在上传前拒绝。

#### Scenario: 字形按物理栅格定位
- **WHEN** 已按字体 density 与 quarter-pixel phase 定位的 logical glyph 被打包
- **THEN** shader 位置保留该 physical raster origin 与 1:1 texel 尺寸，打包不触发 shaping、rasterization 或 atlas 上传

#### Scenario: 无效 device metrics
- **WHEN** extent 为零或 display_scale 非有限或非正数
- **THEN** 同步失败且 attachment 无效，不发布新的 GPU scene

### Requirement: Incremental packing and metrics invalidation

renderer SHALL 在普通更新时只重打包合并后的 dirty ranges，idle 同步 SHALL 不上传 instance bytes；首次、增长或 metrics 变化 SHALL 重新打包全部有效实例，即使 Core 没有 dirty geometry。

#### Scenario: 单实例材质更新
- **WHEN** scene 中只有一个 Quad/Glyph 的颜色或 opacity 改变，metrics 保持不变
- **THEN** 仅上传对应实例范围，无关实例 bytes 与字体状态保持不变

#### Scenario: Resize 无 CPU 失效
- **WHEN** device metrics 改变，CPU stores 已清空 dirty ranges
- **THEN** Quad/Glyph 全量重打包并上传，CPU 数据、组件 mount 和字体 atlas 保持不变

### Requirement: Packing participates in scene recovery

打包资源 SHALL 遵守共同 Scene 的 owner/epoch 与事务合同；失败后 MUST 使旧 attachment 无效，并在重试时恢复完整所需 packed 数据，设备恢复 SHALL 从保留 CPU scene 重建。

#### Scenario: Metrics 变化时上传或提交失败
- **WHEN** 新 metrics 的上传或 batch commit 失败后进行重试
- **THEN** 重试发布完整的新投影 bytes，不因 metrics 缓存或已清空 dirty ranges 保留旧投影

#### Scenario: Device epoch 改变
- **WHEN** device epoch 更新后同步同一 logical scene
- **THEN** 从 CPU scene 建立当前 epoch 的资源并保留 draw order，不重挂载组件
