# RynUI

RynUI 是一个面向桌面应用的现代 C++ 响应式 UI 框架方案。项目目标是把 Ant Design-native typed component API、C++ declarative slot DSL、SolidJS 式细粒度响应、持久化 UI 树与专用 GPU 渲染链路组合起来，让状态变化尽可能直接更新受影响的 UI 属性和 GPU 数据。

> 当前状态：项目已进入首个 OpenSpec change 的工程基线实现阶段。

## 核心方向

- 使用 C++20 与 `ryn` 命名空间。
- 不使用 Virtual DOM；组件默认只在挂载时执行一次。
- 使用 `Signal`、`Memo`、`Effect`、`Binding` 和 `Scope` 构建细粒度响应图。
- 使用 Retained UI Tree、Constraints 布局与分阶段 Dirty 传播。
- 基础 UI 组件、公开布局语义、Design Token、主题和交互状态统一参照 Ant Design 6；底层仍为原生 C++ 实现，不依赖 React 或 CSS-in-JS。
- 公开组件使用 typed Props、typed slots 与 reactive `Prop<T>`；通用 `Modifier` 不作为组件视觉样式入口。
- Compose 仅作为 slot composition、Constraints 与 phased invalidation 的机制参考，不定义 RynUI 的公开组件语言。
- 普通 UI 走自研轻量 GPU Renderer，优先映射为 `Quad`、`Glyph`、`Image`、`Clip` 等 Primitive。
- SDL3 负责窗口、输入、IME、剪贴板和跨平台 GPU 接入。
- FreeType 与 HarfBuzz 负责字体栅格化和文本整形。
- Skia 不作为核心依赖，只保留未来复杂 Canvas/Path 场景的可选插件位置。

## 文档

- [最终架构与实现路线](docs/architecture.md)
- [开发构建说明](docs/development/building.md)
- [首个实现 change](openspec/changes/001-20260825-establish-rynui-foundation)
- [Agent 协作规则](AGENTS.md)

## 当前开发阶段

首个实现 change 只验证一条最小技术闭环：

```text
Signal
  -> Binding
  -> DirtyFlags
  -> Node
  -> QuadPrimitive
  -> SDL_GPU
```

在这条链路通过真实窗口、自动测试和性能观测验证之前，不扩展完整组件库、复杂 Path、无障碍平台适配或多窗口能力。
