# Spec Delta

## Purpose

为 RynUI 详情页提供可组合原生控件的多列标签和值面板，通过确定性 span 排布、响应式外观与持久化子树，在有限布局空间内保持可读信息、编辑草稿与原生事件生命周期。

## ADDED Requirements

### Requirement: 固定身份与响应式 API

Descriptions SHALL 提供 typed Props、label/content/title/extra slots 与 reactive Prop；item key MUST 非空且唯一，column/span MUST 为正整数。普通属性变化 SHALL 保留所有 slot 身份，不执行无关组件。

#### Scenario: 配置与草稿更新
- **WHEN** 修改内容、标签、列数、span 或显隐
- **THEN** 使用现有子树重排，隐藏项离开 flow，恢复后编辑草稿和身份保留；非法配置被拒绝

### Requirement: 多列 packing 与外观

Descriptions SHALL 按声明顺序填列，超出行尾的 span 截至剩余列；最后不足一行时扩展末项。组件 SHALL 支持 bordered/unbordered、水平/垂直标签、Default/Middle/Small、title/extra；colon 仅用于非 bordered 标签。

#### Scenario: 不整除的 span
- **WHEN** 三列依次收到 span 2、2、1
- **THEN** 第一行是 2、1，第二行末项扩展为 3；隐藏中间项后重新按可见项排布

### Requirement: 有限空间与原生交互

Descriptions SHALL 在有限、零或短宽高约束内发布有限非负 geometry 并裁剪 glyph/surface/hit。富内容 SHALL 保持 Button/Input/Select 事件、焦点和 popup 边界；隐藏及卸载 SHALL 取消 owned 输入状态，不影响其他容器。

#### Scenario: 隐藏正在编辑的内容
- **WHEN** item 隐藏或祖先 Modal/Spin 阻止其输入
- **THEN** 旧输入会话、捕获及 popup 失效，恢复时保留编辑值；卸载后其他输入仍可使用

### Requirement: Theme 与被动调度

Descriptions SHALL 完整支持 component seed、algorithm、override、继承、identity、诊断序列化和 colors/metrics dirty。算法关闭 SHALL 忽略 component seed；最终派生 token MUST 在发布前验证，字体至少 0.5 logical dp。普通更新 SHALL 通过正常 dirty 唤醒，idle SHALL 无连续帧。

#### Scenario: 非法后代主题
- **WHEN** Theme 更新使后代 Descriptions 最终字体或 metrics 非法
- **THEN** 更新原子拒绝且旧 snapshot 可继续使用，修正配置后恢复；五组旧 golden typed values 保持不变

### Requirement: 首版边界

文档 SHALL 明确固定拓扑、等分列、无 locale/RTL、无自动响应断点、无 Web DOM/CSS/AT 及 Gallery/platform 验收边界，不把 headless 验证描述为真实窗口或 GPU 通过。

#### Scenario: 用户读取组件文档
- **WHEN** 用户查阅公开用法及证据
- **THEN** 能分辨已支持的原生行为、显式限制和实际 MSVC-headless 覆盖
