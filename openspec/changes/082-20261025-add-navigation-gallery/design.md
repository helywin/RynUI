# Design

## Context

动机见 proposal.md。基线为 `62c990a`，以显式 merge 保留 `7db4123` 导航 Core 历史；不导入 Time 或 Descriptions WIP。Gallery 通过共享 definition 挂载 retained 组件，生产 runtime 与 Recording 均调用 `gallery_component_navigation_anchor`。

## Goals / Non-Goals

Goals：只追加两个真实样例，控制和恢复按钮位于被禁用/隐藏项之外；复用 Core typed slots、reactive Props 和当前 Theme。

Non-Goals：不扩展框架、公开 API 或 Theme；不创建路由框架、不模拟事件；不运行全量 frame/native 矩阵，也不创建远端操作。

## Decisions

1. Breadcrumb 使用固定唯一 key 的三项，祖先 title/visible/disabled 响应式更新，当前项富标题含原生 Input/Button，图标和 separator 使用 typed slots。回调展示原始 key/index，草稿不随状态更新重建。增加不带 handler 的展示层级验证子编辑资格。
2. Steps 使用受控 current 与真实 onChange 回写，外部上一步/下一步/重置及横纵向、尺寸、Error/Process、可见/禁用控制；说明内 Input/Button 独立激活。隐藏不改变原始编号和索引。恢复不清空草稿。
3. 两个独立所有权 Flex 追加在 DateGroup 尾部；旧 ID 和 `add_*` 函数体保持。只在共享 anchor helper 增加两项目标，避免测试与生产跳转分叉。全局数量只用于盘点和基线检查，定位使用 heading 所有权、field 或 editor 内容。
4. 新增 `--navigation-only`，同时接入完整入口；真实目录点击、Pointer/Key/Text/selection、聚焦覆盖、实际 glyph/quad/hit clip、普通更新和隐藏恢复均验证 retained ID。Default/Dark/Compact × 1200/560 × 900 覆盖有限视口；60000 高只做 inventory。
5. Windows MSVC `windows-msvc` configure、`windows-msvc-debug` build，Ninja Multi-Config，最多 4 jobs；只构建 frame/reference catalog targets，执行导航、受影响 Date/Form/Panels、inventory 与八项 catalog contracts。格式使用 clang-format 22.1.3，执行 doctor/strict/diff 和 generator consistency。此为平台通用 Recording 证据，不代表 OS/GPU/IME。

## Risks / Trade-offs

- [富内容宽度和外扩焦点被裁剪] → 样例保留边距，以实际 scene coverage 验证；发现 Core 缺陷保留未修改回归并交接。
- [新增 Input/焦点暴露旧全局 fixture] → 以原样例所有权补定位，保持原断言强度及 R1 数值/Collapse Select fixture。
- [支持状态夸大] → 仅修改两个 overlay 条目为明确 partial，验证后才加入命令与证据；其他条目逐项比较。

## Migration Plan

无持久化迁移。规划和实现各以本地英文 Conventional Commit 交付，保留 merge 历史；父线程独立审查和最终集成验收。回退时可只撤销 082 实现提交。
