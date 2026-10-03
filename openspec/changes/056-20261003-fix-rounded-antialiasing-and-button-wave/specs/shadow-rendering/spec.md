## Purpose

为原生圆角填充、阴影与边框提供跨共享渲染路径一致的像素覆盖，防止零模糊或内部裁剪形成阶梯边缘，并保持原有逻辑布局、祖先裁剪和各平台独立 GPU 验收边界。

## ADDED Requirements

### Requirement: RadioButton border-box fill stays continuous

RadioButton 背景 SHALL 覆盖完整 border box，并在异色边框之下连续填充。同色背景/边框 MUST 以一次外缘覆盖呈现，避免内部 AA 叠加露出底色或外缘重复加深；混合圆角、连接边、主题与 retained 局部更新保持。

#### Scenario: Solid selected mixed corners
- **WHEN** Solid RadioButton 首尾、RTL或竖直连接按钮在分数坐标与DPI呈现
- **THEN** 背景内部不存在偏移浅框，曲面外缘连续抗锯齿，连接边保留优先级，更新不重挂内容

### Requirement: Rounded zero-blur coverage is antialiased

圆角填充与零 blur rounded effect SHALL 在曲面边界提供连续像素覆盖，不使用0/1硬阈值。覆盖过渡 MUST 使用固定物理像素尺度，不因 display scale 放大；效果不得改变 logical bounds、布局或命中。

#### Scenario: Fractional DPI round corners
- **WHEN** 相同圆角填充在1/1.25/1.5/2 scale以及分数坐标呈现
- **THEN** 曲面边界存在0与1之间的覆盖，中心保持不透明，远处保持透明，逻辑尺寸不变且不出现四象限接缝

### Requirement: Inset rounded surface mask preserves antialiasing

inset shadow SHALL 以平滑圆角 surface coverage 限制自身，保留声明的 Gaussian 衰减；祖先 clip MUST 仍限制合法范围，绘制边界包含必要像素安全区，不能裁掉自身曲面的 AA 过渡。

#### Scenario: Inset curved edge and ancestor clip
- **WHEN** 带 offset/blur 的 inset shadow 在圆角边缘与裁剪容器中呈现
- **THEN** 曲面覆盖连续、祖先之外透明，填充/outline 对齐且无矩形漏色

### Requirement: Shared coverage retains renderer contract

平台通用 coverage 数学与共同打包参考 SHALL 与锁定 shader 保持一致，保留共享 ABI、scene order、straight-alpha 与局部更新/idle 合同；实际 GPU 输出 MUST 按操作系统/backend 分别记录。

#### Scenario: Reference and actual readback
- **WHEN** 正式 preset 编译 shader并运行真实 GPU 像素矩阵
- **THEN** 输出与参考在声明容差内一致，DXIL/SPIR-V 来自同一 source，未运行平台不标记通过
