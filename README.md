# RynUI

RynUI 是一个面向桌面应用的现代 C++20 响应式 UI 框架。它以 Ant Design 6 的组件语义、Design Token、主题和交互状态作为设计基线，结合 typed component API、声明式 slot composition、细粒度响应、Retained UI Tree、Constraints 布局与专用 GPU 渲染链路。

项目使用 `ryn` 命名空间，不引入 React、DOM、CSS-in-JS 或 Virtual DOM。组件在挂载时建立稳定 identity，普通属性更新只推进受影响的响应、布局、场景或 GPU 数据。

> 当前状态：核心基础设施与首批公开组件已经可运行，仍处于持续开发阶段，尚不是完整的通用组件库。

## 已实现能力

- `Signal`、`Memo`、`Effect`、`Binding`、`Scope` 与 reactive `Prop<T>`。
- UTF-8 `ryn::String`/`StringView`，支持直接使用 C++20 `u8"..."` 字面量。
- typed `Text`、`Button`、`Flex` 与 `Space` 组件，以及 typed content slots。
- `LayoutStyle` 外部布局约束、Flex wrap/justify/align/gap、grow/shrink/basis/order。
- Ant Design 6 风格的 Design Token、Default/Dark/Compact/Brand/Nested Theme 与组件状态 token。
- Pointer routing、hover/active、keyboard focus、focus-visible、disabled/loading 和 Button activation。
- Retained scene、Quad/Glyph/RoundedEffect、阴影、圆角与离线 DXIL/SPIR-V shader。
- Windows/MSVC/D3D12 和 Linux/GCC/Clang/Vulkan 构建路径。
- high-DPI viewport、输入坐标映射、动态 display scale 与平台默认 UI 字体链。

## 设计边界

- 基础 UI 组件和视觉合同参照 Ant Design 6，但底层是原生 C++ 实现。
- Compose 只作为 typed slots、Constraints 和 phased invalidation 的机制参考；公开 API 不提供通用 `Modifier`。
- `LayoutStyle` 只控制组件的外部布局；稳定组件的颜色、字体、圆角、阴影和交互状态由 Theme 与 Component Token 控制。
- SDL3 类型不会泄漏到公开组件、Reactive 或 Layout API。
- Skia 不属于核心依赖，仅为未来复杂 Canvas/Path 场景保留可选扩展位置。

## 构建与运行

正式构建统一使用仓库内的 `CMakePresets.json` 和 `Ninja Multi-Config`。默认 `BUNDLED` 模式会下载并校验锁定依赖；Windows 必须使用 MSVC。

Windows PowerShell：

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe
```

Linux / GCC：

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
./out/build/linux-gcc/examples/Debug/rynui_token_gallery
```

Linux 也提供 `linux-clang` presets。完整环境要求、Release 构建、`SYSTEM` 依赖模式和离线构建方式见[开发构建说明](docs/development/building.md)。

## 字体与平台行为

Windows 使用 DirectWrite 发现系统 UI 字体，Linux 使用 Fontconfig 读取桌面默认字体；当前两端都由 HarfBuzz 保持 logical shaping，并由 FreeType 生成透明 grayscale glyph atlas。平台集成层保留内部 typed font request，可指定其他字体文件和 face index，并继续使用系统及锁定字体 fallback。

Windows DirectWrite grayscale glyph raster path 已完成方案评估，但当前暂不切换；现有 FreeType raster path 继续作为正式实现，后续只有在真实窗口的小字号中英文对比证明有明确收益时再推进。

## 示例

仓库包含以下可运行示例：

- `rynui_minimal`：最小响应式 GPU 闭环。
- `rynui_text_demo`：Latin/CJK shaping、fallback 与文本更新。
- `rynui_button_demo`：Button 状态、焦点与输入闭环。
- `rynui_layout_demo`：公开 Flex/Space DSL 与响应式布局。
- `rynui_token_gallery`：Ant Design Token、主题、阴影、圆角、焦点和多缩放展示。

## 文档

- [架构基线](docs/architecture.md)
- [开发构建说明](docs/development/building.md)
- [第三方依赖与锁定规则](docs/development/third-party.md)
- [OpenSpec changes](openspec/changes)
- [Agent 协作规则](AGENTS.md)
