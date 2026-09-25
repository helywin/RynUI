# Spec Delta

## Purpose

规定同帧 atlas 与实例上传的顺序、提交确认及首次可见帧观测。

## ADDED Requirements

### Requirement: Gallery 上传应保持纹理和实例的有序依赖
系统 SHALL 将同帧 atlas 纹理和实例 buffer 上传按调用顺序编码，并在绘制依赖的 glyph 之前成功提交。

#### Scenario: 首次挂载大量 glyph
- **WHEN** Gallery 首次挂载并产生多个 atlas dirty rectangle 与实例区域
- **THEN** 上传保留每个区域、row pitch、目标位置和数据，使用共享 copy pass/命令提交，显示所需资源在 draw 前可用

### Requirement: 失败批次不得确认 atlas 状态
系统 MUST 在上传 batch 取消或命令提交失败时保留完整 atlas 与实例重传能力。

#### Scenario: 已编码纹理后提交失败
- **WHEN** atlas 纹理已编码但 batch 最终未成功提交
- **THEN** 下一次同步重新上传已有 atlas 页的完整有效内容，实例 buffer 也完整重传

### Requirement: 首次可见帧性能应独立报告
系统 SHALL 将首次可见帧的 CPU 阶段、atlas 上传与 GPU 上传提交数同滚动期指标分开，记录实际后端并声明 GPU 执行未测。

#### Scenario: 五进程真实窗口对照
- **WHEN** 相同 Windows MSVC Release 配置运行五个独立 D3D12 Gallery 进程
- **THEN** 报告首次帧与滚动 240 帧的原始值、汇总值、完成状态及差异，不以提交计数替代整帧测量
