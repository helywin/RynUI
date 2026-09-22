# RynUI

简体中文 · [English](README.en.md)

**用现代 C++ 构建响应式桌面界面。**

RynUI 是一个面向 Windows 和 Linux 的 C++20 桌面 UI 框架，以 Ant Design 6 的组件语义、设计变量（Design Token）、主题和交互状态为设计基线。你可以用类型明确的 C++ 属性和嵌套内容声明界面，让状态变化自动更新相关组件，再通过 GPU 绘制窗口内容。

项目面向桌面工具、工业控制、机器人应用和监控面板等场景。界面由原生 C++ 运行时实现，不依赖浏览器、React、DOM 或 Virtual DOM。

> 开发中：核心运行时、首批组件和示例已经可运行，适合源码学习、原型验证与参与开发。组件覆盖和平台验收仍在推进，尚不是完整的通用组件库。

## 核心特点

- **声明式 C++ API**：公开 API 使用 `ryn` 命名空间，通过 typed Props、内容插槽和 reactive `Prop<T>` 组合组件。
- **细粒度响应**：组件挂载后保留节点与依赖关系，普通属性更新只推进受影响的布局或绘制数据，不重新执行无关组件。
- **统一主题**：颜色、字体、圆角、阴影和交互状态由 Theme 与 Component Token 控制；`LayoutStyle` 负责外部布局。
- **原生 GPU 绘制**：SDL3 负责窗口、输入和 GPU 接入，专用渲染链路绘制文字、矩形、圆角和阴影，并按需调度帧。

## 当前进展

| 领域 | 已有实现 |
| --- | --- |
| 响应式状态 | `Signal`、`Memo`、`Effect`、`Binding`、`Scope` 与 `Prop<T>` |
| 组件与布局 | `Text`、`Button`、单行 `Input`、`Flex`、`Space`；布局支持换行、对齐、间距和伸缩 |
| 主题与交互 | 默认、暗色、紧凑、品牌色和嵌套主题；鼠标交互、键盘焦点、禁用/加载状态与基础状态动画 |
| 文本 | UTF-8 `ryn::String`/`StringView`、C++20 `u8"..."` 字面量、中英文排版、字体回退与高 DPI 字形绘制 |
| 单行编辑 | 受控/非受控值、占位文本、前后缀、Unicode 字素安全选区、剪贴板、撤销/重做、输入法组合事件与光标闪烁 |
| 示例与参考 | 可交互组件示例，以及基于 Ant Design 6.6.5 的离线组件目录和 Token Gallery |

当前组件仅覆盖 Ant Design 的部分能力。Gallery 的七类 73 项是**参考目录**，每项单独标注支持范围，并不表示已实现 73 个组件。`List` 已按上游标记 deprecated，`Listy` 是新加入的参考项；`Table`、`Tree`、多行文本、密码输入和搜索输入等仍未提供。

单行 `Input` 已接入 Gallery，并完成平台通用测试和 Windows Debug/Release 构建验收；真实系统输入法、候选窗与视觉效果仍需分别完成 Windows 和 Linux 验收。动画和 Gallery 也有待完成的平台验收，具体进度见 [Input](openspec/changes/010-20260831-build-text-input-foundation/tasks.md)、[动画](openspec/changes/009-20260829-build-animation-runtime-foundation/tasks.md)和 [Gallery](openspec/changes/008-20260829-build-ant-design-reference-gallery/tasks.md) 清单。

## 构建与体验

建议先运行 `rynui_token_gallery`，浏览设计变量、组件支持状态以及 Button / Input 交互样例。

| 平台 | 工具链 | 当前 GPU 路径 |
| --- | --- | --- |
| Windows | MSVC x64 | SDL3 GPU / D3D12 / DXIL |
| Linux | GCC 或 Clang | SDL3 GPU / Vulkan / SPIR-V |

macOS 属于后续架构目标，当前尚未提供正式构建 preset 或验收结果。

需要 Git、CMake 3.25+、Ninja 和 C++20 工具链；运行示例需要图形桌面和支持相应 GPU 后端的驱动。Windows 需要安装 Visual Studio C++ x64 工具。Linux 需要 Fontconfig 2.13+ 开发包；默认 bundled Wayland 构建还需要 Meson、pkg-config、patch、wayland-scanner，以及 Wayland、wayland-protocols、Cairo、PangoCairo 开发依赖。

```bash
git clone https://github.com/helywin/RynUI.git
cd RynUI
```

所有正式构建使用仓库内的 `CMakePresets.json` 和 `Ninja Multi-Config`。默认 `BUNDLED` 模式会下载并校验锁定依赖，首次配置需要网络。

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

Linux 也提供 `linux-clang` presets。Release 构建和更多环境配置见[开发构建说明](docs/development/building.md)；`SYSTEM` 依赖模式、版本锁定与离线准备方式见[第三方依赖说明](docs/development/third-party.md)。

## 示例

| 可执行文件 | 适合了解什么 |
| --- | --- |
| `rynui_token_gallery` | 从这里开始：主题、组件支持范围、目录导航与 Button / Input 交互 |
| `rynui_minimal` | 响应式状态到 GPU 绘制的最小闭环 |
| `rynui_text_demo` | 中英文排版、字体回退与文本更新 |
| `rynui_button_demo` | 按钮状态、鼠标与键盘交互 |
| `rynui_layout_demo` | Flex / Space 组合与响应式布局 |

示例源码位于 [`examples/`](examples)，公开头文件位于 [`include/ryn/`](include/ryn)。

## 文档与参与

- [开发构建说明](docs/development/building.md)：环境、presets 与构建选项。
- [架构基线](docs/architecture.md)：设计目标、模块边界和长期技术决策；包含尚未实现的规划。
- [Design Token 参考](docs/design-tokens.md)：锁定的设计变量及支持范围。
- [第三方依赖与锁定规则](docs/development/third-party.md)：版本、来源、校验与许可证信息。
- [OpenSpec changes](openspec/changes)：各项变更的范围、任务和验收证据。
- [Agent 协作规则](AGENTS.md)：仓库开发与验证约定。

欢迎通过 [GitHub Issues](https://github.com/helywin/RynUI/issues) 反馈问题、讨论使用场景或提出组件需求。报告运行问题时，请附上操作系统、构建 preset、窗口系统、GPU/驱动及复现步骤。详细技术文档目前以简体中文为主。
