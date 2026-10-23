# Proposal

## Why

073 正在提供 Empty/Result 原生组件，但当前 Gallery 仍只有离线参考条目，使用者无法直接体验空状态操作和结果页。补齐实际样例，让新增组件能与已有 Button/Input 一起使用。

## What Changes

- 在完整 Gallery 尾部追加 Empty 默认/紧凑/自定义图示及 Result 状态/异常/详情/操作样例，保留已有样例顺序与身份。
- 提供真实操作切换描述、图示和结果状态；在保留 slot 中放置可编辑草稿，验证更新不重挂载控件。
- 更新稳定测试标识、catalog/support overlay 和用户文档；仅将实际实现范围标为 partial，插画等缺口明确记录。
- 增加聚焦 Recording 场景，验证实际控件、布局与 glyph 裁剪，并执行 Gallery 启动 smoke；不把自动 smoke 当成人工视觉或系统输入验收。

## Capabilities

### New Capabilities

- `empty-result-gallery`: 原生空状态和结果页的可操作 Gallery、保留内容、主题切换与有限视口布局。

### Modified Capabilities

无；当前主规格目录为空。

## Impact

依赖已修复并集成的 073 Empty/Result。涉及 token_gallery_definition、生成目录、Gallery frame tests、6.6.5 support overlay、组件文档和完成清单；不修改 renderer/GPU ABI 或引入依赖。主要风险为长文档布局、Theme 切换时的保留身份以及短视口裁剪。Windows/Linux 人工视觉、GPU 组件矩阵和系统 IME 继续独立验收。
