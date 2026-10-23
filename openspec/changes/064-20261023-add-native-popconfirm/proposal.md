# Proposal

## Why

常见危险操作需要靠近触发入口的原生二次确认。现有 Tooltip 只有提示语义，Modal 过重；新增可交互的 Popconfirm。

## What Changes

- typed Props、trigger/title/description slots、reactive 属性和真实 Button 确认/取消。
- 支持受控/默认显示、disabled、showCancel、confirmLoading、文字、Escape/外部关闭、焦点转移恢复、12 方位和箭头。
- 复用 Tooltip 定位函数、window layer 与 Modal 输入边界，新增独立 Theme Token 和聚焦合同测试。
- 首版仅点击/键盘触发；不含 Promise、静态服务、动画、RTL、平台可访问性桥接或任意 ButtonProps 透传。

## Capabilities

### New Capabilities

- `native-popconfirm`: 保留式原生确认弹层的行为、主题和生命周期合同。

### Modified Capabilities

无。

## Impact

新增公开头、model/host、主题解析与订阅及 CMake 注册。只增量修改共享文件；不改 CPU scene v3/GPU ABI v2、Gallery、README 或父任务完成状态。风险集中于重入回调、焦点恢复和子弹层事件优先级，使用实际窗口服务 fixture 验证。
