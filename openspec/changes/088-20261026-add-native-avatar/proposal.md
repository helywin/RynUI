# Proposal

## Why

RynUI 尚无原生 Avatar。需要在现有 retained scene、字体测量和 Popover 基础上提供可用的文字/图标头像及受限头像组，避免应用自行拼装布局与 overflow。

## What Changes

- 新增 typed Avatar：circle/square、small/medium/large/数值尺寸、gap、真实 shaping 测量的文字 auto-fit、retained icon/content 与显隐。
- 新增固定 key AvatarGroup：继承尺寸/形状、overlap、现代 typed `max.count`、仅真实溢出时出现 `+N` 与原生 Popover 展示。
- Avatar Theme 完整接入配置、继承、独立算法、identity/序列化/dirty；新增字段不得改变旧组件 recipe 与 typed golden 字段。
- 新增公开 `docs/avatar.md` 与定向 native headless 验证。
- 非目标：图片 src/srcSet、联网/文件解码、响应式断点尺寸、DOM/CSS 属性；仓库未提供公开 Image 管线，不添加 stub。不修改 Gallery、README 或生成目录。

## Capabilities

### New Capabilities

- `native-avatar`：原生文字/图标头像、retained 固定 key 头像组与 overflow。

### Modified Capabilities

无。

## Impact

修改公开 header、component host/model、WindowComponentServices 注册、Theme 全链、CMake 与本组件测试。无第三方依赖、renderer/GPU ABI 或平台输入 API 修改。风险集中于实际字体 auto-fit、受限裁剪、overflow 子树身份与同步卸载；以 logical scene/glyph/quad/hit 和生命周期断言验收。验证使用 Windows MSVC `windows-msvc-headless` Debug/Release，最多 2 jobs；不宣称 GPU、真实窗口、Linux、系统 IME 或 AT 已验收。
