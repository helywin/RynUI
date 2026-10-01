# 在 VS Code 中使用 RynUI

仓库自带 `.vscode/` 配置，克隆后打开工作区即可获得不报错的 C/C++ IntelliSense，不需要为编辑器再写一份 include 路径。本页说明配置来源、生效条件和排查方式。

## 配置来源

| 文件 | 作用 |
|---|---|
| `.vscode/c_cpp_properties.json` | IntelliSense 的 include 路径、宏、语言标准和 `compile_commands.json` 位置 |
| `.vscode/settings.json` | C/C++ 语言标准回退值、HLSL 关联、`out/` 的搜索与监视排除 |
| `.vscode/extensions.json` | 推荐安装 `ms-vscode.cpptools` |

`c_cpp_properties.json` 提供两个 configuration：

| configuration | 平台 | build tree |
|---|---|---|
| `Win32` | Windows / MSVC | `out/build/windows-msvc` |
| `Linux` | Linux / GCC 或 Clang | `out/build/linux-gcc` |

`Win32`、`Linux` 是 cpptools 的特殊名称，会按当前平台自动选中，不需要手动切换。

`c_cpp_properties.json` 只使用 `${workspaceFolder}` 相对路径，不写本机绝对工具链路径，因此可以安全提交。本机特有的值应与 `CMakeUserPresets.json` 遵循同一规则：放在个人设置里，不写进仓库文件。

## 为什么不列系统头文件

cpptools 会自己查询编译器来获得 MSVC STL、Windows SDK 或 Linux 系统头文件。`includePath` 只列项目自身的 `include/`、`src/` 和锁定的 `_deps` 第三方头文件即可。把系统包含目录写进 `includePath` 既容易过期，也会覆盖编译器的真实行为。

## compile_commands.json

`base` configure preset 设置 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`，所以任何 preset 都会在 build tree 根目录生成 `compile_commands.json`。至少要 configure 过一次，文件才存在：

```powershell
./scripts/build-windows.ps1 -Configuration Debug
```

或者已经位于 Visual Studio Developer PowerShell 时：

```powershell
cmake --preset windows-msvc
```

IntelliSense 对 `compile_commands.json` 中已有的源文件直接使用该文件的真实编译命令行；`includePath` 和 `defines` 是数据库中没有该文件时的回退，所以尚未 configure 的新 `.cpp` 同样能正确解析系统头文件与项目头文件。

## 修改依赖之后

新增或改动锁定的第三方依赖、调整 `target_include_directories` 之后，重新 configure 以刷新数据库，并把新的 include 目录补进 `c_cpp_properties.json` 的 `includePath`。判断真实路径的权威来源是构建树本身：

```powershell
Select-String -Path out/build/windows-msvc/CMakeFiles/impl-Debug.ninja -Pattern '^\s*INCLUDES = '
```

## 排查

- **C/C++: Log Diagnostics**：确认 cpptools 实际使用的 include 路径、宏和编译器。
- **C/C++: Reset IntelliSense Database**：配置改动后没有生效时重建数据库。
- **C/C++: Edit Configurations (JSON)**：打开 `.vscode/c_cpp_properties.json`。
- 状态栏的 configuration 名称应为当前平台对应的 `Win32` 或 `Linux`。

`linux-clang` preset 使用 `out/build/linux-clang`，需要在 `c_cpp_properties.json` 的 `Linux` configuration 中把 `compileCommands` 与 `includePath` 前缀改为该目录。

## 边界

仓库不在 `.vscode/` 中接管构建：正式构建仍由 `CMakePresets.json` 驱动，Windows 需要通过 `scripts/build-windows.ps1` 或 Developer Environment 进入 x64 MSVC 环境（见[开发构建说明](building.md)）。编辑器内联 configure 不是验收路径，也不要把 CMake Tools 配置为 IntelliSense 的 `configurationProvider`，否则 configure 失败时 IntelliSense 会一起失效。

## 验证状态

Windows 上已验证 `Win32` configuration 与 `windows-msvc` preset 的 include 路径、宏和语言标准一致。`Linux` configuration 由同名 preset 和相同 FetchContent 目录推导，尚未在 Linux 上验收。
