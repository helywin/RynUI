# Tasks

## 1. 平台通用：主题与公开合同

- [x] 1.1 实现 Rate typed API、规范化模型、Theme 全链和文档；通过 rate_model、rate_theme、rate_public_api 定向测试。
- [x] 1.2 生成五份 fresh Theme goldens，深比较全部旧 typed 字段，仅允许 rate 与 identity 变化；核对全部旧 IIFE 与 TokenIdentity 原文/顺序。

## 2. 平台通用：retained 原生组件

- [ ] 2.1 实现鼠标/键盘/受控拒绝、真实半字符、逐项 Tooltip、size 与主题更新；通过 rate_component 的真实事件、glyph/quad/clip 断言。
- [ ] 2.2 实现有限宽/短高/零尺寸恢复、隐藏 flow、count 身份、输入边界与代际清理；通过 rate_lifecycle 定向测试并更新 docs/rate.md。

## 3. 平台通用：本机集成检查

- [ ] 3.1 在 Windows MSVC windows-msvc-headless Ninja Multi-Config、最多 2 jobs 完成上述 Debug/Release 定向 CTest，记录精确命令和数量；不运行历史 full suite 或长 Gallery。
- [ ] 3.2 完成 clang-format 22.1.3 check、OpenSpec 1.14 doctor/strict、git diff --check，保留 1 MiB 默认 PE 栈；中文证据记录 HEAD/worktree/边界并以本地英文阶段提交交付父 review。
