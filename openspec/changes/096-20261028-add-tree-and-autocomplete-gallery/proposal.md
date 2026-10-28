# Proposal

## Why

AutoComplete 和 Tree 已集成发布，但组件目录尚不能进入可操作样例。需要让使用者从 Gallery 实际体验自由输入与建议选择、层级选择与勾选，同时保留已经验收的成员、指标及评分流程。

## What Changes

- 在现有样例末尾追加联系人建议和项目目录两个业务流程；仅使用已发布的 typed API，不扩大组件首版范围。
- AutoComplete 展示实际输入、建议过滤/更新、键盘选择、受控接受/拒绝、清空、禁用/只读恢复及保留富建议草稿；加入 Form String 同值 reset 操作。
- Tree 展示固定 keyed 项目层级、真实展开/选择/级联勾选、禁用隔离、显隐恢复、有限滚动和富标题备注保留。
- 两条组件目录通过生产 anchor 直达实际样例；增加定向 Recording 入口，覆盖三主题双宽的真实事件、实际绘制/命中/焦点和身份保留。
- 保留已发布 419 IDs、39 个样例函数及调用顺序、所有非目标支持条目和 Core/API/Theme/renderer/RC/CMake；同步两条 overlay、生成目录及 API 文档。

## Capabilities

### New Capabilities

- `tree-autocomplete-gallery`: 联系人建议输入与固定项目树的真实原生 Gallery 操作合同。

### Modified Capabilities

无；当前 main specs 清单为空。

## Impact

修改范围为 Gallery definition、生产目录 anchor、frame tests、两条 support overlay/生成目录、AutoComplete/Tree 文档及本 change。业务起点为 main `c1c0bb22a1af2dfc50a5f1b1146cc8e4ef071bbe`；R1 发现的既有 Tree 焦点问题已在 094 独立修复发布，本阶段 Core、公开 API、Theme、goldens、renderer 和 CMake 冻结于 `a02a2abcd7684b9152f0c5bb40bc5ccce937ad8e`。主要风险是建议弹层与文档裁剪、富内容的焦点/草稿、树级联反馈和旧定位偏移；通过实际事件与有限视口验收。网络异步、动态树拓扑、虚拟化、拖放和新的平台专属功能不在范围内。完整 Gallery/native 在统一收尾时执行；规划和定向 Recording 不代替 GPU/Linux/系统 IME/AT 验收。
