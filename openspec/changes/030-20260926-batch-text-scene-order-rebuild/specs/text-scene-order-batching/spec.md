## ADDED Requirements

### Requirement: 文本宿主批次合并绘制顺序重建
RynUI SHALL 在同一文本宿主同步轮次中合并多个文本 primitive 改变，仅在所有成功记录处理后重建一次 ordered scene；最终 draw commands、range、atlas page 与相同顺序的逐条同步结果 SHALL 一致。

#### Scenario: 多个文本同帧进入视口
- **WHEN** 多个此前未实现化的文本在一个宿主同步轮次中进入可见 clip
- **THEN** 每个文本生成正确 primitive，最终 ordered scene 按原声明顺序绘制，且该轮 ordered scene 最多重建一次

### Requirement: 单记录和失败边界保持一致
RynUI SHALL 让批次外单记录同步在返回时具有可读的 ordered scene；批次提前失败或异常后不得把陈旧 ordered scene 作为下一次成功帧的最终结果。

#### Scenario: 批次取消后重试
- **WHEN** 多记录批次在部分更新后取消，随后执行新的成功同步
- **THEN** 新同步提交前重建包含已成功更新记录的 ordered scene，不丢失绘制顺序或 range 修改

### Requirement: 真实窗口性能复测
RynUI SHALL 在 Windows MSVC Release 的五个独立 D3D12 Gallery 进程中比较 029 基线，分别报告首帧、240 步滚动平均/p95/最长帧与新增 raster；不得将 CPU 墙钟时间描述为 GPU 执行时间。

#### Scenario: Gallery 长距离滚动
- **WHEN** 固定 240 步 `--scroll-acceptance` 包含最后的长距离跳转
- **THEN** 所有进程到达相同最终 offset 并退出成功，报告最长帧是否真正下降及任何其他阶段回归
