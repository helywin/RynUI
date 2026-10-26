# Tasks

## 1. 规划与锁定合同

- [x] 1.1 核对基线、上游文档/源码/token/snapshot 并完成中文规划，通过 OpenSpec doctor、全量 strict validate 和 diff check，保存本地规划提交。

## 2. 平台通用实现（本机 Windows / windows-msvc-headless）

- [x] 2.1 实现 typed value/Props/slots、稳定格式化与完整 Theme 全链；定向 model/theme/public API 测试和 5 份 fresh golden 深比较通过，旧 IIFE 原文/顺序及旧 identity 保持。
- [x] 2.2 实现 retained Statistic title/value/prefix/suffix、Skeleton loading、可见性、布局与服务注册；定向真实场景测试覆盖属性/Theme 更新、草稿/ref 身份、有限宽/短高/零尺寸、实际 R8 glyph/quad/hit clip、loading/hidden 输入边界、同步卸载和资源清理。

## 3. 平台通用集成验收（本机 Windows / MSVC）

- [ ] 3.1 docs/statistic.md 的公开 API 示例实际编译运行；windows-msvc-headless Debug/Release 定向 build/CTest 通过（Ninja Multi-Config、最多 2 jobs、默认 1 MiB PE 栈），记录命令、日志、数量和覆盖边界。
- [ ] 3.2 clang-format 22.1.3 check、OpenSpec 1.14 doctor/全量 strict validate、git diff --check 通过，证据落盘并保存本地英文 Conventional Commits，返回精确 HEAD/path/branch/dirty 待父审查。
