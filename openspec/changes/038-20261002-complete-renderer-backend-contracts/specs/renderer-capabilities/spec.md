# Spec Delta

## Purpose

明确后端承诺的共同 scene 视觉能力、版本兼容性与接受输入的资源上限，使组件保持平台无关，并在创建资源或上传之前报告不支持或超限，而不是默默丢弃文本、阴影和绘制顺序。

## ADDED Requirements

### Requirement: Explicit required renderer capabilities
backend MUST 显式声明 logical scene 与 packed ABI 版本、Quad/Glyph/Effect、R8 atlas sampling、有序绘制与局部上传能力；共同资源创建 SHALL 拒绝任一必需能力缺失、版本不兼容或无效资源限制。不得默默降级基础视觉合同。

#### Scenario: Unsupported backend
- **WHEN** backend 缺少任一必需能力或报告不兼容版本
- **THEN** 场景资源在创建 sampler/buffer/texture 前失败并提供明确原因

#### Scenario: Native device reports R8 support
- **WHEN** SDL renderer 初始化实际 device
- **THEN** R8 sampling 能力来自实际 format support 查询，不把编译成功当作设备能力证据

### Requirement: Reject scene input limits before upload
backend SHALL 声明 buffer bytes 与 texture width/height 输入上限；共同场景同步 MUST 在 begin/upload 前检查 scene 所需 buffer 容量及有 page 时的 atlas extent。上限是 backend 接受输入的限制，不保证所有输入都能获得硬件内存。

#### Scenario: Oversized scene
- **WHEN** Quad/Glyph/live Effect 所需容量或 atlas extent 超过 backend 声明上限
- **THEN** 上传尚未开始，附件失效且 dirty/retry 数据保持；修复输入后可完整重试

#### Scenario: Empty atlas and boundary input
- **WHEN** atlas 没有 page，或输入恰好达到声明上限
- **THEN** 不为不存在的 texture 拒绝 atlas config，满足限制的 scene 可正常提交
