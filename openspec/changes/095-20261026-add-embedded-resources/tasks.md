# Tasks

## 1. 规划与来源核对

- [x] 1.1 核实现有 shader 部署/读取、系统字体和内存字体链路，完成中文规划与边界；显式 OpenSpec 1.14 doctor、全量 strict 和 diff check 通过，记录源依据，不把规划计为实现。

## 2. 平台通用资源基础（本机 Windows / MSVC HEADLESS）

- [ ] 2.1 实现只读资源包 API、qrc prefix/alias 子集、CMake 生成与 depfile；docs/resources.md 给出实际可编译用法，定向 Debug/Release 验证二进制/空文件/缺失、非法/重复 alias、静态库引用、空格/Unicode 路径、资源修改后的增量构建与 no-op，不引入 Qt/SDL/平台依赖。

## 3. 平台通用接入与构建合同

- [ ] 3.1 将实际编译的 12 个 DXIL/SPIR-V shader 嵌入 renderer，两个 renderer 与示例改用 byte span，移除旁置 shader 部署；通过资源内容与既有 shader/renderer 合同检查、HEADLESS 分层 configure/build，更新对应构建文档。
- [ ] 3.2 普通示例默认不指定/复制验收字体，保留显式 custom/fallback 与验收配置；更新资源用法、字体和架构说明，通过示例/API 构建与源配置合同，不改变系统字体发现或假称未覆盖字形存在。

## 4. Windows 原生发布验收

- [ ] 4.1 使用官方 windows-msvc-debug/release、Ninja Multi-Config、最多 2 jobs 实际构建示例与字体目标；复制 minimal 和 Token Gallery exe 至无 shaders/fonts 的独立目录，并从另一 cwd 执行原生 smoke，核实 Direct3D12/DXIL、实际字体来源/字形、退出码、文件 hashes 及日志；系统字体测试通过，保留原警告与平台边界。

## 5. 平台通用集成收口

- [ ] 5.1 通过全新 Astra 独立审查并修复可达问题，保持既有 Theme/IIFE/ordinal 与 GPU ABI；执行受影响定向检查、format 22、显式 OpenSpec 1.14 doctor/strict 和 diff check，记录当前构建/部署证据、提交推送 main 并核对远端 SHA。

## 6. Linux 原生发布验收（独立平台，待实际机器）

- [ ] 6.1 在实际 Linux 使用正式 GCC/Clang preset 构建并复制 exe 至无 shader/font 资源目录，从另一 cwd 验证 Vulkan/SPIR-V 与 Fontconfig 系统字体、退出码和日志；独立记录及提交，不重复平台通用逻辑测试，不由 Windows 结果勾选。
