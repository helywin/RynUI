# RynUI

简体中文 · [English](README.en.md)

**用现代 C++20 构建细粒度响应式的原生桌面界面。**

RynUI 是一个面向 Windows 和 Linux 的现代 C++20 桌面 UI 框架，以 **Ant Design 6.6.5** 的组件语义、设计变量（Design Token）、主题体系与交互状态为设计基线。你可以用类型安全的 C++ Props、内容插槽和响应式 `Prop<T>` 声明界面，状态更新只推进受影响的布局或绘制数据，再由原生 GPU 渲染到窗口。

项目面向桌面工具、工业控制、机器人应用和监控面板等原生场景，完全由原生 C++ 运行时驱动，不依赖浏览器、React、DOM 或 Virtual DOM。

> **开发中**：核心运行时、GPU 渲染链路、原生国际化以及 Ant Design 6.6.5 七类 73 项参考目录中的 71 个原生组件（65 `implemented` + 6 `partial`）已实现并可在 `rynui_token_gallery` 中交互体验。API 细节、视觉差分与分平台实机验收（Windows / Linux GPU、系统 IME、无障碍等）仍在持续完善中。

## 核心特点

- **声明式 C++20 API**：公开接口位于 `ryn` 命名空间，采用 typed Props、typed slots 与 reactive `Prop<T>` 组合界面，不依赖宏或额外 DSL 编译器。
- **细粒度响应，无 Virtual DOM**：`Signal`、`Memo`、`Effect`、`Binding` 与 `Scope` 直接驱动持久化 Node 树；普通属性变更只触发局部布局或绘制更新，不重新执行无关组件。
- **Ant Design 6.6.5 主题与 Token 体系**：基于 1198 项锁定 Design Token 派生 Default、Dark、Compact、品牌色与嵌套主题；`LayoutStyle` 仅负责外部布局，视觉样式统一归属 Theme 与 Component Token。
- **原生文本、双向排版与国际化**：内置 UTF-8 `ryn::String`/`StringView`、FreeType + HarfBuzz + SheenBidi（Unicode 17 双向文本、中英文与系统字体回退、高 DPI 栅格化）、字素级编辑与 IME 会话，以及基于 `ConfigProvider` 和 `useI18n()` 的 `zh-CN`/`en-US` 原生国际化。
- **原生 GPU 绘制与可移植核心**：SDL3 宿主管窗口与输入，独立 renderer 管 GPU（Windows D3D12/DXIL、Linux Vulkan/SPIR-V），支持按需帧调度与内嵌 shader；Core 与 `renderer/common` 隔离平台 SDK，支持无 SDL 的 HEADLESS/Recording 合同构建与测试。

## 代码一瞥

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
                    [] { ryn::Text(u8"增加 / Increase"); });

        ryn::Button(ryn::ButtonProps{}.onClick([&] { loading.set(!loading.get()); }),
                    [] { ryn::Text(u8"切换加载状态"); });
    });
});
```

更多完整用法可直接参考 [`examples/`](examples)（含国际化示例 [`examples/i18n_app/`](examples/i18n_app) 与全局通知示例 [`examples/app_notice/`](examples/app_notice)），或在 `rynui_token_gallery` 中展开各组件的实时 C++ 源码面板。

## 当前进展

### 核心运行时与基础设施

| 领域 | 当前能力 |
| --- | --- |
| 响应式与布局 | `Signal`、`Memo`、`Effect`、`Binding`、`Scope`、`Prop<T>`；持久化 Node 树、Constraints 布局、共享响应式断点、按需帧调度与基础状态动效 |
| 文本与桌面输入 | UTF-8 `ryn::String`/`StringView`、`u8"..."` 字面量、Unicode 17 双向排版、系统字体回退、高 DPI 字形栅格化；字素安全选区、剪贴板、撤销/重做与 IME 会话桥接 |
| 主题与国际化 | 1198 项锁定 Design Token、Default/Dark/Compact/品牌色/嵌套主题；`ConfigProvider` 默认属性继承与内置 `zh-CN`/`en-US` 语言包、复数与精确数值格式 |
| 渲染与可移植基础 | 独立宿主/GPU 所有权、共同 `SceneResources` 上传事务、Quad/Glyph/RoundedEffect/Image 与合成路径、内嵌 shader、无 SDL 的 HEADLESS/Recording 构建 |
| Gallery 与测试体系 | 离线 Ant Design 6.6.5 组件目录与独立页面 `rynui_token_gallery`、在线主题/语言/方向切换、后台 Wayland 验收以及推进中的 Web/native 自动差分测试 |

### Ant Design 6.6.5 组件覆盖（七类 73 项）

下表对应离线参考目录 [`gallery/ant-design/6.6.5/support-overlay.json`](gallery/ant-design/6.6.5/support-overlay.json) 的真实状态（共 **65 `implemented` · 6 `partial` · 1 `deprecated` · 1 `web-only`**）：

| 分类 | 状态统计 | 组件与文档入口 |
| --- | --- | --- |
| 通用 General (4) | 4 implemented | [Button](docs/button.md)、[FloatButton](docs/float-button.md)、[Icon](docs/icon.md)、[Typography](docs/typography.md) |
| 布局 Layout (7) | 7 implemented | [Divider](docs/divider.md)、[Flex](docs/flex-space.md)、[Grid](docs/grid.md)、[Layout](docs/components/layout.md)、[Masonry](docs/masonry.md)、[Space](docs/flex-space.md)、[Splitter](docs/components/splitter.md) |
| 导航 Navigation (7) | 7 implemented | [Anchor](docs/anchor.md)、[Breadcrumb](docs/breadcrumb.md)、[Dropdown](docs/menu-dropdown.md)、[Menu](docs/menu-dropdown.md)、[Pagination](docs/pagination.md)、[Steps](docs/steps.md)、[Tabs](docs/tabs.md) |
| 数据录入 Data Entry (18) | 18 implemented | [AutoComplete](docs/auto-complete.md)、[Cascader](docs/components/cascader.md)、[Checkbox](docs/checkbox.md)、[ColorPicker](docs/color-picker.md)、[DatePicker](docs/date-picker.md)、[Form](docs/form.md)、[Input](docs/input.md)（含 Search / Password / TextArea / OTP）、[InputNumber](docs/input-number.md)、[Mentions](docs/mentions.md)、[Radio](docs/radio.md)、[Rate](docs/rate.md)、[Select](docs/select.md)、[Slider](docs/slider.md)、[Switch](docs/switch.md)、[TimePicker](docs/time-picker.md)、[Transfer](docs/components/transfer.md)、[TreeSelect](docs/tree-select.md)、[Upload](docs/components/upload.md) |
| 数据展示 Data Display (21) | 15 implemented · 5 partial · 1 deprecated | **implemented**：[Avatar](docs/avatar.md)、[Badge](docs/tag-and-badge.md)、[Calendar](docs/calendar.md)、[Card](docs/card.md)、[Carousel](docs/carousel.md)、[Collapse](docs/collapse.md)、[Descriptions](docs/descriptions.md)、[Image](docs/components/image.md)、[Listy](docs/listy.md)、[QRCode](docs/qrcode.md)、[Segmented](docs/segmented.md)、[Tag](docs/tag-and-badge.md)、[Timeline](docs/timeline.md)、[Tooltip](docs/tooltip.md)、[Tree](docs/tree.md)<br>**partial**：[Empty](docs/empty-result.md)、[Popover](docs/popover.md)、[Statistic](docs/statistic.md)、[Table](docs/table.md)、[Tour](docs/tour.md)<br>**deprecated**：[List](docs/list-migration.md)（上游已废弃，迁移至 Listy） |
| 反馈 Feedback (11) | 10 implemented · 1 partial | **implemented**：[Alert](docs/feedback.md)、[Drawer](docs/drawer.md)、[Message](docs/message-notification.md)、[Modal](docs/modal.md)、[Notification](docs/message-notification.md)、[Popconfirm](docs/popconfirm.md)、[Result](docs/empty-result.md)、[Skeleton](docs/skeleton.md)、[Spin](docs/feedback.md)、[Watermark](docs/components/watermark.md)<br>**partial**：[Progress](docs/feedback.md) |
| 其他 Other (5) | 4 implemented · 1 web-only | **implemented**：[Affix](docs/components/affix.md)、[App](docs/app.md)、[BorderBeam](docs/components/border-beam.md)、[ConfigProvider](docs/config-provider.md)<br>**web-only**：[Util](docs/util.md)（上游为 TypeScript 类型工具，C++ 直接使用 typed Props/ref） |

> **范围与验证说明**：目录状态表达的是各组件按原生桌面合同实现的覆盖情况，不移植 Web 专用的 DOM/CSS/React API。组件已实现或通过 HEADLESS/Recording 合同测试，不代表 Windows 与 Linux 全部真机 GPU、系统输入法（IME）、OS 辅助技术、跨显示器 DPI 及 Web/native 差分验收均已关闭；详细缺口、平台证据与已知问题见[组件收尾清单](docs/component-completion.md)、[未解决问题总表](docs/open-issues.md)和 [OpenSpec changes](openspec/changes)。

## 快速开始

### 支持平台与后端

| 平台 | 工具链 | 当前后端路径 |
| --- | --- | --- |
| Windows | MSVC x64 | SDL3 GPU / D3D12 / DXIL |
| Linux | GCC 或 Clang | SDL3 GPU / Vulkan / SPIR-V |
| 合同测试 | Windows MSVC / Linux GCC | `HEADLESS` + `RECORDING`（无 SDL / GPU 依赖） |

macOS、移动端与 Web 属于后续架构目标，尚未提供正式构建 preset 或实机验收结果；跨端边界见[多 backend 与跨端调研](docs/research/multi-backend-portability.md)。

### 环境要求

- **通用**：Git、CMake 3.25+、Ninja、C++20 工具链；运行原生示例需要图形桌面与支持对应 GPU 后端的驱动。
- **Windows**：Visual Studio C++ x64 工具链（MSVC，不支持用 MinGW 替代）。
- **Linux**：Fontconfig 2.13+ 开发包、`wayland-client >= 1.18` 开发包（Gallery 有限验收同步）；默认 `BUNDLED` Wayland 构建还需要 Meson、pkg-config、patch、`wayland-scanner`，以及 Wayland、`wayland-protocols`、Cairo、PangoCairo 开发依赖。

### 克隆与构建

所有正式构建统一由仓库的 [`CMakePresets.json`](CMakePresets.json) 与 `Ninja Multi-Config` 驱动。默认 `BUNDLED` 模式会下载并校验 SHA256 锁定的第三方依赖，首次配置需要网络。

```bash
git clone https://github.com/helywin/RynUI.git
cd RynUI
```

**Windows (PowerShell)**：

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe
```

交互体验或性能测量建议使用 `-Configuration Release`。

**Linux (GCC)**：

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
./out/build/linux-gcc/examples/Debug/rynui_token_gallery
```

Linux 同时提供 `linux-clang` 与无窗口合同测试 preset `linux-gcc-headless`（Windows 对应 `windows-msvc-headless`）。Release 构建、`SYSTEM` 依赖模式与离线覆盖方法见[开发构建说明](docs/development/building.md)和[第三方依赖与锁定规则](docs/development/third-party.md)。

## 示例

推荐从 `rynui_token_gallery` 开始体验：它按七类 73 项提供独立组件页、真实交互样例、主题 / Compact / 品牌色 / 语言 / 方向实时切换，以及每个样例对应的 C++ 源码与 Design Token 视图。

| 可执行文件 | 说明 |
| --- | --- |
| `rynui_token_gallery` | 推荐入口：独立组件页、真实交互、主题与国际化切换、内联 C++ 源码与 Token 浏览 |
| `rynui_minimal` | 从响应式 `Signal` 到 GPU Quad 绘制的最小闭环 |
| `rynui_text_demo` | 中英文排版、系统字体回退与文本动态更新 |
| `rynui_button_demo` | Button 变体、状态切换、鼠标与键盘焦点交互 |
| `rynui_layout_demo` | `Flex` / `Space` 组合与响应式布局更新 |

示例源码位于 [`examples/`](examples)（另包含纯公开头示例 [`examples/i18n_app/`](examples/i18n_app) 与 [`examples/app_notice/`](examples/app_notice)），公开 C++ 头文件位于 [`include/ryn/`](include/ryn)。

## 文档导航

### 架构与核心合同

- [架构基线](docs/architecture.md)：设计目标、模块边界与长期技术决策（包含尚未实现的规划）。
- [Design Token 参考](docs/design-tokens.md)：Ant Design 6.6.5 的 1198 项锁定 Token 及支持分类。
- [Renderer 合同](docs/renderer-contract.md)：logical CPU scene、packed GPU ABI、资源上传事务与 backend 能力边界。
- [原生国际化](docs/i18n.md) · [ConfigProvider](docs/config-provider.md) · [App 服务](docs/app.md)：语言包、消息词典、默认属性继承与窗口级通知/弹层服务。
- [双向文本](docs/bidirectional-text.md)：Unicode 17 段落分析、混合脚本排版与视觉光标导航合同。
- [多 backend 与跨端调研](docs/research/multi-backend-portability.md)：已落地的可移植边界及桌面、移动端、Web 后续路线。

### 开发、构建与测试

- [开发构建说明](docs/development/building.md)：环境要求、正式 presets、Release 与 HEADLESS 构建。
- [第三方依赖与锁定规则](docs/development/third-party.md)：`BUNDLED`/`SYSTEM` 模式、版本与 SHA256 锁定、离线构建及许可证。
- [内嵌资源与示例部署](docs/resources.md)：内置 shader 资源嵌入、默认系统字体链与验收字体准备。
- [Gallery 组件页面](docs/development/gallery-pages.md)：页面宿主、状态草稿、源码面板与定向验收入口。
- [Web / native 自动差分测试](docs/development/demo-differential-testing.md)：锁定官方 Web 参考与自动化差分 runner。
- [VS Code 使用说明](docs/development/vscode.md) · [代码格式](docs/development/formatting.md)：IntelliSense 配置与 clang-format 22.x 规范。

### 进度与协作

- [组件收尾清单](docs/component-completion.md) · [未解决问题总表](docs/open-issues.md)：组件原生功能状态、已知问题与平台验证边界。
- [OpenSpec changes](openspec/changes) · [OpenSpec 开发工具](docs/development/openspec.md)：各项变更的范围、任务清单与验收摘要。
- [Agent 协作规则](AGENTS.md)：面向 coding agent 的仓库协作、验证与提交规则。

## 反馈与参与

欢迎通过 [GitHub Issues](https://github.com/helywin/RynUI/issues) 报告问题、交流桌面场景或提出组件需求。反馈运行问题时，请附上操作系统、构建 preset、窗口系统、GPU/驱动信息及最小复现步骤。详细技术文档目前以简体中文为主。
