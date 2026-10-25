# Proposal

## Why

已发布的 Descriptions 与 Skeleton 尚无可从 Gallery 目录直达的完整交互示例。需要让使用者实际编辑详情、调整布局并切换加载，在同一流程观察内容保留与恢复。

## What Changes

- 在既有样例之后追加详情编辑与加载预览，使用原生 Input、Select、Switch、Button；恢复控制置于隐藏内容之外。
- Descriptions 展示响应式列数、span、显隐、横纵标签、边框与尺寸；Skeleton 展示 loading、title、paragraph、avatar、round 及三种独立形态。
- 共同生产 anchor 将两个目录条目直接定位到真实示例。
- 增加 `--details-loading-only` 定向 frame 验证并接入 main，覆盖三主题、两种有限视口、真实事件、绘制裁剪、焦点装饰与 retained 身份。
- 仅更新两个组件的文档、support overlay 与生成目录；保留旧 ID 顺序、旧样例函数体和旧精度/所有权断言。

## Capabilities

### New Capabilities

- `details-loading-gallery`: 详情编辑与加载恢复的可操作 Gallery 合同。

### Modified Capabilities

无；当前主 specs 目录没有已发布 capability。

## Impact

范围限于 Gallery、frame tests、Descriptions/Skeleton 两条 overlay、对应文档及本 change。Core、公开 API、Theme、goldens、CMake、README、完成记录均不修改；无新依赖。风险主要是尾部追加改变滚动上限和旧盘点数量，以及窄视口中 popup/焦点装饰的真实裁剪。通过生产目录导航、输入事件、精确盘点与基线比较验证。完整 frame/native smoke 留父统一执行，不声明 GPU、Linux、系统 IME 或 AT 验收。
