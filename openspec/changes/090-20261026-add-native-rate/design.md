# Design

## Context

基线为 8056de67953ee73015c7559f1782033d85e08573。参照本机 `D:/code/RynUI/out/upstream-ant-design-6.6.5`（4a39f54842eade4e565ab336ef6097cd7e723cdd）的 Rate 文档、index.tsx、style/index.ts 与 snapshots。现有 Segmented 提供原生输入/slot 裁剪范式，Timeline 提供主题追加与隐藏/代际清理范式。

## Goals / Non-Goals

实现 proposal 与 native-rate spec 的 retained 首版。只新增组件拥有的布局、逻辑场景、输入与主题；不修改 renderer ABI、宿主入口、Gallery 或已有组件行为。

## Decisions

- `RateCharacter` 为文本或 `IconSource` 的 typed 值，可按 index 函数生成并响应更新。背景和前景分别保留一份纯绘制节点；不执行两份任意原生子控件 slot，避免重复 ref/编辑草稿/焦点。Tooltip 复用现有 typed trigger/title 合同。
- 每项具有稳定 ComponentId、interaction、前景/背景 viewport；count 增加只追加，缩减以 inactive/out_of_flow 保留已建项，最多 1000，恢复保持已建身份。count=0 与 visible=false 根节点退出 flow。
- 保存原始权威值与规范化值；受控候选回调前清预览，回调后不再使用借用状态。所有 callback、focus cancel、boundary reconciliation 均可能同步卸载，使用完整 generation ID 重新查询。
- 单一可聚焦根 interaction 接受键盘，项 interaction 负责指针；Tooltip trigger 不增加 Tab stop。Left/Right 每次变化 .5 或 1，Home=0、End=count，Enter/Space 提交当前位置。上下键不吞掉。此为原生 keyboard 合同，不复制 snapshot 的多个 tabindex。
- Theme 追加 Rate 派生块和 identities，既有 IIFE 原文顺序不改；全链包括最终尺寸校验及祖先 preflight。大 ThemeConfig/Snapshot 测试对象放 heap，保持默认 1 MiB PE 栈。
- 尺寸自然宽为所有项边长加 marginXS 间距；有限分配按自然位置裁剪，不压缩星形以伪造半选。每个 viewport 同时约束绘制和命中，focus decoration 与内容 clip 分离。无动画 deadline；hover scale 用静态几何同步，闲置不请求下一帧。
- 富 Tooltip title slot 只挂载一次，readOnly 仅取消评分自身 focus/tab/edit，不关闭作为原生后代 interaction parent 的根 eligibility。根命中范围以窗口为总界，composer 仍单独对每个文档 interaction 施加 document clip；已有 Tooltip 窗口层中的原生子控件可在文档区域之外命中。

## Risks / Trade-offs

- 大 count 的 retained 节点占用 → 限制 1000，按实际 count 懒追加；不引入虚拟化。
- 非法 Prop 更新或同步卸载 → 验证先于写入，所有外部回调前复制回调与值，之后重新查询。
- 文本与图标半选出现不同边界 → 同一 viewport clip，测试读取实际 glyph 与 quad 内容。
- 本机 PATH 默认 OpenSpec 为 1.13.1 → 明确使用已安装 pnpm global/v11 的 1.14.0 CLI，不升级全局环境。
- 只做 Windows MSVC `windows-msvc-headless`（Ninja Multi-Config，最多 2 jobs）Debug/Release 定向验收；不以 HEADLESS/Recording 代替 native/GPU/Linux/IME/AT。
