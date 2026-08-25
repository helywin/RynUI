# RynUI

RynUI 是一个面向桌面应用的现代 C++ 响应式 UI 框架方案。项目目标是把 Compose 风格的声明式 API、SolidJS 式细粒度响应、持久化 UI 树与专用 GPU 渲染链路组合起来，让状态变化尽可能直接更新受影响的 UI 属性和 GPU 数据。

> 当前状态：项目已完成 Git、OpenSpec 和正式架构文档初始化，尚未进入代码实现阶段。

## 核心方向

- 使用 C++20 与 `ryn` 命名空间。
- 不使用 Virtual DOM；组件默认只在挂载时执行一次。
- 使用 `Signal`、`Memo`、`Effect`、`Binding` 和 `Scope` 构建细粒度响应图。
- 使用 Retained UI Tree、Constraints 布局与分阶段 Dirty 传播。
- 普通 UI 走自研轻量 GPU Renderer，优先映射为 `Quad`、`Glyph`、`Image`、`Clip` 等 Primitive。
- SDL3 负责窗口、输入、IME、剪贴板和跨平台 GPU 接入。
- FreeType 与 HarfBuzz 负责字体栅格化和文本整形。
- Skia 不作为核心依赖，只保留未来复杂 Canvas/Path 场景的可选插件位置。

## 文档

- [最终架构与实现路线](docs/architecture.md)
- [OpenSpec 项目配置](openspec/config.yaml)

## OpenSpec 约定

- change 名称：`NNN-YYYYMMDD-lowercase-kebab-case`
- 示例：`001-20260908-my-first-change`
- `NNN` 是三位递增序号，`YYYYMMDD` 是创建日期。
- OpenSpec 说明性正文使用简体中文。
- `ADDED`、`MODIFIED`、`REMOVED`、`RENAMED`、`Requirement`、`Scenario`、`WHEN`、`THEN`、`SHALL`、`MUST` 等结构关键字保持英文。

## 规划边界

首个实现阶段只验证一条最小闭环：

```text
Signal
  -> Binding
  -> DirtyFlags
  -> Node
  -> QuadPrimitive
  -> SDL_GPU
```

在这条链路通过真实窗口、自动测试和性能观测验证之前，不扩展完整组件库、复杂 Path、无障碍平台适配或多窗口能力。
