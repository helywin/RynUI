# 构建 RynUI

RynUI 的正式构建统一使用仓库内的 `CMakePresets.json` 和 `Ninja Multi-Config`。不要使用 Visual Studio generator、MinGW 或手写 build directory 代替 preset 验收。

## 基础要求

- CMake 3.25 或更高版本。
- Ninja。
- 支持 C++20 的工具链。
- Windows：带有 x64 C++ tools 的 Visual Studio，编译器必须为 MSVC。
- Linux：GCC 或 Clang。
- 原生 `BUNDLED` 构建会下载锁定的 shader host tool 输入；`SYSTEM` 或交叉编译需要一份可在构建主机执行的 SDL_shadercross CLI。

`CMakeUserPresets.json` 已被忽略，可用于个人路径和本机 cache variable 覆盖；不要把这些值写入共享的 `CMakePresets.json`。

默认 preset 使用锁定归档的 `BUNDLED` 依赖模式。系统或 package-manager 提供的 SDL3 使用带 `-system` 后缀的 configure/build/test preset；完整规则与锁定值见 [第三方依赖](third-party.md)。

## Windows / MSVC

推荐从普通 PowerShell 运行仓库包装脚本。脚本只负责定位 Visual Studio、进入 x64 Developer Environment，随后仍通过 presets 完成全部操作：

```powershell
./scripts/build-windows.ps1 -Configuration Debug
./scripts/build-windows.ps1 -Configuration Release
```

使用已有 SDL3 CMake package：

```powershell
./scripts/build-windows.ps1 -Configuration Debug -DependencyMode SYSTEM `
  -Sdl3Root D:/deps/SDL3 `
  -ShadercrossExecutable D:/tools/shadercross.exe
```

需要丢弃旧 CMake cache 并重新探测工具链时，加上 `-Fresh`。

如果已经位于 Visual Studio Developer PowerShell，也可以直接执行：

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

根工程会检查 generator 和编译器。Windows 上没有解析到 MSVC，或 generator 不是 `Ninja Multi-Config` 时，configure 会立即失败。

## 离线 shader

`rynui_shaders` target 从单一 `shaders/quad.hlsl` 离线生成 vertex/fragment 的 DXIL 与 SPIR-V：

```powershell
cmake --build --preset windows-msvc-debug --target rynui_shaders
```

输出位于对应 build tree 的 `generated/shaders/`。原生 `BUNDLED` 模式会构建锁定的 host CLI；要使用已准备的工具，可以在任意模式传入：

```powershell
cmake --preset windows-msvc `
  -DRYNUI_SHADERCROSS_EXECUTABLE=D:/tools/shadercross.exe
```

configure 会执行 `shadercross --help` 验证 override。交叉编译不会尝试执行 target binary；未提供 host override 时会立即失败。

## Linux / GCC

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

把后缀换成 `release` 即可构建和测试 Release configuration。

## Linux / Clang

```bash
cmake --preset linux-clang
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

## 当前 targets

| Target | 职责 | 允许的直接依赖 |
|---|---|---|
| `rynui_reactive` | 响应运行时基础层 | 无 |
| `rynui_runtime` | 持久化 UI Runtime | `rynui_reactive` |
| `rynui_layout` | Layout 核心 | `rynui_runtime` |
| `rynui_graphics` | 平台无关图形层 | `rynui_layout` |
| `rynui` / `RynUI::RynUI` | 公开 facade | `rynui_graphics` |

CMake 在 configure 阶段核对这些直接依赖，并扫描 `include/ryn/`，阻止公开 API 提前出现通用 `Modifier` 类型。测试目标本身只依赖当前公开 facade，不链接 SDL3；示例应用也会检查自身没有链接仅供构建使用的 `SDL3_shadercross` library。
