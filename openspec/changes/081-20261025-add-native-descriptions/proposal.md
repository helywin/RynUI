# Proposal

## Why

RynUI 已有 Form 和常用输入组件，但详情页仍需手工拼装标签和值。新增原生 Descriptions，为业务提供可响应重排、保留富内容身份的多列详情面板。

## What Changes

- 提供 `ryn::Descriptions`、typed Props 与 label/content/title/extra slots，固定非空唯一 key，支持响应式正整数 column/span 和 visibility。
- 按锁定 Ant Design 6.6.5 的 useRow 规则 packing，支持 bordered、水平/垂直标签、Default/Middle/Small 和适用的 colon。
- 实现 retained layout、真实文字/边框与有限约束裁剪；富内容中的 Input/Button/Select 保持事件、焦点、草稿和 popup 生命周期。
- 增加独立 Descriptions Theme seed/algorithm/override/inheritance、分组 dirty 和序列化，保留旧字段及旧解析逻辑。
- 增加针对性测试、组件文档和平台通用证据。本 change 不处理 Gallery/catalog、README 或全局完成状态。

## Capabilities

### New Capabilities

- `native-descriptions`: 固定拓扑原生多列详情布局、富内容保留与 Theme 合同。

### Modified Capabilities

无。

## Impact

涉及公开头、组件 host/model、WindowComponentServices、Theme token/snapshot/runtime、CMake 和测试。无新第三方依赖，不改变 logical scene v3、GPU ABI 或 Core/renderer 边界。首版不含动态增删/排序、locale/RTL、自动响应断点、Web DOM/CSS、AT、Gallery 或真实平台视觉验收。风险集中在有限空间、隐藏输入清理和大型 Theme 的 Windows 栈使用，均需真实 headless 数据验证。
