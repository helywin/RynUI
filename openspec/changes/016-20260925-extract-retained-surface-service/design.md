# Design

## Context

`ButtonSceneService` 已实现 span 形式的 `create_surface`/`update_surface`，包含按 component/root/fragment 验证的 slot+generation ID、retained quad range、RoundedEffect 子实例、scene fragment 绑定及失效计数。Button 的 `create`/`update` 只是固定视觉数组的薄入口；Input 与 Selection 已使用泛型 span 入口。

## Goals / Non-Goals

**Goals:** 让新控件只依赖通用 retained surface 类型和 `WindowComponentServices::surfaces()`；保留固定 topology、stale ID 拒绝、range compaction 与最小 Material/Geometry 更新；不增加每帧 allocation。

**Non-Goals:** 不统一 Button/Input 的 focus 视觉，不移动 Token 解析，不重写 GPU renderer、RoundedEffect、scene composer 或动画，不开放 public surface API。

## Decisions

### 1. 通用 service 与专属 Button visuals 分离

通用 `retained_surface_service.hpp/.cpp` 定义 `RetainedSurfaceId`、`RetainedSurfaceEffects`、`RetainedSurfaceDiagnostics` 和 `RetainedSurfaceService`；它只接收 `span<QuadInstance>` 与 typed effect 数据，不定义 Button layer count。`button_scene_service.hpp` 只保存 Button 的 visual layer 枚举/数组及迁移期内部别名，避免本轮把大量已有内部测试一次性强制改写。

### 2. 单一窗口 owner、对等 consumer

`WindowComponentServices` 只持有一个 `RetainedSurfaceService`。Button 可继续通过自身便利 accessor 取得同一对象；Input/Selection/Gallery 使用 `surfaces()` 与通用 ID/effects。Button 的 typed array 通过 span 进入通用 create/update，surface service 不知道 Button spinner 的八段含义。

### 3. 保持兼容与测试证据

先搬迁核心实现，再转换 consumer 类型和调用点，最后收窄 Button 专属头。已有 `button_scene_service_tests` 中对通用 `create_surface`、stale generation、effect/order/range、dirty domains 的测试作为核心回归；新增或改造测试核对多个 consumer 共享同一 service、Button adapter 不拥有第二套 store。泛型 surface 的颜色更新不得触发测量或重建稳定 scene topology。

## Validation

正式 Windows MSVC Debug/Release preset 构建与受影响 CTest；一次平台通用完整 CTest，Search/Input/Selection/Animation 实窗回归，OpenSpec strict validate 与 `git diff --check`。Linux 原生 Wayland/GPU 检查依用户安排单列待验，不重复通用合同。
