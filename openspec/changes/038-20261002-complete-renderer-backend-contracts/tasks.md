# Tasks

## 1. 规划（平台通用）

- [x] 1.1 完成两项 capability 的 proposal/spec/design/tasks，doctor/全量 strict/diff check 通过，保存规划证据并按英文 Conventional Commit 提交；按用户授权直接进入 apply。

## 2. 后端合同整体收口（平台通用）

- [ ] 2.1 连贯迁移 atlas source view、R8 validator、SDL mapped transfer packing、Recording owned pixels 和全部 consumers；测试非零 offset/stride/短末行、literal pixels/零 padding、目标保留、invalid/overflow、chunk/oversize、source lifetime、cancel/commit/epoch retry，保留 shader ABI/scene/局部更新合同。
- [ ] 2.2 建立必需能力/ABI/资源输入限制 manifest 与创建/上传 preflight，SDL 查询实际 R8 能力、Recording 负例 fixtures；验证逐项 unsupported 在创建前拒绝、三类 buffer/atlas 超限在 begin 前拒绝、dirty 与附件恢复、恰好上限/空 atlas；更新 architecture/renderer contract/AGENTS/研究入口，在 Windows/MSVC 的 windows-msvc-headless Debug/Release 完整 CTest 验证合同与 allocation，doctor/strict/diff check 后记录通用证据并提交。

## 3. Windows 原生验收

- [ ] 3.1 使用 windows-msvc-debug/release 构建全部 native 目标并运行受影响 Glyph/texture batch/scene/frame/组件/allocation/shader/font/lifetime tests；两种配置实际 Gallery resize、Typography 系统 scale/2.0、Selection acceptance，检查截图、退出码和最终二进制 SHA256，保存独立 Windows 证据，doctor/strict/diff check 后提交。

## 4. Linux 原生验收

- [ ] 4.1 在实际 Linux 环境使用现有 GCC 或 Clang native preset 构建 Debug/Release，验证 Vulkan texture upload、受影响 shader/font/lifetime 及真实 Gallery resize/Typography/Selection，独立记录与提交 Linux evidence；不重复平台通用合同，Windows 结果不替代此项。
