# Spec Delta

## Purpose

使 renderer 可以消费同一份 R8 atlas 源字节而独立选择传输布局；明确源 rectangle、offset、row stride、数据生命周期及事务恢复，保证后续组件无需依赖具体 GPU API 的对齐规则。

## ADDED Requirements

### Requirement: Backend independent R8 source view
纹理上传接口 SHALL 表达目标 rectangle、源 byte offset、源 row stride 与借用 bytes，不要求调用者提供 backend transfer padding、transfer offset 或 layer stride。共同资源层 MUST 直接发布 atlas page 的源视图。

#### Scenario: Dirty atlas subrectangle
- **WHEN** dirty 区域位于 atlas page 内非零 offset，源 row stride 大于 rectangle width
- **THEN** backend 只复制该区域每行的有效 R8 pixels，并保持目标 rectangle 外像素不变

#### Scenario: Last row has no trailing padding
- **WHEN** bytes 恰好包含 source offset、前面各行 stride 及最后一行有效 width
- **THEN** 上传成功，不能额外要求末行尾部 padding

### Requirement: Validate before texture mutation
backend MUST 在复制或记录上传前拒绝零尺寸、短 stride、越界目标、越界源、溢出 offset/extent；invalid upload SHALL 不消耗 failure injection 或修改待提交纹理数据。

#### Scenario: Invalid source or destination
- **WHEN** 提交含不可表示 offset 或越界 source/destination 的区域
- **THEN** 该上传被拒绝，纹理像素与已有待提交区域不变，后续有效上传可正常完成

### Requirement: Backend owns accepted source pixels
backend SHALL 在 upload 成功返回前复制或拥有所需源 pixels；之后 caller 修改/释放源 bytes 不得影响 commit。各 backend MUST 自行处理 row/transfer 对齐，并保留 batch、chunk、cancel 与失败重试合同。

#### Scenario: Borrowed source changes before commit
- **WHEN** caller 在 upload 返回后覆盖原 bytes 并提交 batch
- **THEN** 目标纹理仍为上传时 pixels，SDL padding 为零，Recording 不保留 caller page

#### Scenario: Cancel or commit failure
- **WHEN** 多区域 batch 取消或 commit 失败
- **THEN** 不呈现不完整场景，共同事务恢复所有 atlas dirty 数据，重试和 epoch 重建从 CPU page 恢复正确 pixels
