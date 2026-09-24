# Design

## Context

参见 `proposal.md`。013 已把窗口资源定义为 `WindowComponentServices`，015/016 又把动画与 surface 通用实现迁出，但其全部非内联成员定义仍位于 `button_component.cpp`。这些方法仅操作窗口服务成员和 `WindowComponentParticipant` 接口；Button 宿主通过已有公开内部 accessor 借用服务。

## Goals / Non-Goals

**Goals:** 使窗口服务拥有独立翻译单元和显式 CMake 源文件；保持构造/析构、participant 次序、异常恢复、文本编辑唯一绑定、frame/scene 同步及 ABI 等价。

**Non-Goals:** 不重新设计 participant 接口，不改 Button 的兼容宿主入口，不移动 Button 自己的 Token/视觉/按压逻辑，也不增加公开 API。

## Decisions

### 1. 精确迁移成员定义

将连续的 `WindowComponentServices::` 定义从 `button_component.cpp` 移到 `window_component_services.cpp`，只加入该翻译单元所需的标准头。保留 `window_component_services.hpp` 现有声明与成员顺序，因此对象布局和同步路径不变。相比让 Button 源文件 `#include` 一个实现片段，独立源文件可由 CMake 明确跟踪并独立增量编译。

### 2. 保留 Button 宿主转发

Button 的自有/借用 `WindowComponentServices` 构造、`button_scene()` 便利接口和现有兼容方法留在 Button 翻译单元。这样本轮只改变定义归属，不连带改写 Gallery、现有测试夹具或公开组件声明。后续新增 consumer 可只引用窗口服务头与通用设施。

### 3. 验证范围

用正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug 构建先暴露缺失符号/include；跑窗口服务生命周期、Button/Input/Search/Selection/Gallery 定向 CTest，再运行一次完整 CTest 以覆盖依赖、公开头、shader 和缓存。Release 构建和一项真实 Win32/D3D12/DXIL/系统字体 Gallery 回归覆盖 Windows 集成。平台通用合同不要求 Linux 重复，Linux 原生 Wayland 验证暂缓。

## Risks / Trade-offs

- [迁移遗漏或定义重复] → 编译/链接报错；核对所有 `WindowComponentServices::` 定义只出现在新源文件并运行 Debug/Release 正式构建。
- [新增翻译单元缺少直接 include] → MSVC 构建失败；保持头文件自包含并只添加必需标准头。
- [潜在生命周期或帧顺序变化] → 不改方法体和成员顺序；用混合窗口、scene/idle 和实窗回归比较。

## Migration Plan

先提交计划；随后迁移实现并独立提交通过定向验证的源码阶段；最后记录完整 CTest 与 Windows 实窗证据并独立提交。没有持久数据或发布迁移，回退仅需撤销对应源码提交。Linux 证据待用户提供原生环境后单独补齐。
