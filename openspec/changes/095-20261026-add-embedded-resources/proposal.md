# Proposal

## Why

示例 exe 当前通过 `executable/shaders` 读取二进制 shader，离开同目录资源后不能启动。用户要求类似 Qt qrc 的资源系统，并让字体默认使用系统字体 fallback，避免发布时依赖示例旁的验收资源目录。

## What Changes

- 新增无 Qt/SDL 依赖的只读内嵌资源包 API，以及支持 prefix/alias 的 qrc 子集和 CMake 构建时生成工具；资源按稳定路径读取原始 bytes，不依赖 cwd。
- 将实际编译的 DXIL/SPIR-V shader 嵌入 SDL renderer，覆盖 Quad/Glyph/RoundedEffect 的 vertex/fragment，示例启动不再读取旁置 shaders。
- 普通示例默认使用现有系统字体链，不再默认复制/读取锁定验收字体；保留应用显式字体与 fallback 配置，测试仍可显式使用锁定字体。
- 增加构建依赖、静态库链接、资源内容及默认字体验证，并把 exe 复制到无 shaders/fonts 的独立目录、切换 cwd 后实际启动。
- 非目标：引入 Qt、完整 qrc/rcc 兼容、压缩/加密、动态资源挂载、热更新、网络资源或替换已有字体/shader 编译器。

## Capabilities

### New Capabilities

- `embedded-resources`：只读资源包、构建时嵌入和脱离旁置资源的原生示例启动合同。

### Modified Capabilities

无；当前主 specs 目录为空。现有平台字体发现与 renderer ABI 保持，架构文档将明确普通示例和显式验收字体的职责。

## Impact

涉及公开 resource header、CMake helper/生成脚本、renderer SDL 的 shader 加载、示例构建与入口、默认字体文档和定向测试。资源基础层不 include/link SDL、renderer 或系统字体服务；shader 包属于 renderer。风险是生成输入依赖遗漏、静态库 dead stripping、资源别名冲突、Multi-Config 产物以及系统字体覆盖差异；用实际构建和独立目录原生启动验证。Windows 与 Linux 原生验收分别记录，只推 main，不自动 PR/archive。
