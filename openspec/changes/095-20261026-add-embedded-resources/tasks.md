# Tasks

## 1. 规划与实现方式修正

- [x] 1.1 核实现有磁盘 shader、字体链和旧 exe 独立目录失败；按用户纠正改用标准 rc，撤销未提交 qrc 原型，保持规划/设计/spec/tasks 一致，通过显式 OpenSpec 1.14 doctor/strict 与 diff 检查后提交。

## 2. 构建与示例接入

- [ ] 2.1 为固定 shader 集接入标准 rc 和非 Windows 内嵌数据，两个 renderer 使用私有只读 bytes；资源依赖正确、无新清单/公开打包 API，通过 shader 内容/现有反射合同与 HEADLESS 分层检查，并更新资源构建文档。
- [ ] 2.2 普通示例默认系统字体，不再指定/复制固定验收字体；显式应用/验收字体配置保持，通过对应构建/配置检查，更新字体与架构说明。

## 3. Windows 原生发布验收

- [ ] 3.1 使用正式 windows-msvc-debug/release、Ninja Multi-Config、最多 2 jobs 新构建 .rc、两个 renderer 与示例，验证内嵌 shader bytes 与全部实际输入一致及资源更新，系统字体测试和受影响合同通过。
- [ ] 3.2 复制新 minimal 和 Token Gallery exe 到无 shaders/fonts 的独立目录，从另一 cwd 实际执行原生 smoke；核对 Direct3D12/DXIL、实际字体/字形、退出码、hashes 和日志，不沿用旧二进制通过结果。

## 4. 集成收口

- [ ] 4.1 经全新 Astra 独立审查并修复可达问题，保持 Theme/IIFE/ordinal/GPU ABI，受影响检查、format22、显式1.14 doctor/strict、diff 通过；记录准确证据并只推 main、核远端 SHA。

## 5. Linux 原生发布验收（独立平台，待实际机器）

- [ ] 5.1 在实际 Linux 正式 GCC/Clang preset 验证内嵌 shader、Vulkan/SPIR-V、Fontconfig 系统字体及无资源目录启动，独立记录/提交，不重复平台通用逻辑，不由 Windows 结果勾选。
