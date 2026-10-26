# Tasks

## 1. 规划合同

- [x] 1.1 核对基线、本地 6.6.5 文档/源码/token/snapshot、Image 边界并完成中文规划；doctor、strict validate 与 diff check 通过

## 2. 平台通用实现（实际 Windows MSVC headless）

- [ ] 2.1 实现 Avatar/AvatarGroup typed API、Theme 全链与 append-only identity；Theme 定向测试及五份 fresh goldens 旧 typed 字段深比较通过
- [ ] 2.2 实现 retained Avatar 测量/auto-fit/富 slot/裁剪/显隐与 Group overlap/max/原生 Popover；定向 component 测试覆盖实际 glyph/quad/hit、有限宽/短高/零恢复、身份、草稿/ref、同步卸载和 gate 清理

## 3. 定向收口

- [ ] 3.1 windows-msvc-headless 的 Ninja Multi-Config Debug/Release（最多 2 jobs）定向 build/CTest 通过，保留默认 1MiB PE stack 并记录精确命令/数量/日志
- [ ] 3.2 公开 docs/avatar.md 示例与限制准确；clang-format 22.1.3 check、OpenSpec 1.14 doctor/strict、git diff --check 通过，本地英文阶段提交完成，交父审查
