# Spec Delta

## Purpose

提供符合 RynUI 原生组件与 Ant Design 6.6.5 静态设计合同的加载占位结构，使应用在首屏加载时表达标题、段落及 avatar，并在显示真实内容时保持组件身份与原生输入行为。

## ADDED Requirements

### Requirement: Typed static skeleton composition

系统 SHALL 提供 typed reactive Skeleton 和 typed content，以及独立 SkeletonAvatar、SkeletonButton、SkeletonInput；title、paragraph、avatar、round、size、shape 和 block 仅暴露实际支持的语义，占位本身无焦点或点击目标。第一版 MUST 明确为静态 partial，不提供 active/shimmer、Image/Node 或 Web/AT 支持声明。

#### Scenario: Structural composition
- **WHEN** 应用改变标题、段落行数或宽度、avatar 与 standalone 形态
- **THEN** 实际逻辑占位更新到指定结构，普通更新不重执行无关 slot，尺寸颜色间距圆角只取 Theme

### Requirement: Finite responsive logical scene

系统 SHALL 在有限、无界、短、零 constraints 中分配独立 avatar/body/行区域并绘制合法 logical CPU scene v3 quads。title 和行宽为正文区域比例，standalone block 遵守父宽。每个区域不得越过其有限分配，恢复可用空间后 SHALL 正常恢复。

#### Scenario: Narrow and short recovery
- **WHEN** 在 Default/Dark/Compact 下缩小宽高到短区域和零，再恢复
- **THEN** quad bounds、radius、content glyph/hit clip 合法且不跨区，恢复后尺寸与布局正确

### Requirement: Retained loading content boundary

loading=true SHALL 保留真实 content 挂载但禁止其绘制、pointer、focus、popup、IME 和隐藏动画活动；loading=false SHALL 恢复相同组件、editor、draft、refs 身份及原生控件行为。无 content 且 loading=false SHALL 完全不占布局或相邻 gap；有 content SHALL 遵守外部 LayoutStyle。

#### Scenario: Editor and popup loading round trip
- **WHEN** Input 有普通 draft/preedit，或未聚焦 Select 有打开 popup，随后 loading 往返
- **THEN** preedit 与输入会话清除，popup 受抑制且 controlled 拒绝关闭也不泄漏，普通 draft/refs/组件身份保留，恢复可继续编辑或操作

#### Scenario: Reentrant cleanup and independent owners
- **WHEN** 输入清理的 focus/user callback 同步卸载或重建 Skeleton，且另有独立 input block owner
- **THEN** 不访问旧代 state，只注销自身完整 generational owner，其他 owner 的输入门禁保持有效

### Requirement: Theme publication and idle

Skeleton SHALL 完整参与 seed/algorithm/override/inherit、snapshot identity/hash/serialization/equality 与颜色/度量 dirty 链。algorithm=false SHALL 忽略局部 seed；最终派生度量必须在发布前验证。非法父更新影响后代时 SHALL 原子拒绝且允许修正恢复。静态占位 idle 不持续请求下一帧。

#### Scenario: Theme changes preserve old values
- **WHEN** 切换算法、继承与局部覆盖，并从新 binary 生成五份 token goldens
- **THEN** 新 Skeleton 数据符合派生合同，父基线全部旧 typed values 和 31 个 IIFE 保留，仅追加新 identity/字段

#### Scenario: Invalid derived metrics recover atomically
- **WHEN** 最终 Skeleton 派生长度无效，或父更新使 heap 后代结果无效
- **THEN** 整次发布拒绝且旧 snapshot/配置不变，随后合法更新成功；idle 无持续帧请求
