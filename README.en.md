# RynUI

[简体中文](README.md) · English

**Build reactive desktop interfaces with modern C++.**

RynUI is a C++20 desktop UI framework for Windows and Linux. It uses Ant Design 6 as the baseline for component behavior, design tokens, themes, and interaction states. Declare interfaces with typed C++ properties and content slots, connect them to reactive state, and render them on the GPU.

The project targets desktop tools, industrial control interfaces, robotics applications, and monitoring dashboards. Its UI runtime is native C++, with no browser, React, DOM, or Virtual DOM dependency.

> In development: the core runtime, initial components, and examples are runnable. RynUI is suitable for exploring the source, evaluating prototypes, and contributing. Component coverage and platform validation are still in progress; this is not yet a complete general-purpose component library.

## Key ideas

- **Declarative C++ API:** public APIs live in the `ryn` namespace and compose components through typed Props, content slots, and reactive `Prop<T>` values.
- **Fine-grained reactivity:** mounted components retain their nodes and dependencies. Ordinary property updates affect the relevant layout or rendering data without rerunning unrelated components.
- **Consistent theming:** Theme and Component Tokens control colors, typography, corners, shadows, and interaction states. `LayoutStyle` controls external layout.
- **Native GPU rendering:** SDL3 provides window, input, and GPU integration. A dedicated renderer draws text, rectangles, rounded shapes, and shadows, with frames scheduled on demand.

## Current status

| Area | Available implementation |
| --- | --- |
| Reactive state | `Signal`, `Memo`, `Effect`, `Binding`, `Scope`, and `Prop<T>` |
| Components and layout | `Text`, `Button`, single-line `Input`, `Flex`, and `Space`; wrapping, alignment, gaps, and flexible sizing |
| Themes and interaction | Default, dark, compact, brand, and nested themes; pointer interaction, keyboard focus, disabled/loading states, and basic state animations |
| Text | UTF-8 `ryn::String`/`StringView`, C++20 `u8"..."` literals, Latin/CJK shaping, font fallback, and high-DPI glyph rendering |
| Single-line editing | Controlled/uncontrolled values, placeholders, prefix/suffix slots, grapheme-safe selection, clipboard, undo/redo, IME composition events, and caret blinking |
| Examples and reference | Interactive component examples, an offline component catalog, and a Token Gallery based on Ant Design 6.5.0 |

The current components cover a subset of Ant Design. The Gallery's 72 entries across seven categories form a **reference catalog**, with support recorded for each entry; they are not 72 implemented components. `Table`, `Tree`, multiline text, password inputs, and search inputs are not available yet.

Single-line `Input` is integrated into the Gallery and has passed platform-independent tests and Windows Debug/Release build validation. Real system IME behavior, candidate window placement, and visual results still require separate Windows and Linux validation. Animation and Gallery platform checks also remain open. See the [Input](openspec/changes/010-20260831-build-text-input-foundation/tasks.md), [animation](openspec/changes/009-20260829-build-animation-runtime-foundation/tasks.md), and [Gallery](openspec/changes/008-20260829-build-ant-design-reference-gallery/tasks.md) checklists for the detailed status.

## Build and try it

Start with `rynui_token_gallery` to explore design tokens, component support, and interactive Button / Input samples.

| Platform | Toolchain | Current GPU path |
| --- | --- | --- |
| Windows | MSVC x64 | SDL3 GPU / D3D12 / DXIL |
| Linux | GCC or Clang | SDL3 GPU / Vulkan / SPIR-V |

macOS is a future architecture target; it does not yet have an official build preset or validation results.

You need Git, CMake 3.25+, Ninja, and a C++20 toolchain. Running the examples requires a graphical desktop and a driver supporting the corresponding GPU backend. On Windows, install the Visual Studio C++ x64 tools. On Linux, install Fontconfig 2.13+ development files; the default bundled Wayland build also requires Meson, pkg-config, patch, wayland-scanner, and development dependencies for Wayland, wayland-protocols, Cairo, and PangoCairo.

```bash
git clone https://github.com/helywin/RynUI.git
cd RynUI
```

Official builds use the repository's `CMakePresets.json` with `Ninja Multi-Config`. The default `BUNDLED` mode downloads and verifies locked dependencies, so the first configuration requires network access.

Windows PowerShell:

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe
```

Linux / GCC:

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
./out/build/linux-gcc/examples/Debug/rynui_token_gallery
```

Linux also has `linux-clang` presets. See the [build guide](docs/development/building.md) for Release builds and further configuration, and the [dependency guide](docs/development/third-party.md) for `SYSTEM` mode, version locks, and offline preparation.

## Examples

| Executable | What it demonstrates |
| --- | --- |
| `rynui_token_gallery` | Start here: themes, component support, catalog navigation, and Button / Input interaction |
| `rynui_minimal` | The smallest reactive-state-to-GPU rendering loop |
| `rynui_text_demo` | Latin/CJK shaping, font fallback, and text updates |
| `rynui_button_demo` | Button states and pointer/keyboard interaction |
| `rynui_layout_demo` | Flex / Space composition and reactive layout |

Example sources live in [`examples/`](examples); public headers live in [`include/ryn/`](include/ryn).

## Documentation and participation

- [Build guide](docs/development/building.md): environment, presets, and build options.
- [Architecture](docs/architecture.md): design goals, module boundaries, and long-term decisions, including plans that are not implemented yet.
- [Design Token reference](docs/design-tokens.md): locked design tokens and their support status.
- [Third-party dependencies](docs/development/third-party.md): versions, sources, verification, and dependency licenses.
- [OpenSpec changes](openspec/changes): change scope, tasks, and validation evidence.
- [Agent collaboration rules](AGENTS.md): repository development and verification conventions.

Use [GitHub Issues](https://github.com/helywin/RynUI/issues) to report problems, discuss use cases, or request components. For runtime problems, include your operating system, build preset, window system, GPU/driver, and reproduction steps. Detailed technical documentation is currently primarily in Simplified Chinese.
