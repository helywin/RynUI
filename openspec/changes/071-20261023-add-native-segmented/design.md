# Design

## Context

动机见 proposal。现有 Tabs 使用 String key、Prop<String> 和固定选项；Radio/Select 的复合值分别服务其现有合同。本组件采用 Tabs 的 String key，避免新增通用值框架。WindowComponentServices 提供逻辑归属、输入边界和 retained surface；Theme 已分离 token domains，并减少大型 snapshot 的栈复制。

## Goals / Non-Goals

支持稳定键的固定选项和只更新属性的标签、禁用及选择状态。富标签与 icon slots 只挂载一次，文本继承语义前景和字体。首版边界见 proposal；不扩展 renderer 或支持清单。

## Decisions

- `SegmentedOption{String key, String label}` 提供 `.label(Prop<String>)`、`.content(SegmentedLabel)`、`.icon(SegmentedIcon)`、`.disabled(Prop<bool>)`。`SegmentedProps::value/defaultValue/onChange` 使用 String；空/重复 key 和 controlled/default 冲突在挂载前拒绝。未知显式值保留但无选中项；没有显式值时选择首个可用项。空集、全部禁用可以没有选择。
- 每项固定 node/interaction/effects，颜色更新只 Material；选项布局与 label 更新才 Measure/Layout。选择禁用项的显式值保持显示；禁用不自动改写 selection、不发 onChange。回调复制后最后执行，不在回调后访问旧状态。
- 自然宽度按内容测量；水平 block 使用有限父宽并分配等宽；纵向按最大自然项宽或 block 宽排列。place_child 必须显式拉伸项的宽高，内部富标签在所分配宽度重新测量，避免测量缓存掩盖几何错误。
- 使用共同 focus/interaction：当前 focused 可用项优先成为唯一 Tab stop，否则选中可用项或首个可用项。水平 Left/Right、纵向 Up/Down 循环跳过禁用项；Home/End、Enter/Space 激活。输入逻辑检查 effective eligibility，遵守 Modal/Spin 边界。
- 独立 `segmented_tokens.hpp`，接入 snapshot 解析/继承/identity/serialization 和颜色、metrics/effects domains；保留所有既有 token 字段。默认映射锁定 6.6.5 的 track/item/selected/hover/disabled 语义与尺寸，完整 Web aliases 留待后续。
- 本地 `windows-msvc-headless`、Ninja Multi-Config、MSVC x64，独立 out/build，最多 4 编译作业；不复用或执行 root 构建。对所有旧 golden JSON 字段递归比较，仅允许 identity 与新增 segmented 字段不同。

## Risks / Trade-offs

- 富标签可能包含独立交互 → 文档要求展示型 slot；测试包裹布局与换行，不承诺任意嵌套交互。
- 回调销毁/输入边界重入 → 使用 generation ComponentId、deferred focus 和真实 host 测试。
- Theme 回归 → 运行既有 Drawer/Popup/TagBadge/feedback Theme 定向测试及全部旧 golden 字段对比；不更改旧派生公式或增加 snapshot 栈复制。

## Migration Plan

先提交规划，再实现并提交通过验证的组件，最后提交证据。仅本地提交，无 push、PR、archive。新增 API 可独立撤回，不改变旧 consumer。
