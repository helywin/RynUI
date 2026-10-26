# Design

## Context

基线 main `8056de67953ee73015c7559f1782033d85e08573`。已核对本地 Ant Design 6.6.5 `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 avatar 文档、Avatar.tsx、AvatarGroup.tsx、style/index.ts 与 demo snapshot。现代尺寸名 medium；Group 使用 max.count，显示前 count 个原始头像后追加 +N，默认 hover/top Popover。无公开 Image pipeline。参见 proposal 的动机。

## Goals / Non-Goals

Goals：真正 retained 原生首版；文字 shaping auto-fit；可恢复受限布局；完整 Theme 与生命周期。Non-Goals：图片、响应式 breakpoint、DOM/CSS、任意 Group 富 slot、renderer/GPU ABI 修改。独立 Avatar 支持任意 typed icon/content。

## Decisions

- AvatarProps 以 AvatarSizeValue（预设或 float）、AvatarShape、text、gap、visible、layout 表达；AvatarSlots 的 icon/content 一次挂载，icon 优先。纯文字用独立 measurement scene 测量，量化字体后复测；不估算字符数。最小字体 .5 dp，无法容纳时裁剪。
- 内容使用独立矩形 viewport，clip_content=true、clip_decorations=false；circle 使用内接安全矩形，避免没有弧形 glyph mask 时泄漏到圆外。背景/边框通过现有 retained surface，不新增 shader。内容居中，有限宽/短高收缩到正方形，零尺寸能够恢复。
- AvatarGroupItem 固定 key 与 reactive text/optional IconName/visible；不接受任意用户 slot。每个条目的内联与 overflow 原生 Avatar 各保留一次，视觉共享属性，避免移动组件 ancestry 或复制带 ref/草稿的用户 slot。条目上限 1024；max.count 使用 optional uint32，0 表示只显示 +N。Popover 本身与隐藏展示都 retained，关闭不销毁；只计可见条目。
- Group inline 采用自定义布局，overlap 来自 token，step 至少 0；受限宽不改变自然相对顺序，通过视口裁剪。Popover 内容使用正常 groupSpace；对溢出区域可见性、input block 和真实 hit clip 进行断言。
- Theme 新增 Avatar config/token、append-only 两个 identities，原 IIFE 原文/顺序保持；最终 derive/override 后统一检查字体与组合 extent。大 ThemeConfig/Snapshot 用 heap；不扩大默认 1MiB PE stack。

## Risks / Trade-offs

- 内接矩形比 Web 圆形 overflow mask 保守 → 公开文档说明安全裁剪，实际 scene 验证边界。
- Group 双展示位置增加 retained 节点 → 上限 1024，隐藏展示不 rasterize/命中，不重复执行用户富 slot。
- Popover 同步边界回调可能卸载 Group → 保存完整 ComponentId，跨用户回调后重取状态；注册 block 有对称 cleanup。
- Theme 全链增量可能影响旧配方 → 生成五份 fresh diagnostic goldens，深比较旧 typed 字段，仅允许 avatar 与 identity 增量。

## Migration Plan

新增 API，无旧调用迁移。按规划、实现/主题、定向验收本地英文 Conventional Commits 保存，不访问远端。Windows MSVC 的 windows-msvc-headless、Ninja Multi-Config Debug/Release，最多 2 jobs；不以 headless 结果代替原生窗口、GPU、Linux、系统 IME/AT。父线程执行 Astra 审查与集成发布。
