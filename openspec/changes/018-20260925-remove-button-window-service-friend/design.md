# Design

## Context

参见 `proposal.md`。017 使窗口服务独立编译，Button 仍用三个引用成员直接绑定 `WindowComponentServices` 的私有状态。窗口服务已有 `animation_time()`、`motion_preference()` 与 `mark_scene_structure_dirty()`，可不增设 API 完成替换。

## Goals / Non-Goals

**Goals:** Button 通过窗口服务内部公开的窄接口获取动画状态和报告 scene 结构失效；消除窗口服务对 Button 的 `friend`，保持原有同步和性能。

**Non-Goals:** 不拆除 Button 的其他兼容转发/服务引用，不改变 public C++ API、Token、动画算法或视觉效果。

## Decisions

### 1. 按使用点读取当前窗口状态

删除 `ButtonComponentHost` 的三个引用成员，在原有读取位置调用 `services_->animation_time()` 或 `services_->motion_preference()`，在 scene 变化处调用 `services_->mark_scene_structure_dirty()`。accessor 均为简单内联操作，避免镜像状态过期。相比新增可写引用 accessor，此方式不暴露窗口服务私有存储。

### 2. 不改其他兼容接口

`ButtonComponentHost` 的文本、交互、场景和动画 runtime 引用仍来自窗口服务现有 accessor；本轮仅清理越权字段访问。后续若删除整个兼容宿主，应以独立 change 评估既有测试/示例调用点。

### 3. 验证边界

在正式 `windows-msvc` Ninja Multi-Config/MSVC x64 Debug/Release 构建；用 Button animation/focus/dirty、Input/Search/Selection 混合窗口、Gallery frame 与 idle CTest 核对行为。通用逻辑只在 Windows 正式 preset 测一次，Linux 原生平台行为依用户安排。

## Risks / Trade-offs

- [遗漏读取点] → 编译失败或测试异常；搜索三个旧字段和 `friend`，正式构建两个配置。
- [scene dirty 时序偏移] → 直接调用同一服务对象的内联方法，运行 scene topology/主题回归与 Gallery frame。
- [每帧开销] → accessor 内联且无分配，运行既有 idle/animation benchmark。

## Migration Plan

提交计划后做一次源码阶段，正式构建和定向测试通过再提交；另交验收证据。无持久状态或 ABI 发布迁移，失败时可撤销源码提交。Linux 项保持待验。
