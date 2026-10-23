# Proposal

## Why

原生应用需要内联状态提示、保留内容的加载反馈和确定进度，当前公共 API 缺少 Alert、Spin 与 Progress。此次提供可直接组合使用的首批原生反馈组件。

## What Changes

- Alert 提供共享 Status、响应式 message/description、icon/action/close typed slots、受控 visible/closable 与 onClose 请求。
- Spin 保留内容，加载时覆盖 dim shield 并屏蔽子树输入；只在可见且窗口活动且允许 motion 时调度有限 spinner 周期。
- Progress 提供 line、circle、dashboard，响应式 percent/status/size/showInfo 和 Theme token；复用 rounded/vector primitive。
- 补充公共合同、focused headless 测试和新增 token goldens，保持旧字段与 CPU scene v3/GPU ABI v2。

## Capabilities

### New Capabilities

- `native-feedback-components`: 内联提示、加载屏蔽和确定进度的原生 retained 行为。

### Modified Capabilities

无。

## Impact

新增公开头、component host 与局部 Theme/services/CMake 集成。以父提交 4e258e2 和 Tag/Badge 777939d 为依赖，复用其 Status。Ant Design 6.6.5 锁定 source 是基线，Context7 只作补充；不改 Gallery、README、全局 support overlay。风险是输入屏蔽、动画空闲和圆弧精度，使用定向测试验证。此阶段不包含 Spin fullscreen/delay/自动 percent、Progress steps/渐变/success segment/任意 gap、Alert banner/marquee/退出动画和平台 accessibility adapter；不声称原生 GPU、Linux 或系统 IME 验收。
