# Spec Delta

## Purpose

原生 AutoComplete 为桌面表单提供可自由输入的 String 编辑器与可选择建议，保持受控数据权威、组合输入安全和保留子树生命周期。它遵循锁定 Ant Design 的输入辅助语义，并通过原生主题和公共窗口服务呈现。

## ADDED Requirements

### Requirement: 自由文本与事件语义

AutoComplete SHALL 接受任意 String，支持 reactive value 或一次性 defaultValue，并拒绝二者同时设置。输入提交 SHALL 发出 onChange 和 onSearch；选择建议 SHALL 按顺序发出变化的 onChange 和 onSelect，不发出 onSearch。受控 value SHALL 保持外部权威。

#### Scenario: 输入与选择
- **WHEN** 用户输入集合外文本后选择建议
- **THEN** 输入文本被保留，选择发出建议 String 和 typed option，且不产生 search 回调

### Requirement: 动态建议与保留身份

建议 SHALL 为具有唯一非空 key 的 flat typed options，允许 String label 或 typed rich label。默认 SHALL 不过滤，显式过滤 SHALL 支持按 value 子串或 typed predicate。更新与重排同 key 建议 SHALL 保留 label 子树与输入编辑器身份；删除 SHALL 释放对应资源。

#### Scenario: 更新建议
- **WHEN** 建议在输入后动态增删、重排或更新 label
- **THEN** 同 key 子树保留，删除项不再命中，editor 和 preedit 不因列表更新而丢失

### Requirement: 原生交互与组合输入

AutoComplete SHALL 只有一个 Tab 输入入口，支持上下移动、Enter 选择、Escape 关闭、Tab 离开及 pointer 选择/清空。组合输入活动时建议逻辑 SHALL 不消费上下/Enter/Escape 或提交建议；Escape 交还真实 Input 取消自身 composition，不关闭 popup 或改变接受值。disabled/readOnly SHALL 禁止用户修改与建议选择。ref SHALL 使用真实 Input 的 focus/blur/select 和 generation/lifetime 检查。

#### Scenario: backfill 与 IME
- **WHEN** 开启 backfill 后键盘移动，或存在活动 preedit
- **THEN** backfill 只更新临时展示而不触发值事件，取消时恢复已接受值；活动 preedit 不被建议键盘逻辑消费

### Requirement: popup 权威与输入边界

open SHALL 支持 controlled/defaultOpen 且拒绝同时设置；受控关闭请求 SHALL 不覆盖外部 open。空过滤结果 SHALL 不显示 popup。popup SHALL 在有限视口定位并滚动，遵循最内层 popup 的 wheel/Escape 所有权；隐藏、Modal/Spin 阻断、失活或销毁 SHALL 清理命中、焦点、capture 与临时编辑状态。

#### Scenario: 空集合与受控打开
- **WHEN** open=true 且 options 为空，然后填充建议
- **THEN** 空时不显示 popup，填充后按外部打开状态呈现

### Requirement: Form reset 合同

AutoComplete SHALL 通过 FormField<String> 和 FormItem.control(ref) 组合。reset SHALL 清理 preedit、候选、临时 backfill 和滚动草稿并恢复当前接受值，包含同值 reset；正常同值 value/options 刷新 SHALL 不清理 preedit。

#### Scenario: 同值 reset
- **WHEN** 接受值未变化而 editor 有 preedit 或 backfill，调用 Form reset
- **THEN** editor 和展示恢复字段初值，不发出用户 change/search/select

### Requirement: 完整原生主题与兼容性

AutoComplete SHALL 具有完整 derive/override/algorithm/inherit/serialize/hash/equality/dirty Theme 合同，只通过 Theme 控制视觉。最终 font SHALL 按实际 round 后满足 FontRuntime 的 1..INT_MAX/64 范围，非法子主题 SHALL 在发布前原子拒绝。旧 Theme IIFE、ordinal 与旧 typed golden leaves SHALL 保持不变。

#### Scenario: 非法字体与增量主题
- **WHEN** 根或后代主题设置非法字体，或仅修改 AutoComplete 颜色
- **THEN** 非法发布不改变任一已发布 snapshot；有效颜色更新只触发对应 material 订阅
