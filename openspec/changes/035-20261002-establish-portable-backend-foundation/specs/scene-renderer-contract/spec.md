# Spec Delta

## Purpose

为现有与未来 renderer 建立共同场景数据和资源提交合同，使用实际上传字节、纹理内容、绘制顺序及失败恢复验证保证后端替换不改变组件的 CPU 场景语义。

## ADDED Requirements

### Requirement: Shared scene submission
SDL GPU 和 Recording SHALL 消费同一 packed scene ABI、共同资源管理与上传事务。成功上传返回前 backend MUST 拥有上传字节，最终 commit 才表示事务接受，且不等同于 GPU 完成。

#### Scenario: Owned upload bytes
- **WHEN** 上传后调用者修改或释放原始字节
- **THEN** backend 接受的 buffer/texture 内容保持上传时数据

#### Scenario: Ordered component scene
- **WHEN** 真实组件场景包含 Quad、Glyph 和 RoundedEffect
- **THEN** Recording 按共同 ordered scene 的顺序记录 draw，内容与 CPU packed 数据一致

### Requirement: Retry after failed transaction
任意 begin/upload/commit 失败或异常时，共同资源路径 MUST 保留或恢复 CPU dirty 状态，后续成功事务 SHALL 重传所有受影响内容；idle 成功帧不得重复上传。

#### Scenario: Partial upload failure
- **WHEN** 一个资源接受上传后其他上传或最终 commit 失败
- **THEN** 重试得到完整的最新场景字节与纹理，并且随后 idle 同步没有新增上传

### Requirement: Resource ownership and device epoch
资源 MUST 归属于唯一 backend 与设备 epoch；外部 backend、旧 epoch 的附件/handle SHALL 被拒绝。设备 epoch 重建 SHALL 从保留的 CPU scene 恢复，无需重新执行组件挂载。

#### Scenario: Foreign and stale resources
- **WHEN** 将资源交给其他 backend 或设备 reset 后使用旧附件
- **THEN** 提交失败，旧回调/handle 不改变新代际资源

#### Scenario: Explicit rebuild
- **WHEN** backend epoch 改变且共同资源再次同步
- **THEN** 最新 CPU 内容完整重建，组件身份与编辑状态保持不变
