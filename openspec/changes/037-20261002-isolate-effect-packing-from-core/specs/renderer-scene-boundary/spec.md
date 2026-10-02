# Spec Delta

## Purpose

约束 RynUI logical scene 与 renderer 的依赖方向，让共同场景的坐标转换、shader ABI 和资源恢复独立于组件及平台宿主。后续后端可以复用三类 primitive 的打包合同，而 Core 保持 logical 数据和组件状态。

## ADDED Requirements

### Requirement: GPU packing belongs to renderer

系统 SHALL 将 Quad、Glyph 和 RoundedEffect 的 packed GPU ABI、device 坐标转换及 shader reference 置于 renderer 边界。Core SHALL 仅保留 logical scene、其数学与裁剪合同，不依赖 renderer 头文件或 target；renderer 共同层 SHALL 可以依赖 Core，并保持无具体 backend/OS 依赖。

#### Scenario: Core attempts to acquire renderer dependencies
- **WHEN** Core include renderer 头文件，或直接/间接链接 renderer target
- **THEN** 配置 SHALL 失败并定位违规依赖；renderer 共同层对 Core 和自身共同头文件的合法依赖 SHALL 通过

#### Scenario: Existing packed effects migrate
- **WHEN** renderer 打包合法 retained effect
- **THEN** 112-byte packed ABI、字段 offset、NDC/pixel 值、straight alpha、physical AA 与原有 outer/inset/outline coverage SHALL 保持，CPU logical instance SHALL 不被打包修改

### Requirement: Shared device metrics are validated

三类 primitive SHALL 消费同一 renderer device metrics 合同：正 physical pixel extent、有限正 display scale、可表示的有限正 logical viewport。非法 metrics SHALL 在资源上传开始前拒绝，不产生可呈现附件。

#### Scenario: Common metrics convert logical viewport
- **WHEN** 使用 physical 400×200 与 scale 2.0
- **THEN** 三类 primitive SHALL 使用 logical 200×100，并保持既有 GPU packed ABI v1

#### Scenario: Invalid or unrepresentable metrics
- **WHEN** extent 为零、scale 非有限/非正，或 extent/scale 产生非有限 viewport
- **THEN** 同步 SHALL 拒绝并不开始 upload；下一次合法同步 SHALL 能恢复

### Requirement: Effect failures invalidate projection cache

Effect 同步失败 SHALL 保留重试信息并失效成功 projection 缓存。abandon 旧 device 后 SHALL 不释放旧 handle 到新 device，并 SHALL 为下一代重新创建 buffer、完整打包上传；staging 容量 SHALL 可复用。

#### Scenario: Failed metrics update returns to previous metrics
- **WHEN** metrics 更新的 upload 失败，随后以原 metrics 重试且无 CPU dirty
- **THEN** Effect SHALL 完整打包上传原投影，不能把被失败上传污染的 buffer 当成 idle

#### Scenario: Effect device epoch is abandoned
- **WHEN** 资源丢弃旧 device handle 后使用新 device 同步保留 scene
- **THEN** SHALL 新建 buffer 并完整上传；旧代际容量和 metrics SHALL 不阻止重建，CPU effect identity 与 material SHALL 保留
