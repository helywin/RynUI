# RynUI

[简体中文](README.md) · English

**Build fine-grained reactive native desktop interfaces with modern C++20.**

RynUI is a modern C++20 desktop UI framework for Windows and Linux, designed against **Ant Design 6.6.5** for component semantics, Design Tokens, theming, and interaction states. Declare interfaces with type-safe C++ props, typed slots, and reactive `Prop<T>` bindings; state changes update only the affected layout or rendering data and render natively on the GPU.

Built for desktop tools, industrial control interfaces, robotics applications, and monitoring dashboards, RynUI runs on a native C++ runtime with no browser, React, DOM, or Virtual DOM dependency.

> **In development**: The core runtime, GPU rendering pipeline, native i18n, and 71 native components across the Ant Design 6.6.5 catalog of 73 entries (`65 implemented` + `6 partial`) are runnable in `rynui_token_gallery` and standalone demos. API details, visual differential parity, and per-platform native validation (Windows/Linux GPU, real system IME, OS accessibility) are still actively progressing.

## Key Highlights

- **Declarative C++20 API**: Public APIs live in the `ryn` namespace and compose interfaces via typed Props, typed slots, and reactive `Prop<T>` values — no macros or external DSL compilers required.
- **Fine-grained reactivity, no Virtual DOM**: `Signal`, `Memo`, `Effect`, `Binding`, and `Scope` drive a retained Node tree. Ordinary property updates trigger only the affected layout or paint data without rerunning unrelated components.
- **Ant Design 6.6.5 themes & Design Tokens**: 1,198 locked Design Tokens derive Default, Dark, Compact, brand-color, and nested themes. `LayoutStyle` controls external layout only; visual styling comes exclusively from Theme and Component Tokens.
- **Native text, bidi, and i18n**: UTF-8 `ryn::String`/`StringView`, FreeType + HarfBuzz + SheenBidi (Unicode 17 bidirectional layout, Latin/CJK & system font fallback, high-DPI rasterization), grapheme-safe editing with IME sessions, and built-in `zh-CN`/`en-US` i18n via `ConfigProvider` and `useI18n()`.
- **Native GPU rendering & portable core**: An SDL3 host handles windows and input while a separate renderer owns GPU resources (Windows D3D12/DXIL, Linux Vulkan/SPIR-V), with on-demand frame scheduling and embedded shaders. Core and `renderer/common` stay isolated from platform SDKs and support SDL-free HEADLESS/Recording builds and tests.

## Quick Taste

```cpp
#include <ryn/rynui.hpp>
#include <string>

ryn::Signal<int> count{0};
ryn::Signal<bool> loading{false};

ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::SpaceSize::Middle), [&] {
    ryn::Title(ryn::TitleProps{}
                   .level(ryn::TypographyLevel::H4)
                   .content(ryn::bind([&] {
                       return ryn::String::from_utf8("Count: " + std::to_string(count.get())).value();
                   })));

    ryn::Space(ryn::SpaceProps{}.size(ryn::SpaceSize::Small), [&] {
        ryn::Button(ryn::ButtonProps{}
                        .type(ryn::ButtonType::Primary)
                        .loading(loading)
                        .onClick([&] { count.set(count.get() + 1); }),
                    [] { ryn::Text(u8"Increase"); });

        ryn::Button(ryn::ButtonProps{}.onClick([&] { loading.set(!loading.get()); }),
                    [] { ryn::Text(u8"Toggle loading"); });
    });
});
```

See [`examples/`](examples) (including the public-header [`examples/i18n_app/`](examples/i18n_app) and [`examples/app_notice/`](examples/app_notice) modules) or open the live C++ source panel inside `rynui_token_gallery` for full examples.

## Current Status

### Core Runtime & Infrastructure

| Area | Available Capabilities |
| --- | --- |
| Reactivity & layout | `Signal`, `Memo`, `Effect`, `Binding`, `Scope`, `Prop<T>`; retained Node tree, Constraints layout, shared responsive breakpoints, on-demand frame scheduling, and foundational state motion |
| Text & desktop input | UTF-8 `ryn::String`/`StringView`, `u8"..."` literals, Unicode 17 bidi shaping/wrapping, system font fallback, high-DPI glyph rasterization; grapheme-safe selection, clipboard, undo/redo, and IME session bridge |
| Theming & i18n | 1,198 locked Design Tokens; Default, Dark, Compact, brand, and nested themes; `ConfigProvider` inherited defaults and built-in `zh-CN`/`en-US` locale packs, plurals, and exact numeric formatting |
| Rendering & portability | Separate host/GPU ownership, shared `SceneResources` upload transactions, Quad/Glyph/RoundedEffect/Image & composition paths, embedded shaders, and SDL-free HEADLESS/Recording builds |
| Gallery & automated checks | Offline Ant Design 6.6.5 reference catalog and per-component pages in `rynui_token_gallery`, live theme/locale/direction switching, headless Wayland checks, and in-progress Web/native differential testing |

### Ant Design 6.6.5 Component Coverage (73 entries across 7 categories)

The table below reflects [`gallery/ant-design/6.6.5/support-overlay.json`](gallery/ant-design/6.6.5/support-overlay.json) (**65 `implemented` · 6 `partial` · 1 `deprecated` · 1 `web-only`**):

| Category | Status Breakdown | Components & Documentation |
| --- | --- | --- |
| General (4) | 4 implemented | [Button](docs/button.md), [FloatButton](docs/float-button.md), [Icon](docs/icon.md), [Typography](docs/typography.md) |
| Layout (7) | 7 implemented | [Divider](docs/divider.md), [Flex](docs/flex-space.md), [Grid](docs/grid.md), [Layout](docs/components/layout.md), [Masonry](docs/masonry.md), [Space](docs/flex-space.md), [Splitter](docs/components/splitter.md) |
| Navigation (7) | 7 implemented | [Anchor](docs/anchor.md), [Breadcrumb](docs/breadcrumb.md), [Dropdown](docs/menu-dropdown.md), [Menu](docs/menu-dropdown.md), [Pagination](docs/pagination.md), [Steps](docs/steps.md), [Tabs](docs/tabs.md) |
| Data Entry (18) | 18 implemented | [AutoComplete](docs/auto-complete.md), [Cascader](docs/components/cascader.md), [Checkbox](docs/checkbox.md), [ColorPicker](docs/color-picker.md), [DatePicker](docs/date-picker.md), [Form](docs/form.md), [Input](docs/input.md) (incl. Search / Password / TextArea / OTP), [InputNumber](docs/input-number.md), [Mentions](docs/mentions.md), [Radio](docs/radio.md), [Rate](docs/rate.md), [Select](docs/select.md), [Slider](docs/slider.md), [Switch](docs/switch.md), [TimePicker](docs/time-picker.md), [Transfer](docs/components/transfer.md), [TreeSelect](docs/tree-select.md), [Upload](docs/components/upload.md) |
| Data Display (21) | 15 implemented · 5 partial · 1 deprecated | **implemented**: [Avatar](docs/avatar.md), [Badge](docs/tag-and-badge.md), [Calendar](docs/calendar.md), [Card](docs/card.md), [Carousel](docs/carousel.md), [Collapse](docs/collapse.md), [Descriptions](docs/descriptions.md), [Image](docs/components/image.md), [Listy](docs/listy.md), [QRCode](docs/qrcode.md), [Segmented](docs/segmented.md), [Tag](docs/tag-and-badge.md), [Timeline](docs/timeline.md), [Tooltip](docs/tooltip.md), [Tree](docs/tree.md)<br>**partial**: [Empty](docs/empty-result.md), [Popover](docs/popover.md), [Statistic](docs/statistic.md), [Table](docs/table.md), [Tour](docs/tour.md)<br>**deprecated**: [List](docs/list-migration.md) (deprecated upstream; migrate to Listy) |
| Feedback (11) | 10 implemented · 1 partial | **implemented**: [Alert](docs/feedback.md), [Drawer](docs/drawer.md), [Message](docs/message-notification.md), [Modal](docs/modal.md), [Notification](docs/message-notification.md), [Popconfirm](docs/popconfirm.md), [Result](docs/empty-result.md), [Skeleton](docs/skeleton.md), [Spin](docs/feedback.md), [Watermark](docs/components/watermark.md)<br>**partial**: [Progress](docs/feedback.md) |
| Other (5) | 4 implemented · 1 web-only | **implemented**: [Affix](docs/components/affix.md), [App](docs/app.md), [BorderBeam](docs/components/border-beam.md), [ConfigProvider](docs/config-provider.md)<br>**web-only**: [Util](docs/util.md) (TypeScript compile-time type helpers upstream; C++ uses typed Props/refs directly) |

> **Scope & validation note**: Catalog status tracks native desktop feature contracts; Web-specific DOM/CSS/React APIs are intentionally excluded. Component implementation and HEADLESS/Recording checks do not substitute for full per-platform validation across Windows D3D12, Linux Vulkan/Wayland, real system IME, OS accessibility, cross-monitor DPI, or Web/native differential suites. See the [component completion checklist](docs/component-completion.md), [open issues](docs/open-issues.md), and [OpenSpec changes](openspec/changes) for detailed status and evidence.

## Quick Start

### Platforms & Backends

| Platform | Toolchain | Current Backend Path |
| --- | --- | --- |
| Windows | MSVC x64 | SDL3 GPU / D3D12 / DXIL |
| Linux | GCC or Clang | SDL3 GPU / Vulkan / SPIR-V |
| Contract tests | Windows MSVC / Linux GCC | `HEADLESS` + `RECORDING` (no SDL or GPU required) |

macOS, mobile, and Web remain future architecture targets without official build presets or device validation yet; see the [multi-backend portability study](docs/research/multi-backend-portability.md).

### Prerequisites

- **Common**: Git, CMake 3.25+, Ninja, and a C++20 toolchain; running native examples requires a graphical desktop and a driver supporting the corresponding GPU backend.
- **Windows**: Visual Studio C++ x64 tools (MSVC is required; MinGW is not accepted for Windows builds).
- **Linux**: Fontconfig 2.13+ development files and `wayland-client >= 1.18` (for finite Gallery acceptance sync); the default `BUNDLED` Wayland build also requires Meson, pkg-config, patch, `wayland-scanner`, and development headers for Wayland, `wayland-protocols`, Cairo, and PangoCairo.

### Clone & Build

Official builds use [`CMakePresets.json`](CMakePresets.json) with `Ninja Multi-Config`. The default `BUNDLED` mode downloads and SHA256-verifies locked dependencies, so the first configuration requires network access.

```bash
git clone https://github.com/helywin/RynUI.git
cd RynUI
```

**Windows (PowerShell)**:

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe
```

Use `-Configuration Release` for interactive evaluation and performance checks.

**Linux (GCC)**:

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
./out/build/linux-gcc/examples/Debug/rynui_token_gallery
```

Linux also provides `linux-clang` presets and `linux-gcc-headless` for SDL-free contract tests (`windows-msvc-headless` on Windows). See the [build guide](docs/development/building.md) and [third-party dependency guide](docs/development/third-party.md) for Release builds, `SYSTEM` mode, and offline overrides.

## Examples

Start with `rynui_token_gallery` to explore dedicated pages for all 73 catalog entries, interactive demos, live Theme / Compact / brand / locale / direction switching, inline C++ source panels, and Design Tokens.

| Executable | What It Demonstrates |
| --- | --- |
| `rynui_token_gallery` | Start here: per-component pages, live demos, theme & i18n controls, inline C++ source, and Token browser |
| `rynui_minimal` | Minimal reactive `Signal`-to-GPU Quad rendering loop |
| `rynui_text_demo` | Latin/CJK shaping, system font fallback, and dynamic text updates |
| `rynui_button_demo` | Button variants, state transitions, and pointer/keyboard focus interaction |
| `rynui_layout_demo` | `Flex` / `Space` composition and reactive layout updates |

Example sources live in [`examples/`](examples) (including public-header examples [`examples/i18n_app/`](examples/i18n_app) and [`examples/app_notice/`](examples/app_notice)); public C++ headers live in [`include/ryn/`](include/ryn).

## Documentation

### Architecture & Core Contracts

- [Architecture baseline](docs/architecture.md): design goals, module boundaries, and long-term technical decisions (including unimplemented roadmap items).
- [Design Token reference](docs/design-tokens.md): 1,198 locked Ant Design 6.6.5 tokens and support classification.
- [Renderer contract](docs/renderer-contract.md): logical CPU scene, packed GPU ABI, shared upload transactions, and backend capabilities.
- [Native i18n](docs/i18n.md) · [ConfigProvider](docs/config-provider.md) · [App services](docs/app.md): locale packs, message catalogs, inherited defaults, and window-scoped notice/panel services.
- [Bidirectional text](docs/bidirectional-text.md): Unicode 17 paragraph analysis, mixed-script shaping, and visual caret navigation.
- [Multi-backend portability study](docs/research/multi-backend-portability.md): implemented portability foundation and future desktop, mobile, and Web roadmap.

### Development, Build & Testing

- [Build guide](docs/development/building.md): prerequisites, official presets, Release builds, and HEADLESS contract builds.
- [Third-party dependencies](docs/development/third-party.md): `BUNDLED`/`SYSTEM` modes, version and SHA256 locks, offline overrides, and licenses.
- [Embedded resources & deployment](docs/resources.md): embedded shaders, default system font chain, and acceptance font preparation.
- [Gallery component pages](docs/development/gallery-pages.md): page host, state drafts, source panel, and targeted checks.
- [Web/native differential testing](docs/development/demo-differential-testing.md): locked official Web reference and automated differential runner.
- [VS Code setup](docs/development/vscode.md) · [Code formatting](docs/development/formatting.md): IntelliSense configuration and clang-format 22.x conventions.

### Status & Collaboration

- [Component completion checklist](docs/component-completion.md) · [Open issues](docs/open-issues.md): native feature status, known issues, and platform validation boundaries.
- [OpenSpec changes](openspec/changes) · [OpenSpec tooling](docs/development/openspec.md): per-change scope, task checklists, and validation summaries.
- [Agent collaboration rules](AGENTS.md): repository development, verification, and commit conventions for coding agents.

## Feedback & Contributing

Use [GitHub Issues](https://github.com/helywin/RynUI/issues) to report bugs, discuss desktop use cases, or request components. For runtime issues, include your operating system, build preset, window system, GPU/driver details, and reproduction steps. Detailed technical documentation is currently primarily in Simplified Chinese.
