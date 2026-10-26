# Proposal

## Why

示例 exe 当前从旁置 shaders 目录读取二进制，离开开发目录就不能启动。用户已明确“qrc 只是举例，使用 rc”，本变更采用 Windows 原生资源，并让普通示例默认使用系统字体 fallback。

## What Changes

- Windows 直接使用 .rc / RCDATA 将实际 shader 二进制链接进 exe，通过原生资源 API 读取。
- 只提供现有固定 shader 集的 CMake 接入，其他平台用简单内嵌 C++ 数据保持同一只读入口；不新增资源清单语言、XML 解析器、资源注册系统或公开 ResourceBundle API。
- 两个 SDL renderer 和示例去掉运行时 shaders 路径依赖；保留现有 shadercross 编译、格式选择和 renderer ABI。
- 普通示例默认走系统字体链，不默认复制/读取验收字体；应用和验收显式字体配置保持。
- 用真实资源内容检查和无 shaders/fonts 独立目录中的原生启动验证结果。

## Capabilities

### New Capabilities

- `embedded-resources`：原生内嵌 shader、标准 rc 接入和可搬移示例的运行合同。

### Modified Capabilities

无；当前主 specs 目录为空。

## Impact

影响 shader CMake 接入、SDL backend 私有资源读取、示例入口/构建、字体默认配置说明和定向测试。Windows SDK 仅出现在具体 backend，Core 和 renderer/common 不增加平台依赖。非目标是 qrc/rcc、Qt、通用资源打包器、压缩、加密、热更新、网络资源或新字体引擎。用户纠正前的未提交 qrc 原型已撤销，不作为实现或验收证据。
