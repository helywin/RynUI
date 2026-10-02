# Spec Delta

## Purpose

使共同逻辑 glyph scene 能表达独立于 shaping/rasterization 的旋转，并由共同 renderer 契约统一验证与打包，在不同 viewport、density 和平台 backend 中维持一致坐标和裁剪。

## ADDED Requirements

### Requirement: Logical rotation and shared packing

共同 glyph SHALL 接受有限逻辑 pivot 和角度，旋转保持 coverage/UV 不变；renderer SHALL 在统一 device metrics 下转换旋转基向量，packed ABI 必须显式版本化，拒绝不兼容 backend。

#### Scenario: Non square viewport
- **WHEN** 同一 glyph 在不同宽高比和 density 下旋转
- **THEN** 逻辑旋转角度与尺寸一致，无屏幕宽高比拉伸，零旋转保持既有呈现

#### Scenario: Incompatible packed ABI
- **WHEN** backend 声明旧版本或变换含 NaN/Inf
- **THEN** 在资源上传/呈现前明确拒绝，失败不消耗有效 dirty 事务

### Requirement: Retained transform update and clip

单独修改 pivot/角度 SHALL 只更新 geometry dirty range，不重新 shape/rasterize 或改动 material；实际 shader 与 CPU reference SHALL 采用相同顶点变换与 viewport/ancestor clip。

#### Scenario: Transform patch
- **WHEN** 连续改变旋转但文本、字体和色彩不变
- **THEN** shape/atlas/upload coverage 不增加，geometry 正确更新，零尺寸与裁剪外图形不生成错误可见像素
