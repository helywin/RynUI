# Design

## Context

见 proposal。已有 GalleryState、标签定位和 Recording fixture 可复用；组件首版本身保持各自公开 API 和支持边界。

## Goals / Non-Goals

使用有限业务记录展示数据与选择流程，不实现新的表格、菜单或数值模型，不增加请求服务或后台计时器。

## Decisions

- 在现有 LiveSamples 末尾追加，保留旧组件索引。Table 使用稳定业务 key 和少量固定列，单元格通过响应读取行内容，编辑回调更新源记录，避免只展示挂载时快照。
- Table 与 Pagination 共享受控页码，排序、选择与数据更新均由 GalleryState 写回；用有限数据控制保留节点规模。
- Menu/Dropdown 用真实命令和选择反馈，InputNumber 展示受控小数、范围及禁用/只读；复用已有组件，明确首版缺口。
- 验收先运行新聚焦场景、catalog 和实际 inventory，再在同一 MSVC windows-msvc preset 的 Recording fixture 跑完整 Gallery；不重复其他平台的共同逻辑。

## Risks / Trade-offs

行内编辑可能在排序翻页后丢失身份 → 断言稳定 key 对应控件与草稿保留。新组件增大 Theme 配置 → 保留默认主题转发与内部引用提交修复，实际挂载和主题切换回归。目录状态变更改变参考卡布局 → 运行整页滚动和可达性合同。

## Migration Plan

精简规划检查、英文提交并推送 main；依赖066/067/068合入后完成样例、聚焦验证和独立提交，再运行完整回归与记录。无需旧调用迁移，可独立回滚 Gallery 提交。
