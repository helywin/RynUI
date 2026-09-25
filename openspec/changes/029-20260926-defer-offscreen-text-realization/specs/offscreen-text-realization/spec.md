## ADDED Requirements

### Requirement: 离屏文本延后绘制资源生成
RynUI SHALL 在文本布局结果已有确定 bounds 后，以最终位置和可见 clip 判断是否需要生成该文本的 glyph 绘制资源；视口外文本的首次布局 SHALL 保留正确测量与布局结果，同时延后 glyph instance 和 atlas 上传。

#### Scenario: 首次布局含视口外文本
- **WHEN** 长文档首次完成布局且某文本 bounds 与保守扩张的 clip 不相交
- **THEN** 该文本保留正确测量与位置，且本轮不为该文本生成 glyph primitive 或 atlas 上传

### Requirement: 文本重新可见时使用最新状态
RynUI SHALL 在离屏文本进入可见 clip 的同步轮次中使用其最新内容、字体与滚动位置生成绘制资源；不得显示延后的旧版本。

#### Scenario: 离屏修改后滚动进入视口
- **WHEN** 文本离屏期间内容改变，随后滚动进入 clip
- **THEN** 首次可见帧的 glyph 绘制资源对应新内容及当前 translation，并保持正确的 fragment 顺序

### Requirement: 性能与窗口验收
RynUI SHALL 在 Windows 正式 MSVC Release 构建的真实 D3D12 Gallery 中记录首帧纹理上传、CPU 与固定滚动工作量，并通过输入和窗口验收；性能结论 SHALL 区分首帧与滚动阶段。

#### Scenario: Gallery 首帧和滚动复测
- **WHEN** 五个独立 Gallery 进程完成 `--scroll-acceptance`
- **THEN** 报告与 028 基线相同的首帧字段、固定 240 步的结果及可能的滚动回归，且不将 CPU 提交时间冒充 GPU 执行时间
