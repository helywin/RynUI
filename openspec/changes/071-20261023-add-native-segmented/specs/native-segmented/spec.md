# Spec Delta

## Purpose

提供面向少量固定选项的原生互斥分段选择，让应用使用稳定键、响应属性和 typed slots 构建紧凑的视图切换，并保持跨输入方式一致的焦点、选择与生命周期行为。

## ADDED Requirements

### Requirement: 固定选项与响应选择

Segmented SHALL 接受唯一非空 String key 的固定选项，提供 typed Props、响应 label/disabled 和展示型 label/icon slots。SHALL 支持 controlled value、defaultValue 与仅在用户请求不同可用 key 时触发的 onChange；controlled/default 冲突 MUST 拒绝。

#### Scenario: 受控拒绝与属性更新
- **WHEN** 应用没有回写 onChange 请求，随后更新 label 或 disabled
- **THEN** 选中值保持应用 value，选项及 slot 身份保持不变；禁用和未知显式值不被自动替换

#### Scenario: 默认选择与非法选项
- **WHEN** 没有显式值或者传入空/重复 key
- **THEN** 前者选择首个可用项（没有可用项则无选择），后者在执行 slots 前拒绝挂载

### Requirement: 原生布局与 Theme 视觉

Segmented SHALL 提供水平/纵向、自然宽度/水平 block 等宽、Small/Middle/Large，并在受约束宽度下正确放置真实 item 和富标签。SHALL 由独立 Theme tokens 表达 selected、hover、pressed、focus-visible、disabled；纯颜色更新 MUST 保持布局和节点身份。

#### Scenario: 等宽与富标签
- **WHEN** 容器拉伸或在 block 中使用长的 rich label 与 icon
- **THEN** 实际 item bounds 填满所分配等宽，内容在其内测量和换行，不依赖仅测量时相等的虚假几何

### Requirement: 输入资格与生命周期

Segmented SHALL 提供一组一个逻辑 Tab 入口、方向匹配的循环箭头、Home/End 与 Enter/Space，跳过禁用项，并支持 pointer 点击和外部释放取消。MUST 遵守 Modal/Spin 逻辑输入资格并允许 onChange 销毁自己或祖先，销毁后不得保留资源或输入捕获。

#### Scenario: 边界与销毁
- **WHEN** 控件被 Modal/Spin 阻挡或者回调销毁所属分支
- **THEN** 阻挡时不响应选择，销毁时安全清理 retained node、interaction、focus 和 effects
