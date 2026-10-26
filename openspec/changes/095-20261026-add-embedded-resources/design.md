# Design

## Context

基线 main `e28fcb10` 的 shader 仍由 ifstream 从 exe/shaders 读取。现有 minimal exe 复制到无资源目录后，实际报 `Unable to open shader`，旧二进制复现记录在 `out/095-relocation-before-1793024363671/`。字体链已有显式文件、DirectWrite/Fontconfig 系统 fallback 和内存字体能力；普通示例额外指定并复制了验收字体。

用户明确 qrc 只是类比，要求使用 rc。因此撤销未提交的 XML 清单、解析器、通用资源包 API 和对应工具；保留本变更的“exe 可脱离资源目录、默认系统字体”目标。

## Goals / Non-Goals

**Goals:** Windows 使用原生 .rc/RCDATA；两种 SDL renderer 从内嵌 bytes 创建 shader；普通示例不依赖旁置 shaders/fonts；资源修改能触发正确重建。

**Non-Goals:** 不新增 qrc 风格文件、公开资源注册/挂载 API、通用 archive/压缩/热更新，不改变 GPU ABI 或字体发现算法。

## Decisions

- **原生 rc 直接链接至 exe。** CMake 为固定的 12 个 shader 配置标准 .rc，使用保留前缀的命名 RCDATA，避免应用数字 ID 冲突。资源脚本直接加入最终 executable，避免放在静态库后没有被抽取。应用其他资源直接使用普通 .rc/target_sources，不经过框架自制格式。
- **小型私有 byte-view 入口。** Windows backend 用 FindResourceW / LoadResource / LockResource / SizeofResource 读取当前 exe，返回模块存活期内的只读 span；不调用 GlobalFree，也不让 Windows 类型泄漏到 Core/common。非 Windows 用同一固定 shader 文件列表生成简单 C++ 数据并直接链接到 exe，不重新引入 manifest 或全局注册器。
- **沿用 shader 构建。** shadercross 仍生成 DXIL/SPIR-V 和既有反射文件。资源编译显式依赖实际二进制；renderer 继续按设备选择格式，只替换磁盘读取。CMake 仅在 Windows SDL GPU 需要时启用 RC，C++ 编译选项限定语言，HEADLESS 仍可配置构建。
- **沿用系统字体。** 普通 text/button/layout/Gallery 的默认 request 不再填 exe/fonts 验收路径，也不复制固定测试字体。显式 custom/fallback 与验收入口保持，系统缺少实际文字 coverage 时保留诊断。系统字体不随程序分发。
- **实际部署验证。** Windows MSVC Ninja Multi-Config 新构建，检查 RCDATA 与编译输入逐字节一致；将 minimal/Gallery exe 复制至无 shaders/fonts 的独立目录，从另一 cwd 启动，记录 backend、字体、退出码和 hashes。Linux/Vulkan/Fontconfig 独立实际验收，不由 Windows 结果勾选。

## Risks / Trade-offs

- [资源脚本未进入最终 exe] → 最终目标显式添加 .rc，真实读取与独立目录启动测试。
- [shader 变化没有重编资源] → 显式依赖实际 shader 输出，检查更新后嵌入内容。
- [路径含空格/Unicode，或 RC 收到 C++ 参数] → 正确引用、UTF-8 rc 脚本、语言限定编译选项，正式 MSVC 构建验证。
- [系统字体覆盖差异] → 沿用现有发现和明确诊断，记录真实字体来源，不用验收字体冒充系统默认。
- [并行组件合并覆盖资源入口] → 保留本变更的 CMake/renderer/runtime 入口，合并后进行受影响检查。

## Migration Plan

先更新并提交本规划，再按 apply 实施；通过的阶段英文 Conventional Commit 后只推 main、核对远端 SHA。Astra 独立审查实现与实际证据，不自动 PR/archive。

## References

已核对 Microsoft 官方 RCDATA、FindResourceW、LoadResource、LockResource 和 SizeofResource 文档。资源 bytes 生命周期属于当前 exe 模块；字体 fallback 复用现有实现。Qt/qrc 不作为本实现的接口或文件格式。
