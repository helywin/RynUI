# Tasks

## 1. 平台通用：主题与公开合同

- [x] 1.1 实现 Rate typed API、规范化模型、Theme 全链和文档；通过 rate_model、rate_theme、rate_public_api 定向测试。
- [x] 1.2 生成五份 fresh Theme goldens，深比较全部旧 typed 字段，仅允许 rate 与 identity 变化；核对全部旧 IIFE 与 TokenIdentity 原文/顺序。

## 2. 平台通用：retained 原生组件

- [x] 2.1 实现鼠标/键盘/受控拒绝、真实半字符、逐项 Tooltip、size 与主题更新；通过 rate_component 的真实事件、glyph/quad/clip 断言。
- [x] 2.2 实现有限宽/短高/零尺寸恢复、隐藏 flow、count 身份、输入边界与代际清理；通过 rate_lifecycle 定向测试并更新 docs/rate.md。

## 3. 平台通用：本机集成检查

- [x] 3.1 在 Windows MSVC windows-msvc-headless Ninja Multi-Config、最多 2 jobs 完成上述 Debug/Release 定向 CTest，记录精确命令和数量；不运行历史 full suite 或长 Gallery。
- [x] 3.2 完成 clang-format 22.1.3 check、OpenSpec 1.14 doctor/strict、git diff --check，保留 1 MiB 默认 PE 栈；中文证据记录 HEAD/worktree/边界并以本地英文阶段提交交付父 review。

## 4. 平台通用：父合并前发现的字体上限补正

- [x] 4.1 在 0190d4b1 的生产实现上复现大字号先发布后失败；按生产 float 乘积与 round 量化校验三尺寸 normal/hover，在 resolve/preflight 拒绝 FontRuntime 不接受的最终像素；新增直接、继承组合、通知原子性与有效恢复回归。
- [x] 4.2 仅构建/执行此补正的 MSVC HEADLESS Debug/Release 定向测试，确认五份金样无变更，通过 format/explicit OpenSpec 1.14 doctor/strict/diff，保存英文 local commit 并交接；不声称已接受 Astra 独立审查。

## 5. 平台通用：Astra full R1 四项 P2

- [x] 5.1 在 cea1552c 的原生产实现逐项真实复现几何归零、鼠标间隙、后代 Tab 退出和键盘 modality 问题；保留 before 证据后最小修复，以真实事件、outline coverage、身份/清理/恢复与 deadline 验证。
- [x] 5.2 完成新回归和直接相关代表的 MSVC HEADLESS Debug/Release 定向验证，保持字号上限与 shared clip 修复，通过 format/explicit OpenSpec 1.14 doctor/strict/diff；英文 local commit、clean HEAD 后停待父 fresh Astra R2。
