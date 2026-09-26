## ADDED Requirements

### Requirement: 新 glyph range 原位插入
RynUI SHALL 在零长度 glyph instance range 插入非空新内容时复用现有 vector 容量并只移动必要后缀；结果 SHALL 与原完整替换路径的实例顺序及 range 一致。

#### Scenario: 多个新文本插入现有场景中间
- **WHEN** 同一场景已有前后文本，连续为多个未实现化文本插入 glyph instances
- **THEN** 现有和新文本的实例顺序及 draw ranges 正确，底层实例容量可跨多次插入复用

### Requirement: 源重叠、错误及脏区合同
RynUI SHALL 在源实例 span 与目标 store 重叠时保留调用开始时的源数据；范围或分配失败不得提前确认 dirty，成功插入后 SHALL 使受影响后缀的 geometry dirty 覆盖完整、material dirty 不引用过期索引。

#### Scenario: 使用 store 内源片段插入
- **WHEN** 插入源 span 指向同一 store 中已有实例
- **THEN** 新实例等于插入前源片段，原实例顺序保留，且脏区对应新位置

### Requirement: Windows 真实窗口长帧复测
RynUI SHALL 在正式 MSVC Release 的五个独立 D3D12 Gallery 进程中复测首帧和 240 步滚动，报告与 030 基线相比的平均、p95、最长帧、上传及最终状态，并把 GPU 执行时间标为未测或独立测得。

#### Scenario: 最后一步长距离跳转
- **WHEN** `--scroll-acceptance` 的第 240 步跳至文档末端
- **THEN** 场景到达与基线相同的最终 offset 且绘制与输入验收通过，报告最长 CPU 帧是否超出进程间波动
