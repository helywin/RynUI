## Why

RynUI 目前只有架构结论和项目约定，还没有可以构建、运行或验证的工程基线。首个 change 需要把最关键的技术假设收敛为一个可测试的最小闭环，避免在响应模型、生命周期和 GPU 数据边界尚未证明前扩展组件库。

## What Changes

- 建立 C++20、CMake、测试和 benchmark 工程骨架，以 `CMakePresets.json` 和 `Ninja Multi-Config` 管理 Windows/MSVC、Linux/GCC 与 Linux/Clang 构建，公开 API 使用 `ryn` 命名空间。
- 建立 package-manager-neutral 的第三方依赖 resolver，以显式 `BUNDLED` 或 `SYSTEM` 模式接入锁定版本的 SDL3，并把 `SDL_shadercross` 限定为离线 shader 构建所需的 host tool。
- 接入 SDL3 Window、事件循环与 SDL3 GPU 的最小 clear/present 生命周期。
- 实现 `Signal`、`Memo`、`Effect`、`Binding`、`Scope`、`batch()` 和单 UI 线程 `Scheduler` 的最小语义。
- 建立持久化 `Node`、Constraints 布局、`DirtyFlags` 和 `QuadPrimitive` 数据边界。
- 建立不与后续 Ant Design-native typed Props、typed slots、reactive `Prop<T>` 和 `LayoutStyle` 冲突的公开扩展边界，不引入通用组件视觉 `Modifier`。
- 打通 `Signal -> Binding -> DirtyFlags -> Node -> QuadPrimitive -> SDL_GPU` 局部更新链路。
- 提供真实窗口示例、单元测试和可观测计数，验证组件不被无关重跑、纯材质更新不触发布局、闲置窗口不持续满帧提交。
- 非目标：完整 Text/TextInput、Ant Design 基础组件层、复杂 Path、多窗口、Accessibility、Skia 插件和发布包装均不在本 change 范围内；本 change 只保留不与后续 Ant Design 公开 API 冲突的底层边界。

## Capabilities

### New Capabilities

- `application-runtime`: RynUI 工程、窗口、GPU device、事件循环和可验证运行生命周期。
- `reactive-runtime`: 细粒度状态、依赖收集、批处理、Scope 清理和确定性调度语义。
- `reactive-gpu-update-loop`: 持久化 Node、最小布局、Dirty 传播、Quad Primitive 与 GPU 局部更新闭环。

### Modified Capabilities

无。

## Impact

- 新增 C++20/CMake 工程、公开头文件、内部模块、测试、benchmark 和示例目录。
- 引入 SDL3 作为平台与 GPU 依赖；依赖版本、归档 SHA256 与 license 进入仓库锁定记录，vcpkg、Conan 和发行版 package 通过 `SYSTEM` 模式接入而不成为 RynUI 的强制工具链。
- 首批公开 API 进入 `ryn` 命名空间，后续 change 必须兼容或显式说明破坏性变更。
- 后续基础组件、公开布局、Design Token、主题和交互状态以 Ant Design 6 为设计基线，公开层采用 typed Props、typed slots 和 reactive `Prop<T>`；本 change 不引入 React、CSS-in-JS 或通用组件视觉 `Modifier`。
- Windows 和 Linux 是本 change 的必验平台；macOS 保留架构兼容性，但不作为本 change 的完成门槛。
- Windows 构建和验收必须使用 MSVC；MinGW 不属于受支持的 Windows 工具链，也不能替代 Windows 验收。
- 主要风险是响应依赖生命周期、Scheduler 重入、GPU device/窗口资源清理，以及 Dirty 范围被意外扩大。
